project('dvbv5', 'c', version: '1.20')

bindir = join_paths(get_option('prefix'), get_option('bindir'))
libdir = join_paths(get_option('prefix'), get_option('libdir'))

cc = meson.get_compiler('c')

lib_both = []
dev_libdvbv5 = get_option('lib')

have_libdvbv5 = false
libdvbv5 = cc.find_library ('libdvbv5', required: false)

if get_option('lib') or not libdvbv5.found()

  project_name = 'lib' + meson.project_name()
  project_version = meson.project_version() + '.0'

  message('Build ' + project_name + ' ' + project_version)

  lib_c_args = '-D_GNU_SOURCE'

  libm        = cc.find_library('libm', required: true)
  librt       = cc.find_library('librt', required: true)
  libpthread  = cc.find_library('libpthread', required: true)

  udevdepend = [dependency('libudev')]
  libdepend  = [udevdepend, libm, librt, libpthread]

  incdir = include_directories('include')

  subdir('include')
  subdir('libdvbv5')

endif

if not get_option('lib')

  project_name = meson.project_name() + '-gtk'
  project_version = meson.project_version() + '.36'

  message('Build ' + project_name + ' ' + project_version)

  res = []
  bin_c_args = ['-DVERSION="' + project_version + '"']

  if get_option('light')
    bin_c_args += ['-DPROGNAME="Dvbv5-Light-Gtk"', '-DLIGHT=1']
    guidepend = [dependency('gtk+-3.0')]
  else
    bin_c_args += ['-DPROGNAME="Dvbv5-Gtk"']
    libgstvideo = cc.find_library('libgstvideo-1.0', required: true)
    guidepend = [dependency('gtk+-3.0'), dependency('gstreamer-base-1.0'), libgstvideo]
  endif

  if libdvbv5.found()
    guidepend += dependency('libdvbv5')
  else
    lib_dep = declare_dependency(link_with: lib_both.get_static_lib(), dependencies: guidepend)
  endif

  subdir('gtk/data')
  subdir('gtk/src')

endif
