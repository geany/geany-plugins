import os
from ConfigParser import SafeConfigParser
import geany
import geany.console
import gobject
import gtk
import gtk.gdk as gdk
import pango


WIDGET_STATES = [ gtk.STATE_NORMAL, gtk.STATE_ACTIVE, gtk.STATE_PRELIGHT,
	gtk.STATE_SELECTED, gtk.STATE_INSENSITIVE ]


class ConsolePlugin(geany.Plugin):

	__plugin_name__ = "Python Console"
	__plugin_description__ = ("Installs a Python console that allows you " +
		"to use the Geany API.")
	__plugin_version__ = "0.01"
	__plugin_author__ = "Matthew Brush <mbrush@codebrainz.ca>"

	_font = "Monospace 9"
	_bg = "#FFFFFF"
	_fg = "#000000"
	_banner = ("Geany Python Console\n You can access the Geany Python " +
				"API by importing the `geany' module.\n")
	_use_rl_completer = True

	_builder = None

	def __init__(self):
		geany.Plugin.__init__(self)
		self.load_config()
		self.install_console()


	def cleanup(self):
		#self.save_config()
		self.on_save_config_timeout() # do it now
		self.uninstall_console()


	def load_config(self):
		self.cfg_path = os.path.join(geany.app.configdir, "plugins", "pyconsole.conf")
		self.cfg = SafeConfigParser()
		self.cfg.read(self.cfg_path)


	def save_config(self):
		gobject.idle_add(self.on_save_config_timeout)


	def on_save_config_timeout(self, data=None):
		self.cfg.write(open(self.cfg_path, 'w'))
		return False


	def install_console(self):

		# load general settings
		self.banner = self.banner
		self.use_rl_completer = self.use_rl_completer

		self.swin = gtk.ScrolledWindow()
		self.swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_ALWAYS)
		self.console = geany.console.Console(banner = self.banner,
							use_rlcompleter = self.use_rl_completer)
		self.console.connect("populate-popup", self.on_console_populate_popup)

		# apply appearance settings
		self.font = self.font
		self.bg = self.bg
		self.fg = self.fg

		self.swin.add(self.console)
		geany.main_widgets.message_window_notebook.append_page(self.swin,
			gtk.Label("Python"))

		self.swin.show_all()
		self.save_config()

	def uninstall_console(self):
		self.swin.destroy()


	def _get_font(self):
		if self.cfg.has_section('appearances'):
			if self.cfg.has_option('appearances', 'font'):
				return self.cfg.get('appearances', 'font')
		return self._font
	def _set_font(self, font):
		self._font = font
		font_desc = pango.FontDescription(font)
		self.console.modify_font(font_desc)
		if not self.cfg.has_section('appearances'):
			self.cfg.add_section('appearances')
		self.cfg.set('appearances', 'font', self._font)
		self.save_config()
	font = property(_get_font, _set_font)


	def _get_bg(self):
		if self.cfg.has_section('appearances'):
			if self.cfg.has_option('appearances', 'bg_color'):
				return self.cfg.get('appearances', 'bg_color')
		return self._bg
	def _set_bg(self, bg):
		self._bg = bg
		color = gdk.color_parse(self._bg)
		for state in WIDGET_STATES:
			self.console.modify_bg(state, color)
			self.console.modify_base(state, color)
		if not self.cfg.has_section('appearances'):
			self.cfg.add_section('appearances')
		self.cfg.set('appearances', 'bg_color', self._bg)
		self.save_config()
	bg = property(_get_bg, _set_bg)


	def _get_fg(self):
		if self.cfg.has_section('appearances'):
			if self.cfg.has_option('appearances', 'fg_color'):
				return self.cfg.get('appearances', 'fg_color')
		return self._fg
	def _set_fg(self, fg):
		self._fg = fg
		color = gdk.color_parse(self._fg)
		for state in WIDGET_STATES:
			self.console.modify_fg(state, color)
			self.console.modify_text(state, color)
		if not self.cfg.has_section('appearances'):
			self.cfg.add_section('appearances')
		self.cfg.set('appearances', 'fg_color', self._fg)
		self.save_config()
	fg = property(_get_fg, _set_fg)


	def _get_banner(self):
		if self.cfg.has_section('general'):
				if self.cfg.has_option('general', 'banner'):
					return self.cfg.get('general', 'banner')
		return self._banner
	def _set_banner(self, banner):
		self._banner = banner
		if not self.cfg.has_section('general'):
			self.cfg.add_section('general')
		self.cfg.set('general', 'banner', self._banner)
		self.save_config()
	banner = property(_get_banner, _set_banner)


	def _get_use_rl_completer(self):
		if self.cfg.has_section('general'):
			if self.cfg.has_option('general', 'use_rl_completer'):
				return self.cfg.getboolean('general', 'use_rl_completer')
		return self._use_rl_completer
	def _set_use_rl_completer(self, use_rl_completer):
		self._use_rl_completer = use_rl_completer
		if not self.cfg.has_section('general'):
			self.cfg.add_section('general')
		self.cfg.set('general', 'use_rl_completer',
			str(self._use_rl_completer).lower())
		self.save_config()
	use_rl_completer = property(_get_use_rl_completer, _set_use_rl_completer)


	def on_console_populate_popup(self, textview, menu, data=None):
		item = gtk.SeparatorMenuItem()
		item.show()
		menu.append(item)
		item = gtk.ImageMenuItem(stock_id=gtk.STOCK_PREFERENCES)
		item.show()
		menu.append(item)
		item.connect("activate", lambda w,d=None: self.show_configure())


	def on_banner_changed(self, text_buf, data=None):
		self.banner = text_buf.get_text(text_buf.get_start_iter(), text_buf.get_end_iter())

	def on_use_rl_completer_toggled(self, chk_btn, data=None):
		self.use_rl_completer = chk_btn.get_active()

	def on_font_changed(self, font_btn, data=None):
		self.font = font_btn.get_font_name()

	def on_fg_color_changed(self, clr_btn, data=None):
		self.fg = clr_btn.get_color().to_string()

	def on_bg_color_changed(self, clr_btn, data=None):
		self.bg = clr_btn.get_color().to_string()


	def show_configure(self):
		dialog = gtk.Dialog("Configure Python Console",
							geany.main_widgets.window,
							gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
							(gtk.STOCK_CLOSE, gtk.RESPONSE_ACCEPT))

		dialog.set_has_separator(True)

		content_area = dialog.get_content_area()
		content_area.set_border_width(6)

		vbox = gtk.VBox(spacing=6)
		vbox.set_border_width(6)

		lbl = gtk.Label()
		lbl.set_use_markup(True)
		lbl.set_markup("<b>General</b>")

		fra_general = gtk.Frame("")
		fra_general.set_shadow_type(gtk.SHADOW_NONE)
		fra_general.set_label_widget(lbl)

		al_general = gtk.Alignment(0.0, 0.0, 1.0, 1.0)
		al_general.set_padding(0, 0, 12, 0)
		fra_general.add(al_general)

		tbl = gtk.Table(3, 2, False)
		tbl.set_row_spacings(6)
		tbl.set_col_spacings(6)
		tbl.set_border_width(6)

		lbl = gtk.Label("Banner:")
		lbl.set_alignment(0.0, 0.0)

		tvw = gtk.TextView()
		tvw.get_buffer().set_text(self.banner)
		tvw.get_buffer().connect("changed", self.on_banner_changed)

		swin = gtk.ScrolledWindow()
		swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
		swin.set_shadow_type(gtk.SHADOW_ETCHED_IN)
		swin.add(tvw)

		tbl.attach(lbl, 0, 1, 0, 1, gtk.FILL, gtk.FILL, 0, 0)
		tbl.attach(swin, 1, 2, 0, 1, gtk.EXPAND | gtk.FILL, gtk.EXPAND | gtk.FILL, 0, 0)

		lbl = gtk.Label("")
		lbl.set_alignment(0.0, 0.5)

		check = gtk.CheckButton("Use Readline")
		if self.use_rl_completer:
			check.set_active(True)
		check.connect("toggled", self.on_use_rl_completer_toggled)

		tbl.attach(lbl, 0, 1, 1, 2, gtk.FILL, gtk.FILL, 0, 0)
		tbl.attach(check, 1, 2, 1, 2, gtk.FILL, gtk.FILL, 0, 0)

		lbl = gtk.Label("")
		lbl.set_alignment(0.0, 0.5)
		lbl.set_use_markup(True)
		lbl.set_markup('<span size="small" style="italic">' +
			'Note: General settings will be applied when console is reloaded.' +
			'</span>')
		tbl.attach(lbl, 0, 2, 2, 3, gtk.FILL, gtk.FILL, 0, 0)

		al_general.add(tbl)

		lbl = gtk.Label()
		lbl.set_use_markup(True)
		lbl.set_markup("<b>Appearances</b>")

		fra_appearances = gtk.Frame("")
		fra_appearances.set_shadow_type(gtk.SHADOW_NONE)
		fra_appearances.set_label_widget(lbl)

		al_appearances = gtk.Alignment(0.0, 0.0, 1.0, 1.0)
		al_appearances.set_padding(0, 0, 12, 0)
		fra_appearances.add(al_appearances)

		tbl = gtk.Table(3, 2, False)
		tbl.set_row_spacings(6)
		tbl.set_col_spacings(6)
		tbl.set_border_width(6)

		lbl = gtk.Label("Font:")
		lbl.set_alignment(0.0, 0.5)

		btn = gtk.FontButton(self.font)
		btn.connect("font-set", self.on_font_changed)

		tbl.attach(lbl, 0, 1, 0, 1, gtk.FILL, gtk.FILL, 0, 0)
		tbl.attach(btn, 1, 2, 0, 1, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)

		lbl = gtk.Label("FG Color:")
		lbl.set_alignment(0.0, 0.5)

		btn = gtk.ColorButton(gdk.color_parse(self.fg))
		btn.connect("color-set", self.on_fg_color_changed)

		tbl.attach(lbl, 0, 1, 1, 2, gtk.FILL, gtk.FILL, 0, 0)
		tbl.attach(btn, 1, 2, 1, 2, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)

		lbl = gtk.Label("BG Color:")
		lbl.set_alignment(0.0, 0.5)

		btn = gtk.ColorButton(gdk.color_parse(self.bg))
		btn.connect("color-set", self.on_bg_color_changed)

		tbl.attach(lbl, 0, 1, 2, 3, gtk.FILL, gtk.FILL, 0, 0)
		tbl.attach(btn, 1, 2, 2, 3, gtk.FILL | gtk.EXPAND, gtk.FILL, 0, 0)

		al_appearances.add(tbl)

		vbox.pack_start(fra_general, True, True, 0)
		vbox.pack_start(fra_appearances, False, True, 0)
		content_area.pack_start(vbox, True, True, 0)
		content_area.show_all()

		dialog.run()
		dialog.destroy()
