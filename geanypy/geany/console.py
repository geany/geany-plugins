#!/usr/bin/env python
#
#  pyconsole.py
#
#  Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
#  Thanks to Geoffrey French for ideas.
#
#  This file is part of medit.  medit is free software; you can
#  redistribute it and/or modify it under the terms of the
#  GNU Lesser General Public License as published by the
#  Free Software Foundation; either version 2.1 of the License,
#  or (at your option) any later version.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with medit.  If not, see <http://www.gnu.org/licenses/>.
#

# This module 'runs' python interpreter in a TextView widget.
# The main class is Console, usage is:
# Console(locals=None, banner=None, completer=None, use_rlcompleter=True, start_script='') -
# it creates the widget and 'starts' interactive session; see the end of
# this file. If start_script is not empty, it pastes it as it was entered from keyboard.
#
# Console has "command" signal which is emitted when code is about to
# be executed. You may connect to it using console.connect or console.connect_after
# to get your callback ran before or after the code is executed.
#
# To modify output appearance, set attributes of console.stdout_tag and
# console.stderr_tag.
#
# Console may subclass a type other than gtk.TextView, to allow syntax highlighting and stuff,
# e.g.:
#   console_type = pyconsole.ConsoleType(moo.TextView)
#   console = console_type(use_rlcompleter=False, start_script="import moo\nimport gtk\n")
#
# This widget is not a replacement for real terminal with python running
# inside: GtkTextView is not a terminal.
# The use case is: you have a python program, you create this widget,
# and inspect your program interiors.

import gtk
import gtk.gdk as gdk
import gobject
import pango
import gtk.keysyms as _keys
import code
import sys
import keyword
import re

# commonprefix() from posixpath
def _commonprefix(m):
    "Given a list of pathnames, returns the longest common leading component"
    if not m: return ''
    prefix = m[0]
    for item in m:
        for i in range(len(prefix)):
            if prefix[:i+1] != item[:i+1]:
                prefix = prefix[:i]
                if i == 0:
                    return ''
                break
    return prefix

