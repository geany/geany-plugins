cmake_minimum_required(VERSION 3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local/lib/geany")

project(
  autorun
  VERSION 0.0.1
  LANGUAGES C
  DESCRIPTION "Geany action interceptor plugin")

set(CMAKE_C_FLAGS "-Wall")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O3")

set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g" )
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_compile_definitions(LOCALEDIR="/usr/share/locale")
# the `pkg_check_modules` function is created with this call
find_package(PkgConfig REQUIRED)
# these calls create special `PkgConfig::<MODULE>` variables
pkg_check_modules(geany REQUIRED IMPORTED_TARGET GLOBAL geany)

add_library(${PROJECT_NAME} SHARED src/plugin_main.c src/autorun.c src/utils.c
                                   src/spawn.c src/menu.c src/project.c)
# prevent cmake from prefixing the shared library
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::geany)
# install to users bin
install(TARGETS ${PROJECT_NAME} DESTINATION plugins)
