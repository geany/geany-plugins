# © Copyright 2016  xiota «xiota•mentalfossa•com»
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

__PLUGIN_NAME__ = 'Preview'
__PLUGIN_DESC__ = 'Preview documents written in various markup languages.'
__PLUGIN_VERS__ = '20161217.185107'
__PLUGIN_AUTH__ = 'xiota «xiota•mentalfossa•com»'

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

import geany
import gtk, gobject
import webkit, email

import time, subprocess
import os, tempfile
import cgi, re

import screenplain.parsers.fountain
import screenplain.export.html

from ConfigParser import SafeConfigParser

try:
	from StringIO import StringIO
except ImportError:
	from io import StringIO

# Import find-executable engine
try:
	from shutil import which
except ImportError:
	from distutils.spawn import find_executable as which

# Path to the executables
PANDOC_PATH = which('pandoc')
ASCIIDOC_PATH = which('asciidoc')
ASCIIDOCTOR_PATH = which('asciidoctor')
UNRTF_PATH = which('unrtf')

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class PreviewPlugin(geany.Plugin):
	__plugin_name__        = __PLUGIN_NAME__
	__plugin_description__ = __PLUGIN_DESC__
	__plugin_version__     = __PLUGIN_VERS__
	__plugin_author__      = __PLUGIN_AUTH__

	# default settings
	_default_filetype = 'plain'
	_update_interval = '1.0'

	_preview_position = 'sidebar'
	_message_position = 'messages_tab'
	_hide_menubar = 'False'

	_asciidoc_processor = 'asciidoctor'


	def __init__(self):
		geany.Plugin.__init__(self)

		# register keyboard shortcuts
		self.kb_actions = [
			('toggle_menu', 'Toggle Menu')
		]

		preview_key = self.set_key_group('Preview', len(self.kb_actions), self.on_kb_activate)

		for key_id in range(len(self.kb_actions)):
			preview_key.add_key_item(key_id=key_id, key=0, mod=0,
				name=self.kb_actions[key_id][0], label=self.kb_actions[key_id][1])

		# variables to save scrollbar position
		self.hadj = None
		self.vadj = None
		self.hadj_save = 0
		self.vadj_save = 0

		# variables to track update_interval
		self.update_time = 0
		self.update_request_time = time.time()

		self.load_config()
		self.install_preview()

		geany.signals.connect('editor-notify', self.on_editor_notify)
		geany.signals.connect('document-save', self.on_document_activity)
		geany.signals.connect('document-activate', self.on_document_activity)
		geany.signals.connect('document-reload', self.on_document_activity)
		geany.signals.connect('document-filetype-set', self.on_document_activity)

		geany.signals.connect('geany-startup-complete', self.on_document_activity)
		geany.signals.connect('document-open', self.on_document_open)
		geany.signals.connect('document-new', self.on_document_new)
		geany.signals.connect('document-close', self.on_document_close)

	def cleanup(self):
		self.on_save_config_timeout()
		self.uninstall_preview()


	def load_config(self):
		self.cfg_path = os.path.join(geany.app.configdir, "plugins", "preview.conf")
		self.cfg = SafeConfigParser()
		self.cfg.read(self.cfg_path)

	def save_config(self):
		gobject.idle_add(self.on_save_config_timeout)

	def on_save_config_timeout(self, data=None):
		self.cfg.write(open(self.cfg_path, 'w'))
		return False


	def install_preview(self):
		# load general settings
		self.default_filetype = self.default_filetype
		self.update_interval = self.update_interval
		self.asciidoc_processor = self.asciidoc_processor

		# load appearance settings
		self.preview_position = self.preview_position
		self.message_position = self.message_position
		self.hide_menubar = self.hide_menubar

		# hide menubar
		try:
			# if toolbar appended to menubar
			self.menubar, toolbar = geany.main_widgets.toolbar.parent.get_children()
		except:
			self.menubar, toolbar, notebook, statusbar = geany.main_widgets.toolbar.parent.get_children()

		if self.hide_menubar == "True":
			self.menubar.set_visible(False)

		# create preview window
		self.preview_box = gtk.VBox()
		box = gtk.HBox()

		self.addressbar = gtk.Entry()
		self.addressbar.connect('activate', self.on_address_action)