class _ReadLine(object):

    class Output(object):
        def __init__(self, console, tag_name):
            object.__init__(self)
            self.buffer = console.get_buffer()
            self.tag_name = tag_name
        def write(self, text):
            pos = self.buffer.get_iter_at_mark(self.buffer.get_insert())
            self.buffer.insert_with_tags_by_name(pos, text, self.tag_name)

    class History(object):
        def __init__(self):
            object.__init__(self)
            self.items = ['']
            self.ptr = 0
            self.edited = {}

        def commit(self, text):
            if text and self.items[-1] != text:
                self.items.append(text)
            self.ptr = 0
            self.edited = {}

        def get(self, dir, text):
            if len(self.items) == 1:
                return None

            if text != self.items[self.ptr]:
                self.edited[self.ptr] = text
            elif self.edited.has_key(self.ptr):
                del self.edited[self.ptr]

            self.ptr = self.ptr + dir
            if self.ptr >= len(self.items):
                self.ptr = 0
            elif self.ptr < 0:
                self.ptr = len(self.items) - 1

            try:
                return self.edited[self.ptr]
            except KeyError:
                return self.items[self.ptr]

    def __init__(self):
        object.__init__(self)

        self.set_wrap_mode(gtk.WRAP_CHAR)
        self.modify_font(pango.FontDescription("Monospace"))

        self.buffer = self.get_buffer()
        self.buffer.connect("insert-text", self.on_buf_insert)
        self.buffer.connect("delete-range", self.on_buf_delete)
        self.buffer.connect("mark-set", self.on_buf_mark_set)
        self.do_insert = False
        self.do_delete = False

        self.stdout_tag = self.buffer.create_tag("stdout", foreground="#006000")
        self.stderr_tag = self.buffer.create_tag("stderr", foreground="#B00000")
        self._stdout = _ReadLine.Output(self, "stdout")
        self._stderr = _ReadLine.Output(self, "stderr")

        self.cursor = self.buffer.create_mark("cursor",
                                              self.buffer.get_start_iter(),
                                              False)
        insert = self.buffer.get_insert()
        self.cursor.set_visible(True)
        insert.set_visible(False)

        self.ps = ''
        self.in_raw_input = False
        self.run_on_raw_input = None
        self.tab_pressed = 0
        self.history = _ReadLine.History()
        self.nonword_re = re.compile("[^\w\._]")

    def freeze_undo(self):
        try: self.begin_not_undoable_action()
        except: pass

    def thaw_undo(self):
        try: self.end_not_undoable_action()
        except: pass

    def raw_input(self, ps=None):
        if ps:
            self.ps = ps
        else:
            self.ps = ''

        iter = self.buffer.get_iter_at_mark(self.buffer.get_insert())

        if ps:
            self.freeze_undo()
            self.buffer.insert(iter, self.ps)
            self.thaw_undo()

        self.__move_cursor_to(iter)
        self.scroll_to_mark(self.cursor, 0.2)

        self.in_raw_input = True

        if self.run_on_raw_input:
            run_now = self.run_on_raw_input
            self.run_on_raw_input = None
            self.buffer.insert_at_cursor(run_now + '\n')

    def on_buf_mark_set(self, buffer, iter, mark):
        if mark is not buffer.get_insert():
            return
        start = self.__get_start()
        end = self.__get_end()
        if iter.compare(self.__get_start()) >= 0 and \
           iter.compare(self.__get_end()) <= 0:
                buffer.move_mark_by_name("cursor", iter)
                self.scroll_to_mark(self.cursor, 0.2)

    def __insert(self, iter, text):
        self.do_insert = True
        self.buffer.insert(iter, text)
        self.do_insert = False

    def on_buf_insert(self, buf, iter, text, len):
        if not self.in_raw_input or self.do_insert or not len:
            return
        buf.stop_emission("insert-text")
        lines = text.splitlines()
        need_eol = False
        for l in lines:
            if need_eol:
                self._commit()
                iter = self.__get_cursor()
            else:
                cursor = self.__get_cursor()
                if iter.compare(self.__get_start()) < 0:
                    iter = cursor
                elif iter.compare(self.__get_end()) > 0:
                    iter = cursor
                else:
                    self.__move_cursor_to(iter)
            need_eol = True
            self.__insert(iter, l)
        self.__move_cursor(0)

    def __delete(self, start, end):
        self.do_delete = True
        self.buffer.delete(start, end)
        self.do_delete = False

    def on_buf_delete(self, buf, start, end):
        if not self.in_raw_input or self.do_delete:
            return

        buf.stop_emission("delete-range")

        start.order(end)
        line_start = self.__get_start()
        line_end = self.__get_end()

        if start.compare(line_end) > 0:
            return
        if end.compare(line_start) < 0:
            return

        self.__move_cursor(0)

        if start.compare(line_start) < 0:
            start = line_start
        if end.compare(line_end) > 0:
            end = line_end
        self.__delete(start, end)

    def do_key_press_event(self, event, parent_type):
        if not self.in_raw_input:
            return parent_type.do_key_press_event(self, event)

        tab_pressed = self.tab_pressed
        self.tab_pressed = 0
        handled = True

        state = event.state & (gdk.SHIFT_MASK |
                                gdk.CONTROL_MASK |
                                gdk.MOD1_MASK)
        keyval = event.keyval

        if not state:
            if keyval == _keys.Return:
                self._commit()
            elif keyval == _keys.Up:
                self.__history(-1)
            elif keyval == _keys.Down:
                self.__history(1)
            elif keyval == _keys.Left:
                self.__move_cursor(-1)
            elif keyval == _keys.Right:
                self.__move_cursor(1)
            elif keyval == _keys.Home:
                self.__move_cursor(-10000)
            elif keyval == _keys.End:
                self.__move_cursor(10000)
            elif keyval == _keys.Tab:
                cursor = self.__get_cursor()
                if cursor.starts_line():
                    handled = False
                else:
                    cursor.backward_char()
                    if cursor.get_char().isspace():
                        handled = False
                    else:
                        self.tab_pressed = tab_pressed + 1
                        self.__complete()
            else:
                handled = False
        elif state == gdk.CONTROL_MASK:
            if keyval == _keys.u:
                start = self.__get_start()
                end = self.__get_cursor()
                self.__delete(start, end)
            else:
                handled = False
        else:
            handled = False

        if not handled:
            return parent_type.do_key_press_event(self, event)
        else:
            return True

    def __history(self, dir):
        text = self._get_line()
        new_text = self.history.get(dir, text)
        if not new_text is None:
            self.__replace_line(new_text)
        self.__move_cursor(0)
        self.scroll_to_mark(self.cursor, 0.2)

    def __get_cursor(self):
        return self.buffer.get_iter_at_mark(self.cursor)
    def __get_start(self):
        iter = self.__get_cursor()
        iter.set_line_offset(len(self.ps))
        return iter
    def __get_end(self):
        iter = self.__get_cursor()
        if not iter.ends_line():
            iter.forward_to_line_end()
        return iter

    def __get_text(self, start, end):
        return self.buffer.get_text(start, end, False)

    def __move_cursor_to(self, iter):
        self.buffer.place_cursor(iter)
        self.buffer.move_mark_by_name("cursor", iter)

    def __move_cursor(self, howmany):
        iter = self.__get_cursor()
        end = self.__get_cursor()
        if not end.ends_line():
            end.forward_to_line_end()
        line_len = end.get_line_offset()
        move_to = iter.get_line_offset() + howmany
        move_to = min(max(move_to, len(self.ps)), line_len)
        iter.set_line_offset(move_to)
        self.__move_cursor_to(iter)

    def __delete_at_cursor(self, howmany):
        iter = self.__get_cursor()
        end = self.__get_cursor()
        if not end.ends_line():
            end.forward_to_line_end()
        line_len = end.get_line_offset()
        erase_to = iter.get_line_offset() + howmany
        if erase_to > line_len:
            erase_to = line_len
        elif erase_to < len(self.ps):
            erase_to = len(self.ps)
        end.set_line_offset(erase_to)
        self.__delete(iter, end)

    def __get_width(self):
        if not (self.flags() & gtk.REALIZED):
            return 80
        layout = pango.Layout(self.get_pango_context())
        letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
        layout.set_text(letters)
        pix_width = layout.get_pixel_size()[0]
        return self.allocation.width * len(letters) / pix_width

    def __print_completions(self, completions):
        line_start = self.__get_text(self.__get_start(), self.__get_cursor())
        line_end = self.__get_text(self.__get_cursor(), self.__get_end())
        iter = self.buffer.get_end_iter()
        self.__move_cursor_to(iter)
        self.__insert(iter, "\n")

        width = max(self.__get_width(), 4)
        max_width = max([len(s) for s in completions])
        n_columns = max(int(width / (max_width + 1)), 1)
        col_width = int(width / n_columns)
        total = len(completions)
        col_length = total / n_columns
        if total % n_columns:
            col_length = col_length + 1
        col_length = max(col_length, 1)

        if col_length == 1:
            n_columns = total
            col_width = width / total

        for i in range(col_length):
            for j in range(n_columns):
                ind = i + j*col_length
                if ind < total:
                    if j == n_columns - 1:
                        n_spaces = 0
                    else:
                        n_spaces = col_width - len(completions[ind])
                    self.__insert(iter, completions[ind] + " " * n_spaces)
            self.__insert(iter, "\n")

        self.__insert(iter, "%s%s%s" % (self.ps, line_start, line_end))
        iter.set_line_offset(len(self.ps) + len(line_start))
        self.__move_cursor_to(iter)
        self.scroll_to_mark(self.cursor, 0.2)

    def __complete(self):
        text = self.__get_text(self.__get_start(), self.__get_cursor())
        start = ''
        word = text
        nonwords = self.nonword_re.findall(text)
        if nonwords:
            last = text.rfind(nonwords[-1]) + len(nonwords[-1])
            start = text[:last]
            word = text[last:]

        completions = self.complete(word)

        if completions:
            prefix = _commonprefix(completions)
            if prefix != word:
                start_iter = self.__get_start()
                start_iter.forward_chars(len(start))
                end_iter = start_iter.copy()
                end_iter.forward_chars(len(word))
                self.__delete(start_iter, end_iter)
                self.__insert(end_iter, prefix)
            elif self.tab_pressed > 1:
                self.freeze_undo()
                self.__print_completions(completions)
                self.thaw_undo()
                self.tab_pressed = 0

    def complete(self, text):
        return None

    def _get_line(self):
        start = self.__get_start()
        end = self.__get_end()
        return self.buffer.get_text(start, end, False)

    def __replace_line(self, new_text):
        start = self.__get_start()
        end = self.__get_end()
        self.__delete(start, end)
        self.__insert(end, new_text)

    def _commit(self):
        end = self.__get_cursor()
        if not end.ends_line():
            end.forward_to_line_end()
        text = self._get_line()
        self.__move_cursor_to(end)
        self.freeze_undo()
        self.__insert(end, "\n")
        self.in_raw_input = False
        self.history.commit(text)
        self.do_raw_input(text)
        self.thaw_undo()

    def do_raw_input(self, text):
        pass


