
docdir = get_option('datadir') / 'doc'
project_datadir = get_option('datadir') / 'geany-plugins'
project_libdir = get_option('libdir') / 'geany-plugins'
project_docdir = docdir / 'geany-plugins'

plugin_datadir = project_datadir / plugin_name
plugin_libdir = project_libdir / plugin_name
plugin_docdir = project_docdir / plugin_name

plugin_path = get_option('libdir') / 'geany'

conf_data = configuration_data()

conf_data.set('plugin_name', plugin_name)
conf_data.set('plugin_path', plugin_path)

conf_data.set('version', meson.project_version())
conf_data.set('geany_version', '2.0.0')

conf_data.set('prefix', get_option('prefix'))
conf_data.set('libdir', get_option('libdir'))
conf_data.set('docdir', docdir)
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