#		button1 = gtk.Button('')
#		button1.connect('clicked', self.on_address_action)
#		img = gtk.image_new_from_icon_name('forward', gtk.ICON_SIZE_BUTTON)
#		button1.set_image(img)

		button2 = gtk.Button('')
		button2.connect('clicked', self.on_toggle_menu_action)
		img = gtk.image_new_from_icon_name('open-menu-symbolic', gtk.ICON_SIZE_BUTTON)
		button2.set_image(img)

		box.pack_start(self.addressbar)
#		box.pack_start(button1, False)
		box.pack_start(button2, False)

		swin = gtk.ScrolledWindow()

		self.web = webkit.WebView()

		self.web.connect('load-committed', self.on_load_committed)
		self.web.connect('load-started', self.on_load_started)
		self.web.connect('load-finished', self.on_load_finished)
		self.web.connect('context-menu', self.on_context_menu)

		self.preview_box.pack_start(box, False)
		self.preview_box.pack_start(swin)

		swin.add(self.web)

		if self.preview_position == 'sidebar':
			self.notebook = geany.main_widgets.sidebar_notebook
		else:
			self.notebook = geany.main_widgets.message_window_notebook

		self.notebook.append_page(self.preview_box, gtk.Label('Preview'))
		self.notebook.connect('show', self.on_notebook_show)

		self.preview_box.show_all()

		# set scrollbar objects for position save/restore
		self.hadj = swin.get_hadjustment()
		self.vadj = swin.get_vadjustment()

	def uninstall_preview(self):
		self.preview_box.destroy()

	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	# Callbacks - Web Browsing
	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	def on_address_action(self, widget):
		address = self.addressbar.get_text()

		if address == '':
			self.on_document_activity()
		else:
			if not (('://' in address) or address.startswith('/')):
				address = 'http://' + address
				self.addressbar.set_text(address)

			self.web.open(address)

	def on_load_committed(self, view, frame):
		address = self.addressbar.get_text()

		if address != '':
			uri = frame.get_uri()
			if uri == None:
				uri = ''

			self.addressbar.set_text(uri)

	def on_load_started(self, view, frame):
		self.save_scrollbar_position()

	def on_load_finished(self, view, frame):
		self.restore_scrollbar_position()

	def on_context_menu(self, webview, context_menu, hit_result_event, event):
		for option in context_menu.get_children():
			if option.get_label() in ['_Back', '_Forward', '_Stop', '_Reload']:
				context_menu.remove(option)

		if context_menu.get_children() != []:
			option = gtk.SeparatorMenuItem()
			context_menu.append(option)

		# ~~~~~~~~~~

		option = gtk.ImageMenuItem('Reload')
		img = gtk.image_new_from_icon_name('reload', gtk.ICON_SIZE_BUTTON)
		option.set_image(img)
		option.connect('activate', self.on_document_activity)
		context_menu.append(option)

		# ~~~~~~~~~~

		option = gtk.SeparatorMenuItem()
		context_menu.append(option)

		# ~~~~~~~~~~

		option = gtk.ImageMenuItem('Sync Position')

		img = gtk.image_new_from_icon_name('emblem-synchronizing-symbolic', gtk.ICON_SIZE_BUTTON)
		option.set_image(img)
		option.connect('activate', self.on_sync_view_action)
		context_menu.append(option)

		# ~~~~~~~~~~

		option = gtk.SeparatorMenuItem()
		context_menu.append(option)

		# ~~~~~~~~~~

		option = gtk.ImageMenuItem('Save As...')
		img = gtk.image_new_from_icon_name('document-save-as-symbolic', gtk.ICON_SIZE_BUTTON)
		option.set_image(img)
		option.connect('activate', self.option_save_source)
		context_menu.append(option)

		# ~~~~~~~~~~

		context_menu.show_all()

	def option_save_source(self, image_menu_item):
		doc = geany.document.get_current()

		# create a filechooserdialog to save
		# arguments: title, parent_window, action, (buttons, response)
		save_dialog = gtk.FileChooserDialog("Save As...", geany.main_widgets.window,
			gtk.FILE_CHOOSER_ACTION_SAVE,
			(gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
			gtk.STOCK_SAVE, gtk.RESPONSE_ACCEPT))

		save_dialog.set_do_overwrite_confirmation(True)
		save_dialog.set_modal(True)

		#save_dialog.set_filename(doc.file_name)
		save_dialog.set_current_folder(os.path.dirname(doc.file_name))
		save_dialog.set_current_name(doc.basename_for_display + '.html')

		save_dialog.connect("response", self.save_source_response)
		save_dialog.show()

	def save_source_response(self, dialog, response_id):
		save_dialog = dialog

		if response_id == gtk.RESPONSE_ACCEPT:
			with open(save_dialog.get_filename(), 'w') as file:
				text = self.web.get_main_frame().get_data_source().get_data()
				file.write(text)
		elif response_id == gtk.RESPONSE_CANCEL:
			geany_message("canceled: Save As...")

		dialog.destroy()


	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	# Callbacks - Menubar and Notebook Visibility
	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	def on_kb_activate(self, keyid = 0, data = None):
		if self.kb_actions[keyid][0] == 'toggle_menu':
			self.on_toggle_menu_action()

	def on_toggle_menu_action(self, widget = None):
		if self.menubar.get_visible():
			self.menubar.set_visible(False)
		else:
			self.menubar.set_visible(True)

	def on_sync_view_action(self, widget = None):
		if self.vadj.upper > 0.0:
			current_position = geany.document.get_current().editor.scintilla.get_current_position()
			length = geany.document.get_current().editor.scintilla.get_length()

			self.vadj_save = self.vadj.upper*current_position/length
			self.vadj.value = self.vadj_save

	def on_notebook_show(self, widget = None):
		self.on_document_activity()

	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	# Callbacks - Document Activity
	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	def on_document_activity(self, obj = None, doc = None, filetype_old = None):
		if not self.notebook.get_visible():
			return False

		text = self.get_document_text()
		file_name = self.get_document_file_name()
		file_ext = self.get_document_file_ext()
		file_type = self.get_document_file_type()

		output = self.preview_process(text, file_type, file_name, file_ext)

		uri = 'file://' + file_name + file_ext

		self.preview_update(content=output, uri=uri)
		self.update_time = time.time()

	def on_document_open(self, obj = None, doc = None):
		self.on_document_activity()

	def on_document_new(self, obj = None, doc = None):
		self.on_document_activity()

	def on_document_close(self, obj = None, doc = None):
		# signal sent before document close
		# closed document will still be in list
		if len(geany.document.get_documents_list()) <= 1:
			self.preview_update()
		else:
			self.on_document_activity()

	def on_editor_notify(self, obj, editor, notification):
		file_type = self.get_document_file_type()

		if file_type in ['html', 'plain']:
			update_interval = 0.10
		else:
			update_interval = float(self.update_interval)

		elapsed_interval = time.time() - self.update_time

		# process first document when geany first started (update_time == 0)
		if ( (notification.modification_type & geany.scintilla.MODIFIED)
				or (notification.modification_type & geany.scintilla.MOD_INSERT_TEXT)
				or (notification.modification_type & geany.scintilla.MOD_DELETE_TEXT)
				):
			if elapsed_interval > update_interval:
				self.on_document_activity()
			else:
				self.update_request_time = time.time()
		elif (elapsed_interval > update_interval
				and self.update_request_time > self.update_time
				):
			self.on_document_activity()

	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	# preview processing and update functions
	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	def preview_process(self, text, file_type, file_name='', file_ext=''):
		address = self.addressbar.get_text()
		if address != '':
			return False

		headers = ''
		body = None
		pandoc_type = None

		# detect RFC-822-like email messages
		eml = email.message_from_string(text)

		if (eml['from'] != None
				or eml['subject'] != None
				or eml['date'] != None
				or eml['to'] != None):

			headers2 = []
			for header in eml._headers:
				headers2.append(': '.join(header))

			headers = "\n".join(headers2)
			headers = '<pre class="header">' + headers + '</pre>'

			# get content type from headers...
			# overrides file_type provided by geany
			if eml['content-type'] == None:
				content_type = file_type
			else:
				content_type = eml['content-type']

			if 'html' in content_type:
				file_type = 'html'
			elif 'markdown' in content_type:
				file_type = 'markdown'
			elif 'plain' in content_type:
				file_type = 'plain'
			#else:
			#	file_type already assigned
			#		based on extension
			#		or provided by geany

			text = eml._payload

		# process into html for preview
		if file_type == 'html' or text == '':
			body = html_wrapper(text)
		elif file_type in ['markdown', 'latex', 'docbook', 'textile', 'org']:
			pandoc_type = file_type
			body = pandoc(text, pandoc_type)
		elif file_type in ['restructuredtext', 'rst']:
			pandoc_type = 'rst'
			body = pandoc(text, pandoc_type)
		elif file_type in ['txt2tags', 't2t']:
			pandoc_type = 't2t'
			body = pandoc(text, pandoc_type)
		elif file_type == 'asciidoc':
			body = asciidoc(text, self.asciidoc_processor)
		elif file_type == 'fountain':
			body = fountain(text)
		elif file_type == 'rtf':
			body = unrtf(text)
		else:
			# mimetype = 'text/plain'
			body = plain_wrapper(text)

		css = self.get_css(file_type)

		content = '<html>'
		content += '<body>'
		content += '<head>'
		content += css
		content += '</head>'
		content += headers + body
		content += '</body>'
		content += '</html>'

		return content


	def get_document_text(self):
		doc = geany.document.get_current()

		if doc != None:
			text = doc.editor.scintilla.get_contents()
		else:
			text = ''

		return text

	def get_document_file_name(self):
		doc = geany.document.get_current()

		if doc != None and doc.file_name != None:
			file_name, file_ext = os.path.splitext(doc.file_name)
		else:
			file_name = ''

		return file_name

	def get_document_file_ext(self):
		doc = geany.document.get_current()

		if doc != None and doc.file_name != None:
			file_name, file_ext = os.path.splitext(doc.file_name)
		else:
			file_ext = ''

		return file_ext

	def get_document_file_type(self):
		doc = geany.document.get_current()

		if doc != None:
			# file_type provided by geany
			file_type = doc.file_type.name.lower()

			file_ext = self.get_document_file_ext()
			# guess file_type from extension
			# overrides file_type provided by geany
			if file_ext == '.textile':
				file_type = 'textile'
			elif file_ext == '.org':
				file_type = 'org'
			elif file_ext == '.rtf':
				file_type = 'rtf'
			elif file_ext in ['.fountain', '.spmd']:
				file_type = 'fountain'
			elif file_type == 'none':
				file_type = self.default_filetype
		else:
			file_type = self.default_filetype

		return file_type

	def get_css(self, file_type):
		css_type = os.path.join(os.path.dirname(__file__), 'preview-' + file_type + '.css')
		css_html = os.path.join(os.path.dirname(__file__), 'preview-html.css')

		if os.path.isfile(css_type):
			css_file = css_type
		elif os.path.isfile(css_html):
			css_file = css_html

		try:
			with open(css_file, 'r') as file:
				css_content = '<style type="text/css">'
				css_content += file.read()
				css_content += '</style>'
		except:
			css_content = ''

		return css_content


	def preview_update(self, content=' ', mimetype = 'text/html', encoding = 'utf-8', uri = ''):
		self.web.load_string(content, mimetype, encoding, uri)

	def save_scrollbar_position(self):
		self.hadj_save = self.hadj.value
		self.vadj_save = self.vadj.value

	def restore_scrollbar_position(self):
		self.hadj.value = self.hadj_save
		self.vadj.value = self.vadj_save

	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	# Configuration Options
	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	def on_default_filetype_changed(self, widget, data=None):
		model = widget.get_model()
		index = widget.get_active()

		if model[index][0] != None:
			self.default_filetype = model[index][0]
		else:
			self.default_filetype = 'plain'

	def _get_default_filetype(self):
		if self.cfg.has_section('general'):
			if self.cfg.has_option('general', 'default_filetype'):
				return self.cfg.get('general', 'default_filetype')
		return self._default_filetype

	def _set_default_filetype(self, default_filetype):
		self._default_filetype = str(default_filetype)
		if not self.cfg.has_section('general'):
			self.cfg.add_section('general')
		self.cfg.set('general', 'default_filetype', self._default_filetype)
		self.save_config()

	default_filetype = property(_get_default_filetype, _set_default_filetype)

	# ~~~~~~~~~~

	def on_update_interval_changed(self, text_buf, data=None):
		self.update_interval = text_buf.get_text()

	def _get_update_interval(self):
		if self.cfg.has_section('general'):
			if self.cfg.has_option('general', 'update_interval'):
				return self.cfg.get('general', 'update_interval')
		return self._update_interval

	def _set_update_interval(self, update_interval):
		self._update_interval = str(update_interval)
		if not self.cfg.has_section('general'):
			self.cfg.add_section('general')
		self.cfg.set('general', 'update_interval', self._update_interval)
		self.save_config()

	update_interval = property(_get_update_interval, _set_update_interval)

	# ~~~~~~~~~~

	def on_preview_position_changed(self, widget, data=None):
		model = widget.get_model()
		index = widget.get_active()

		if model[index][0] == 'Sidebar':
			self.preview_position = 'sidebar'
		elif model[index][0] == 'Message Window':
			self.preview_position = 'message_window'

	def _get_preview_position(self):
		if self.cfg.has_section('appearance'):
			if self.cfg.has_option('appearance', 'preview_position'):
				return self.cfg.get('appearance', 'preview_position')
		return self._preview_position

	def _set_preview_position(self, preview_position):
		self._preview_position = preview_position
		if not self.cfg.has_section('appearance'):
			self.cfg.add_section('appearance')
		self.cfg.set('appearance', 'preview_position', self._preview_position)
		self.save_config()

	preview_position = property(_get_preview_position, _set_preview_position)

	# ~~~~~~~~~~

	def on_message_position_changed(self, widget, data=None):
		model = widget.get_model()
		index = widget.get_active()

		if model[index][0] == 'Status Tab':
			self.message_position = 'status_tab'
		elif model[index][0] == 'Compiler Tab':
			self.message_position = 'compiler_tab'
		elif model[index][0] == 'Messages Tab':
			self.message_position = 'messages_tab'
		elif model[index][0] == 'Standard Output':
			self.message_position = 'stdout'

	def _get_message_position(self):
		if self.cfg.has_section('appearance'):
			if self.cfg.has_option('appearance', 'message_position'):
				return self.cfg.get('appearance', 'message_position')
		return self._message_position

	def _set_message_position(self, message_position):
		self._message_position = message_position
		if not self.cfg.has_section('appearance'):
			self.cfg.add_section('appearance')
		self.cfg.set('appearance', 'message_position', self._message_position)
		self.save_config()

	message_position = property(_get_message_position, _set_message_position)

	# ~~~~~~~~~~

	def on_hide_menubar_changed(self, widget, data=None):
		self.hide_menubar = str(widget.get_active())

	def _get_hide_menubar(self):
		if self.cfg.has_section('appearance'):
				if self.cfg.has_option('appearance', 'hide_menubar'):
					return self.cfg.get('appearance', 'hide_menubar')
		return self._hide_menubar

	def _set_hide_menubar(self, hide_menubar):
		self._hide_menubar = hide_menubar
		if not self.cfg.has_section('appearance'):
			self.cfg.add_section('appearance')
		self.cfg.set('appearance', 'hide_menubar', self._hide_menubar)
		self.save_config()

	hide_menubar = property(_get_hide_menubar, _set_hide_menubar)

	# ~~~~~~~~~~

	def on_asciidoc_processor_changed(self, widget, data=None):
		model = widget.get_model()
		index = widget.get_active()

		if model[index][0] == 'asciidoc':
			self.asciidoc_processor = 'asciidoc'
		elif model[index][0] == 'asciidoctor':
			self.asciidoc_processor = 'asciidoctor'

	def _get_asciidoc_processor(self):
		if self.cfg.has_section('appearance'):
			if self.cfg.has_option('appearance', 'asciidoc_processor'):
				return self.cfg.get('appearance', 'asciidoc_processor')
		return self._asciidoc_processor

	def _set_asciidoc_processor(self, asciidoc_processor):
		self._asciidoc_processor = asciidoc_processor
		if not self.cfg.has_section('appearance'):
			self.cfg.add_section('appearance')
		self.cfg.set('appearance', 'asciidoc_processor', self._asciidoc_processor)
		self.save_config()

	asciidoc_processor = property(_get_asciidoc_processor, _set_asciidoc_processor)


	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	# Configuration GUI
	# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	def configure(self, dialog):
		vbox = gtk.VBox(spacing = 6)
		vbox.set_border_width(6)

		# ~~~~~~~~~~~~~~~~~~~~
		# General Settings
		# ~~~~~~~~~~~~~~~~~~~~
		lbl = gtk.Label()
		lbl.set_use_markup(True)
		lbl.set_markup("<b> General </b>")

		frame = gtk.Frame("")
		#frame.set_shadow_type(gtk.SHADOW_NONE)
		frame.set_label_widget(lbl)

		algn_general = gtk.Alignment(0.0, 0.0, 1.0, 1.0)
		algn_general.set_padding(0, 0, 12, 0)

		tbl = gtk.Table(1, 2, False)
		tbl.set_row_spacings(6)
		tbl.set_col_spacings(6)
		tbl.set_border_width(6)

		algn_general.add(tbl)
		frame.add(algn_general)
		vbox.pack_start(frame)

		# ~~~~~~~~~~

		lbl = gtk.Label('asciidoc Processor')
		lbl.set_alignment(0.0, 0.5)

		combobox = gtk.combo_box_new_text()
		combobox.append_text('asciidoc')
		combobox.append_text('asciidoctor')
		combobox.connect('changed', self.on_asciidoc_processor_changed)

		if self.asciidoc_processor == 'asciidoc':
			combobox.set_active(0)
		elif self.asciidoc_processor == 'asciidoctor':
			combobox.set_active(1)

		tbl.attach(lbl, 0, 1, 0, 1, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)
		tbl.attach(combobox, 1, 2, 0, 1, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)

		# ~~~~~~~~~~

		lbl = gtk.Label("Default Filetype")
		lbl.set_alignment(0.0, 0.5)

		combobox = gtk.combo_box_new_text()
		combobox.append_text('plain')
		combobox.append_text('html')
		combobox.append_text('markdown')
		combobox.append_text('restructuredtext')
		combobox.append_text('txt2tags')
		combobox.append_text('textile')
		combobox.append_text('org')
		combobox.append_text('asciidoc')
		combobox.append_text('fountain')
		combobox.connect('changed', self.on_default_filetype_changed)

		if self.default_filetype == 'plain':
			combobox.set_active(0)
		elif self.default_filetype == 'html':
			combobox.set_active(1)
		elif self.default_filetype == 'markdown':
			combobox.set_active(2)
		elif self.default_filetype == 'restructuredtext':
			combobox.set_active(3)
		elif self.default_filetype == 'txt2tags':
			combobox.set_active(4)
		elif self.default_filetype == 'textile':
			combobox.set_active(5)
		elif self.default_filetype == 'org':
			combobox.set_active(6)
		elif self.default_filetype == 'asciidoc':
			combobox.set_active(7)
		elif self.default_filetype == 'fountain':
			combobox.set_active(8)

		tbl.attach(lbl, 0, 1, 1, 2, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)
		tbl.attach(combobox, 1, 2, 1, 2, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)

		# ~~~~~~~~~~

		lbl = gtk.Label("Update Interval (seconds)")
		lbl.set_alignment(0.0, 0.5)

		txt = gtk.Entry()
		txt.connect('changed', self.on_update_interval_changed)
		txt.set_text(self.update_interval)

		tbl.attach(lbl, 0, 1, 2, 3, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)
		tbl.attach(txt, 1, 2, 2, 3, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)

		# ~~~~~~~~~~~~~~~~~~~~
		# Appearance Settings
		# ~~~~~~~~~~~~~~~~~~~~
		lbl = gtk.Label()
		lbl.set_use_markup(True)
		lbl.set_markup("<b> Appearance </b>")

		frame = gtk.Frame("")
		#frame.set_shadow_type(gtk.SHADOW_NONE)
		frame.set_label_widget(lbl)

		align = gtk.Alignment(0.0, 0.0, 1.0, 1.0)
		align.set_padding(0, 0, 12, 0)

		tbl = gtk.Table(6, 2, False)
		tbl.set_row_spacings(6)
		tbl.set_col_spacings(6)
		tbl.set_border_width(6)

		align.add(tbl)
		frame.add(align)
		vbox.pack_start(frame)

		# ~~~~~~~~~~

		lbl = gtk.Label('Hide Menubar at startup')
		lbl.set_alignment(0.0, 0.5)

		button = gtk.CheckButton('')
		button.connect("toggled", self.on_hide_menubar_changed)

		if self.hide_menubar == "True":
			button.set_active(True)
		else:
			button.set_active(False)

		tbl.attach(lbl, 0, 1, 0, 1, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)
		tbl.attach(button, 1, 2, 0, 1, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)

		# ~~~~~~~~~~

		lbl = gtk.Label('Preview Position')
		lbl.set_alignment(0.0, 0.5)

		combobox = gtk.combo_box_new_text()
		combobox.append_text('Sidebar')
		combobox.append_text('Message Window')
		combobox.connect('changed', self.on_preview_position_changed)

		if self.preview_position == 'sidebar':
			combobox.set_active(0)
		elif self.preview_position == 'message_window':
			combobox.set_active(1)

		tbl.attach(lbl, 0, 1, 1, 2, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)
		tbl.attach(combobox, 1, 2, 1, 2, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)

		# ~~~~~~~~~~

		lbl = gtk.Label('Output Position')
		lbl.set_alignment(0.0, 0.5)

		combobox = gtk.combo_box_new_text()
		combobox.append_text('Status Tab')
		combobox.append_text('Compiler Tab')
		combobox.append_text('Messages Tab')
		combobox.append_text('Standard Output')
		combobox.connect('changed', self.on_message_position_changed)

		if self.message_position == 'status_tab':
			combobox.set_active(0)
		elif self.message_position == 'compiler_tab':
			combobox.set_active(1)
		elif self.message_position == 'messages_tab':
			combobox.set_active(2)
		elif self.message_position == 'stdout':
			combobox.set_active(3)

		tbl.attach(lbl, 0, 1, 2, 3, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)
		tbl.attach(combobox, 1, 2, 2, 3, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)

		# ~~~~~~~~~~

		return vbox

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Global Functions
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