class _Console(_ReadLine, code.InteractiveInterpreter):
    def __init__(self, locals=None, banner=None,
                 completer=None, use_rlcompleter=True,
                 start_script=None):
        _ReadLine.__init__(self)


        code.InteractiveInterpreter.__init__(self, locals)
        self.locals["__console__"] = self

        self.start_script = start_script
        self.completer = completer
        self.banner = banner

        if not self.completer and use_rlcompleter:
            try:
                import rlcompleter
                self.completer = rlcompleter.Completer()
            except ImportError:
                pass

        self.ps1 = ">>> "
        self.ps2 = "... "
        self.__start()
        self.run_on_raw_input = start_script
        self.raw_input(self.ps1)

    def __start(self):
        self.cmd_buffer = ""

        self.freeze_undo()
        self.thaw_undo()
        self.buffer.set_text("")

        if self.banner:
            iter = self.buffer.get_start_iter()
            self.buffer.insert_with_tags_by_name(iter, self.banner, "stdout")
            if not iter.starts_line():
                self.buffer.insert(iter, "\n")

    def clear(self, start_script=None):
        if start_script is None:
            start_script = self.start_script
        else:
            self.start_script = start_script

        self.__start()
        self.run_on_raw_input = start_script

    def do_raw_input(self, text):
        if self.cmd_buffer:
            cmd = self.cmd_buffer + "\n" + text
        else:
            cmd = text

        saved_stdout, saved_stderr = sys.stdout, sys.stderr
        sys.stdout, sys.stderr = self._stdout, self._stderr

        if self.runsource(cmd):
            self.cmd_buffer = cmd
            ps = self.ps2
        else:
            self.cmd_buffer = ''
            ps = self.ps1

        sys.stdout, sys.stderr = saved_stdout, saved_stderr
        self.raw_input(ps)

    def do_command(self, code):
        try:
            eval(code, self.locals)
        except SystemExit:
            raise
        except:
            self.showtraceback()

    def runcode(self, code):
        if gtk.pygtk_version[1] < 8:
            self.do_command(code)
        else:
            self.emit("command", code)

    def exec_command(self, command):
        if self._get_line():
            self._commit()
        self.buffer.insert_at_cursor(command)
        self._commit()

    def complete_attr(self, start, end):
        try:
            obj = eval(start, self.locals)
            strings = dir(obj)

            if end:
                completions = {}
                for s in strings:
                    if s.startswith(end):
                        completions[s] = None
                completions = completions.keys()
            else:
                completions = strings

            completions.sort()
            return [start + "." + s for s in completions]
        except:
            return None

    def complete(self, text):
        if self.completer:
            completions = []
            i = 0
            try:
                while 1:
                    s = self.completer.complete(text, i)
                    if s:
                        completions.append(s)
                        i = i + 1
                    else:
                        completions.sort()
                        return completions
            except NameError:
                return None

        dot = text.rfind(".")
        if dot >= 0:
            return self.complete_attr(text[:dot], text[dot+1:])

        completions = {}
        strings = keyword.kwlist

        if self.locals:
            strings.extend(self.locals.keys())

        try: strings.extend(eval("globals()", self.locals).keys())
        except: pass

        try:
            exec "import __builtin__" in self.locals
            strings.extend(eval("dir(__builtin__)", self.locals))
        except:
            pass

        for s in strings:
            if s.startswith(text):
                completions[s] = None
        completions = completions.keys()
        completions.sort()
        return completions


