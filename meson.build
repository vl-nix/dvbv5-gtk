project('dvbv5-gtk', 'c', version: '1.20.50')

bindir = join_paths(get_option('prefix'), get_option('bindir'))
libdir = join_paths(get_option('prefix'), get_option('libdir'))

cc = meson.get_compiler('c')

lib_both = []

dev_libdvbv5 = false
libdvbv5 = cc.find_library ('libdvbv5', required: false)

if dev_libdvbv5 or not libdvbv5.found()
  incdir = include_directories('lib/include')
  subdir('lib')
endif

res = []
bin_c_args = ['-DVERSION="' + meson.project_version() + '"']

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

subdir('gtk')