def geany_message(message, position='messages_tab'):
	message = time.strftime("%H:%M:%S: ", time.gmtime()) + message

	if position == 'messages_tab':
		geany.msgwindow.msg_add(message)
	elif position == 'compiler_tab':
		geany.msgwindow.compiler_add(message)
	elif position == 'status_tab':
		geany.msgwindow.status_add(message)
	else:
		print message

def make_temp_file(text):
	temp_file = tempfile.NamedTemporaryFile(mode='w', prefix='geany-preview-', delete=False)
	temp_file.write(text)
	temp_file.close()

	return temp_file

def preserve_double_spaces(text):
	# 1/4-em-space (&#8197;) = width of normal inter-word space
	# do not change "  \n" because markdown uses for line-break
	find = '([\.\?\!\:\;\"])\s*\ \ ([A-Z\"])'
	repl = '\\1  \\2'
	text = re.sub(find, repl, text)

	# double-space when sentences separated by newline or tab
	# ignore \n\n; because markdown uses to separate paragraphs
	find = '([\.\?\!\:\;\"])\ ?[\n\t](?!\s*\n)\s*([A-Z\"])'
	repl = '\\1  \\2'
	text = re.sub(find, repl, text)

	return text

# ~~~~~~~~~~~~~~~~~~~~
# Text Processors
# ~~~~~~~~~~~~~~~~~~~~