def ReadLineType(t=gtk.TextView):
    class readline(t, _ReadLine):
        def __init__(self, *args, **kwargs):
            t.__init__(self)
            _ReadLine.__init__(self, *args, **kwargs)
        def do_key_press_event(self, event):
            return _ReadLine.do_key_press_event(self, event, t)
    gobject.type_register(readline)
    return readline

def ConsoleType(t=gtk.TextView):
    class console_type(t, _Console):
        __gsignals__ = {
            'command' : (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (object,)),
            'key-press-event' : 'override'
          }

        def __init__(self, *args, **kwargs):
            if gtk.pygtk_version[1] < 8:
                gobject.GObject.__init__(self)
            else:
                t.__init__(self)
            _Console.__init__(self, *args, **kwargs)

        def do_command(self, code):
            return _Console.do_command(self, code)

        def do_key_press_event(self, event):
            return _Console.do_key_press_event(self, event, t)

    if gtk.pygtk_version[1] < 8:
        gobject.type_register(console_type)

    return console_type

ReadLine = ReadLineType()
Console = ConsoleType()

def _create_widget(start_script):

    console = Console(banner="Geany Python Console",
                          use_rlcompleter=False,
                          start_script=start_script)
    return console

def _make_window(start_script="import geany\n"):
    window = gtk.Window()
    window.set_title("Python Console")
    swin = gtk.ScrolledWindow()
    swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_ALWAYS)
    window.add(swin)
    console = _create_widget(start_script)
    swin.add(console)
    window.set_default_size(500, 400)
    window.show_all()

    if not gtk.main_level():
        window.connect("destroy", gtk.main_quit)
        gtk.main()

    return console

if __name__ == '__main__':
    import sys
    import os
    sys.path.insert(0, os.getcwd())
    _make_window(sys.argv[1:] and '\n'.join(sys.argv[1:]) + '\n' or None)
