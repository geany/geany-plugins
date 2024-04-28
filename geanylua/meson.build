project(
  'geanylua',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'geanylua'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
lua_dep = dependency('lua51')

libgeanylua = library(
  'libgeanylua',
  sources: [
    'glspi_app.c',
    'glspi_dlg.c',
    'glspi_doc.c',
    'glspi_init.c',
    'glspi_kfile.c',
    'glspi_run.c',
    'glspi_sci.c',
    'gsdlg.c',
    'gsdlg_lua.c',
  ],
  dependencies: [config_dep, geany_dep, lua_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_libdir,
)
libgeanylua_dep = declare_dependency(
  link_with: [libgeanylua],
  compile_args: ['-DG_LOG_DOMAIN="GeanyLua"'],
)

library(
  plugin_name,
  sources: [
    'geanylua.c',
  ],
  dependencies: [config_dep, geany_dep, libgeanylua_dep, lua_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)


install_data(
  sources: [
    'examples/dialogs/basic-dialogs.lua',
    'examples/dialogs/complex-dialog.lua',
    'examples/edit/calculator.lua',
    'examples/edit/lua-replace.lua',
    'examples/edit/proper-case.lua',
    'examples/edit/reverse.lua',
    'examples/edit/right-trim.lua',
    'examples/edit/select-block.lua',
    'examples/info/about.lua',
    'examples/info/app-information.lua',
    'examples/info/file-information.lua',
    'examples/info/list-open-files.lua',
    'examples/info/show-filename.lua',
    'examples/scripting/help.lua',
    'examples/scripting/open-script.lua',
    'examples/scripting/rebuild-menu.lua',
    'examples/scripting/show-examples.lua',
    'examples/work/01.edit-test-script.lua',
    'examples/work/02.run-test-script.lua',
    'examples/work/03.install-test-script.lua',
  ],
  install_dir: plugin_datadir,
)

install_data(
  sources: [
    'AUTHORS',
    'COPYING',
    'ChangeLog',
    'NEWS',
    'README',
    'docs/geanylua-index.html',
    'docs/geanylua-input.html',
    'docs/geanylua-intro.html',
    'docs/geanylua-keyfile.html',
    'docs/geanylua-keys.html',
    'docs/geanylua-ref.html',
    'docs/luarefv51.html',
  ],
  install_dir: plugin_docdir,
)
