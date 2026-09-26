project('gimp-plugin-bimp', 'c',
  version : '3.0.0-dev',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.60.0',
  default_options : ['c_std=gnu11', 'warning_level=1', 'buildtype=debugoptimized'],
)

gimp_dep   = dependency('gimp-3.0',   version : '>= 3.0.0')
gimpui_dep = dependency('gimpui-3.0', version : '>= 3.0.0')
gtk_dep    = dependency('gtk+-3.0')
math_dep   = meson.get_compiler('c').find_library('m', required : false)

gettext_package = 'bimp'

# GIMP 3 loads each plug-in from a folder of its own, named like the
# executable: <plug-ins>/bimp/bimp, with its translations in
# <plug-ins>/bimp/locale.
plugindir = get_option('plugindir')
if plugindir == ''
  plugindir = gimp_dep.get_variable(pkgconfig : 'gimplibdir') / 'plug-ins'
endif
bimpdir = plugindir / 'bimp'

gnome = import('gnome')
icons = gnome.compile_resources('bimp-icons', 'src/images/icons.xml',
  source_dir : 'src/images',
  c_name : 'bimp_icons',
)

sources = files(
  'src/bimp.c',
  'src/bimp-gui.c',
  'src/bimp-manipulations.c',
  'src/bimp-manipulations-gui.c',
  'src/bimp-operate.c',
  'src/bimp-serialize.c',
  'src/bimp-utils.c',
  'src/manipulation-gui/gui-changeformat.c',
  'src/manipulation-gui/gui-color.c',
  'src/manipulation-gui/gui-crop.c',
  'src/manipulation-gui/gui-fliprotate.c',
  'src/manipulation-gui/gui-rename.c',
  'src/manipulation-gui/gui-resize.c',
  'src/manipulation-gui/gui-sharpblur.c',
  'src/manipulation-gui/gui-userdef.c',
  'src/manipulation-gui/gui-watermark.c',
)

c_args = [
  '-DGETTEXT_PACKAGE="' + gettext_package + '"',
  '-DPLUG_IN_VERSION="' + meson.project_version() + '"',
]

executable('bimp', sources, icons,
  dependencies : [gimp_dep, gimpui_dep, gtk_dep, math_dep],
  c_args : c_args,
  install : true,
  install_dir : bimpdir,
)

# the .bimp file format, tested without GIMP running: meson test -C build
test_serialize = executable('test-serialize',
  'tests/unit/test-serialize.c',
  'src/bimp-manipulations.c',
  'src/bimp-serialize.c',
  'src/bimp-utils.c',
  dependencies : [gimp_dep, gtk_dep, math_dep],
  c_args : c_args,
  build_by_default : false,
)
test('serialize', test_serialize)

subdir('po')
