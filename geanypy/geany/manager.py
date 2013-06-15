import gtk
import gobject
import glib
from htmlentitydefs import name2codepoint
from loader import PluginLoader


class PluginManager(gtk.Dialog):

	def __init__(self, plugin_dirs=[]):
		gtk.Dialog.__init__(self, title="Plugin Manager")
		self.loader = PluginLoader(plugin_dirs)

		self.set_default_size(400, 450)
		self.set_has_separator(True)
		icon = self.render_icon(gtk.STOCK_PREFERENCES, gtk.ICON_SIZE_MENU)
		self.set_icon(icon)

		self.connect("response", lambda w,d: self.hide())

		vbox = gtk.VBox(False, 12)
		vbox.set_border_width(12)

		lbl = gtk.Label("Choose plugins to load or unload:")
		lbl.set_alignment(0.0, 0.5)
		vbox.pack_start(lbl, False, False, 0)

		sw = gtk.ScrolledWindow()
		sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
		sw.set_shadow_type(gtk.SHADOW_ETCHED_IN)
		vbox.pack_start(sw, True, True, 0)

		self.treeview = gtk.TreeView()
		sw.add(self.treeview)

		vbox.show_all()

		self.get_content_area().add(vbox)

		action_area = self.get_action_area()
		action_area.set_spacing(0)
		action_area.set_homogeneous(False)

		btn = gtk.Button(stock=gtk.STOCK_CLOSE)
		btn.set_border_width(6)
		btn.connect("clicked", lambda x: self.response(gtk.RESPONSE_CLOSE))
		action_area.pack_start(btn, False, True, 0)
		btn.show()

		self.btn_help = gtk.Button(stock=gtk.STOCK_HELP)
		self.btn_help.set_border_width(6)
		self.btn_help.set_no_show_all(True)
		action_area.pack_start(self.btn_help, False, True, 0)
		action_area.set_child_secondary(self.btn_help, True)

		self.btn_prefs = gtk.Button(stock=gtk.STOCK_PREFERENCES)
		self.btn_prefs.set_border_width(6)
		self.btn_prefs.set_no_show_all(True)
		action_area.pack_start(self.btn_prefs, False, True, 0)
		action_area.set_child_secondary(self.btn_prefs, True)

		action_area.show()

		self.load_plugins_list()


	def on_help_button_clicked(self, button, treeview, model):
		path = treeview.get_cursor()[0]
		iter = model.get_iter(path)
		filename = model.get_value(iter, 2)
		for plugin in self.loader.available_plugins:
			if plugin.filename == filename:
				plugin.cls.show_help()
				break
		else:
			print("Plugin does not support help function")


	def on_preferences_button_clicked(self, button, treeview, model):
		path = treeview.get_cursor()[0]
		iter = model.get_iter(path)
		filename = model.get_value(iter, 2)
		try:
			self.loader.plugins[filename].show_configure()
		except KeyError:
			print("Plugin is not loaded, can't run configure function")


	def activate_plugin(self, filename):
		self.loader.load_plugin(filename)


	def deactivate_plugin(self, filename):
		self.loader.unload_plugin(filename)


	def load_plugins_list(self):
		liststore = gtk.ListStore(gobject.TYPE_BOOLEAN, str, str)

		self.btn_help.connect("clicked",
			self.on_help_button_clicked, self.treeview, liststore)

		self.btn_prefs.connect("clicked",
			self.on_preferences_button_clicked, self.treeview, liststore)

		self.treeview.set_model(liststore)
		self.treeview.set_headers_visible(False)
		self.treeview.set_grid_lines(True)

		check_renderer = gtk.CellRendererToggle()
		check_renderer.set_radio(False)
		check_renderer.connect('toggled', self.on_plugin_load_toggled, liststore)
		text_renderer = gtk.CellRendererText()

		check_column = gtk.TreeViewColumn(None, check_renderer, active=0)
		text_column = gtk.TreeViewColumn(None, text_renderer, markup=1)

		self.treeview.append_column(check_column)
		self.treeview.append_column(text_column)

		self.treeview.connect('row-activated',
			self.on_row_activated, check_renderer, liststore)
		self.treeview.connect('cursor-changed',
			self.on_selected_plugin_changed, liststore)

		self.load_sorted_plugins_info(liststore)


	def load_sorted_plugins_info(self, list_store):

		plugin_info_list = list(self.loader.iter_plugin_info())
		#plugin_info_list.sort(key=lambda pi: pi[1])

		for plugin_info in plugin_info_list:

			lbl = str('<big><b>%s</b></big> <small>%s</small>\n%s\n' +
					'<small><b>Author:</b> %s\n' +
					'<b>Filename:</b> %s</small>') % (
					glib.markup_escape_text(plugin_info.name),
					glib.markup_escape_text(plugin_info.version),
					glib.markup_escape_text(plugin_info.description),
					glib.markup_escape_text(plugin_info.author),
					glib.markup_escape_text(plugin_info.filename))

			loaded = plugin_info.filename in self.loader.plugins

			list_store.append([loaded, lbl, plugin_info.filename])


	def on_selected_plugin_changed(self, treeview, model):

		path = treeview.get_cursor()[0]
		iter = model.get_iter(path)
		filename = model.get_value(iter, 2)
		active = model.get_value(iter, 0)

		if self.loader.plugin_has_configure(filename):
			self.btn_prefs.set_visible(True)
		else:
			self.btn_prefs.set_visible(False)

		if self.loader.plugin_has_help(filename):
			self.btn_help.set_visible(True)
		else:
			self.btn_help.set_visible(False)


	def on_plugin_load_toggled(self, cell, path, model):
		active = not cell.get_active()
		iter = model.get_iter(path)
		model.set_value(iter, 0, active)
		if active:
			self.activate_plugin(model.get_value(iter, 2))
		else:
			self.deactivate_plugin(model.get_value(iter, 2))


	def on_row_activated(self, tvw, path, view_col, cell, model):
		self.on_plugin_load_toggled(cell, path, model)
