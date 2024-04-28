
# Datadir: /usr/share/geany-plugins
plugin_datadir = get_option('datadir') / 'geany-plugins' / plugin_name

# Libdir: /usr/lib/geany-plugins
plugin_libdir = get_option('libdir') / 'geany-plugins' / plugin_name

# Docdir: /usr/share/doc/geany-plugins
plugin_docdir = get_option('datadir') / 'doc' / 'geany-plugins' / plugin_name

plugin_path = get_option('libdir') / 'geany'

conf_data = configuration_data()

conf_data.set('plugin_name', plugin_name)
conf_data.set('plugin_path', plugin_path)

conf_data.set('version', meson.project_version())
conf_data.set('geany_version', '2.0.0')

conf_data.set('prefix', get_option('prefix'))
conf_data.set('libdir', get_option('libdir'))
conf_data.set('localedir', get_option('localedir'))

conf_data.set('plugin_datadir', plugin_datadir)
conf_data.set('plugin_libdir', plugin_libdir)
conf_data.set('plugin_docdir', plugin_docdir)

configure_file(
  input: 'config.h.in',
  output: 'config.h',
  configuration: conf_data,
)

config_dep = declare_dependency(
  include_directories: include_directories('.'),
  compile_args: ['-DHAVE_CONFIG_H'],
)

install_data(
  sources: [
    'AUTHORS',
    'COPYING',
    'ChangeLog',
    'NEWS',
    'README',
  ],
  install_dir: plugin_docdir,
)
