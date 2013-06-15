import os
import imp
from collections import namedtuple
import geany

PluginInfo = namedtuple('PluginInfo', 'filename, name, version, description, author, cls')


class PluginLoader(object):

	plugins = {}

	def __init__(self, plugin_dirs):

		self.plugin_dirs = plugin_dirs

		self.available_plugins = []
		for plugin in self.iter_plugin_info():
			self.available_plugins.append(plugin)

		self.restore_loaded_plugins()


	def update_loaded_plugins_file(self):
		for path in self.plugin_dirs:
			if os.path.isdir(path):
				try:
					state_file = os.path.join(path, '.loaded_plugins')
					with open(state_file, 'w') as f:
						for plugfn in self.plugins:
							f.write("%s\n" % plugfn)
				except IOError as err:
					if err.errno == 13: #perms
						pass
					else:
						raise


	def restore_loaded_plugins(self):
		loaded_plugins = []
		for path in reversed(self.plugin_dirs):
			state_file = os.path.join(path, ".loaded_plugins")
			if os.path.exists(state_file):
				for line in open(state_file):
					line = line.strip()
					if line not in loaded_plugins:
						loaded_plugins.append(line)
		for filename in loaded_plugins:
			self.load_plugin(filename)


	def load_all_plugins(self):

		for plugin_info in self.iter_plugin_info():
			if plugin_filename.endswith('test.py'): # hack for testing
				continue
			plug = self.load_plugin(plugin_info.filename)
			if plug:
				print("Loaded plugin: %s" % plugin_info.filename)
				print("  Name: %s v%s" % (plug.name, plug.version))
				print("  Desc: %s" % plug.description)
				print("  Author: %s" % plug.author)


	def unload_all_plugins(self):

		for plugin in self.plugins:
			self.unload_plugin(plugin)


	def reload_all_plugins(self):

		self.unload_all_plugins()
		self.load_all_plugins()


	def iter_plugin_info(self):

		for d in self.plugin_dirs:
			if os.path.isdir(d):
				for current_file in os.listdir(d):
					#check inside folders inside the plugins dir so we can load .py files here as plugins
					current_path=os.path.abspath(os.path.join(d, current_file))
					if os.path.isdir(current_path):
						for plugin_folder_file in os.listdir(current_path):
							if plugin_folder_file.endswith('.py'):
								#loop around results if its fails to load will never reach yield
								for p in self.load_plugin_info(current_path,plugin_folder_file):
									yield p
									
					#not a sub directory so if it ends with .py lets just attempt to load it as a plugin
					if current_file.endswith('.py'):
						#loop around results if its fails to load will never reach yield
						for p in self.load_plugin_info(d,current_file):
							yield p
								
	def load_plugin_info(self,d,f):
		filename = os.path.abspath(os.path.join(d, f))
		if filename.endswith("test.py"):
			pass
		text = open(filename).read()
		module_name = os.path.basename(filename)[:-3]
		try:
			module = imp.load_source(module_name, filename)
		except ImportError as exc:
			print "Error: failed to import settings module ({})".format(exc)
			module=None
		if module:	
			for k, v in module.__dict__.iteritems():
				if k == geany.Plugin.__name__:
					continue
				try:
					if issubclass(v, geany.Plugin):
						inf = PluginInfo(
								filename,
								getattr(v, '__plugin_name__'),
								getattr(v, '__plugin_version__', ''),
								getattr(v, '__plugin_description__', ''),
								getattr(v, '__plugin_author__', ''),
								v)
						yield inf
						
				except TypeError:
					continue


	def load_plugin(self, filename):

		for avail in self.available_plugins:
			if avail.filename == filename:
				inst = avail.cls()
				self.plugins[filename] = inst
				self.update_loaded_plugins_file()
				geany.ui_utils.set_statusbar('GeanyPy: plugin activated: %s' %
					inst.name, True)
				return inst


	def unload_plugin(self, filename):

		try:
			plugin = self.plugins[filename]
			name = plugin.name
			plugin.cleanup()
			del self.plugins[filename]
			self.update_loaded_plugins_file()
			geany.ui_utils.set_statusbar('GeanyPy: plugin deactivated: %s' %
				name, True)
		except KeyError:
			print("Unable to unload plugin '%s': it's not loaded" % filename)


	def reload_plugin(self, filename):

		if filename in self.plugins:
			self.unload_plugin(filename)
		self.load_plugin(filename)


	def plugin_has_help(self, filename):

		for plugin_info in self.iter_plugin_info():
			if plugin_info.filename == filename:
				return hasattr(plugin_info.cls, 'show_help')


	def plugin_has_configure(self, filename):

		try:
			return hasattr(self.plugins[filename], 'show_configure')
		except KeyError:
			return None