def plain_wrapper(text):
	output = '<pre>'
	output += cgi.escape(text)
	output += '</pre>'

	return output

def html_wrapper(text):
	return preserve_double_spaces(text)

def pandoc(text, pandoc_type):
	text = preserve_double_spaces(text)
	temp_file = make_temp_file(text)

	subprocess_arguments = [PANDOC_PATH, '-f {}'.format(pandoc_type),
		'-t html', '--smart',
		temp_file.name, '-o {}.html'.format(temp_file.name)]
	cmd = ' '.join(subprocess_arguments)

	# does not work without shell=True; old method: os.system(cmd)
	subprocess.call(cmd, shell=True)

	with open('{}.html'.format(temp_file.name), 'r') as f:
		html = f.read()

	os.remove(temp_file.name)
	os.remove('{}.html'.format(temp_file.name))

	# prevent showing unwanted centered ellipses
	output = html.replace('…', '...')

	return output

def asciidoc(text, asciidoc_processor='asciidoctor'):
	text = preserve_double_spaces(text)
	temp_file = make_temp_file(text)

	if asciidoc_processor == 'asciidoc':
		subprocess_arguments = [ASCIIDOC_PATH, '-b html5',
			'-o {}.html'.format(temp_file.name), temp_file.name]
	elif asciidoc_processor == 'asciidoctor':
		subprocess_arguments = [ASCIIDOCTOR_PATH, '-b html5',
			'-o {}.html'.format(temp_file.name), temp_file.name]

	cmd = ' '.join(subprocess_arguments)

	# does not work without shell=True; old method: os.system(cmd)
	subprocess.call(cmd, shell=True)

	with open('{}.html'.format(temp_file.name), 'r') as f:
		html = f.read()

	os.remove(temp_file.name)
	os.remove('{}.html'.format(temp_file.name))

	return html

def fountain(text):
	html_io = StringIO()
	text_io = StringIO(text)

	screenplay = screenplain.parsers.fountain.parse(text_io)
	formatter = screenplain.export.html.Formatter(html_io)
	formatter.convert(screenplay)

	output = '<div id="wrapper" class="screenplay">'
	output += html_io.getvalue()
	output += '</div>'

	return output 

def unrtf(text):
	temp_file = make_temp_file(text)

	subprocess_arguments = [UNRTF_PATH, temp_file.name]
	cmd = ' '.join(subprocess_arguments)

	# does not work without shell=True; old method: os.system(cmd)
	proc = subprocess.Popen(cmd, stderr=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)

	output = proc.stdout.read()

	return output
