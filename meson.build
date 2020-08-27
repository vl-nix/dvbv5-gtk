project('dvbv5-gtk', 'c', version: '1.20.34')

if get_option('lib')

  libname = 'libdvbv5'
  libsetver = '1.20.0'
  libversion = '@0@.@1@.@2@'.format(0, 0, 0)
  libdir = join_paths(get_option('prefix'), get_option('libdir'))

  message( 'Build Library: ' + libname )
  message( 'Build Library: ' + libsetver )

else

run_command('tar', '--directory=gtk/src/', '-xf', files('gtk/src/icons.tar.xz'))

bindir = join_paths(get_option('prefix'), get_option('bindir'))
exec_path = bindir + '/' + meson.project_name()

desktop_conf = configuration_data()
set_desktop = [
  ['NAME', 'Dvbv5-Gtk'],
  ['COMMENT', 'DVBv5 tool'],
  ['EXEC', exec_path],
  ['ICON', 'display'],
  ['TERMINAL', 'false'],
  ['TYPE', 'Application'],
  ['CATEGORIES', 'GTK;GNOME;AudioVideo;Player;Video;'],
  ['VERSION', meson.project_version()]
]

foreach props: set_desktop
  desktop_conf.set(props[0], props[1])
endforeach

desktop = meson.project_name() + '.desktop'
configure_file(
  input: 'gtk/desktop',
  output: desktop,
  configuration: desktop_conf,
  install: true,
  install_dir: join_paths('share', 'applications')
)

endif

config_h = configuration_data()
set_defines = [
  ['ICONV_CONST', ''],
  ['ENABLE_NLS', 1],
  ['HAVE_BACKTRACE', 1],
  ['HAVE_DVBV5_REMOTE', 1],
  ['HAVE_PTHREAD', 1],
  ['HAVE_SECURE_GETENV', 1],
  ['LIBDVBV5_DOMAIN', '"libdvbv5"'],
  ['LOCALEDIR', '"/usr/share/locale"'],
  ['V4L_UTILS_VERSION', '"1.20.0"']
]

foreach define: set_defines
  config_h.set(define[0], define[1])
endforeach

configure_file(output: 'config.h', configuration: config_h )

common_flags = [
  '-DVERSION="' + meson.project_version() + '"',
  '-D_GNU_SOURCE'
]

cc = meson.get_compiler('c')
add_project_arguments(common_flags, language: 'c')

libm        = cc.find_library('libm', required: true)
librt       = cc.find_library('librt', required: true)
libpthread  = cc.find_library('libpthread', required: true)
libgstvideo = cc.find_library('libgstvideo-1.0', required: true)

have_libdvbv5 = false
libdvbv5 = cc.find_library ('libdvbv5', required: false)
if libdvbv5.found()
    have_libdvbv5 = true
endif

c = run_command('sh', '-c', 'for file in libdvbv5/*.h libdvbv5/*.c libdvbv5/*/*.c; do echo $file; done')
lib_src = c.stdout().strip().split('\n')

c = run_command('sh', '-c', 'for file in gtk/src/*.h gtk/src/*.c; do echo $file; done')
bin_src = c.stdout().strip().split('\n')

udevdepend = [dependency('libudev')]
basedepend = [libpthread, libm, librt]
libdepend  = [udevdepend, basedepend]
guidepend  = [dependency('gtk+-3.0'), dependency('gstreamer-base-1.0'), libgstvideo, basedepend]

if have_libdvbv5
  bindepend = [dependency('libdvbv5'), guidepend]
  src = [bin_src]
else
  bindepend = [udevdepend, guidepend]
  src = [lib_src, bin_src]
endif

incdir = include_directories('include')

if get_option('lib')

  lib = shared_library('dvbv5', sources: lib_src, version: libversion, soversion: 0, include_directories: incdir, dependencies: libdepend, install: true)

  if get_option('dev')

    c = run_command('sh', '-c', 'for file in include/libdvbv5/*.h; do echo $file; done')
    dvbv5_public_h = c.stdout().strip().split('\n')

    install_headers(dvbv5_public_h, install_dir: join_paths(get_option('prefix'), get_option('includedir'), libname))

    pkg = import('pkgconfig')
    pkg.generate( name: libname, description: 'DVBv5 utility library', requires: [udevdepend], 
      version: libsetver, libraries: lib, install_dir: join_paths(libdir, 'pkgconfig'))

  endif

else

  if have_libdvbv5
    executable(meson.project_name(), src, dependencies: bindepend, install: true)
  else
    executable(meson.project_name(), src, dependencies: bindepend, include_directories: incdir, install: true)
  endif

  path_gschema = join_paths(get_option('prefix'), get_option('datadir'), 'glib-2.0/schemas')
  gschema_file = configure_file(input: 'gtk/gschema', output: 'org.gnome.dvbv5-gtk.gschema.xml', copy: true)
  install_data(gschema_file, install_dir: path_gschema)

  # glib-compile-schemas /usr/share/glib-2.0/schemas
  cmd_compile_schema = 'glib-compile-schemas ' + path_gschema
  meson.add_install_script('sh', '-c', cmd_compile_schema)

  # Uninstall schema: ninja -C build uc
  cmd_rm_schema = 'rm -f ' + path_gschema + '/org.gnome.dvbv5-gtk.gschema.xml ' + path_gschema + '/gschemas.compiled'
  run_target('uc', command: ['sh', '-c', cmd_rm_schema + '; ' + cmd_compile_schema])

endif
