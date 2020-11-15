project('dvbv5-gtk', 'c', version: '1.20.70', meson_version: '>= 0.50.0', default_options: ['warning_level=2', 'c_std=gnu11'])

bindir = join_paths(get_option('prefix'), get_option('bindir'))
libdir = join_paths(get_option('prefix'), get_option('libdir'))

cc = meson.get_compiler('c')

if get_option('lib')

  subdir('lib')

else

  c_args = ['-DVERSION="' + meson.project_version() + '"']

  res = []

  dvbv5_deps = [
    dependency('gio-2.0', version: '>= 2.50'),
    dependency('gtk+-3.0', version: '>= 3.22'),
    dependency('libdvbv5', version: '>= 1.18')
  ]

  if get_option('light')
    c_args += ['-DPROGNAME="Dvbv5-Light-Gtk"', '-DLIGHT=1']
  else
    c_args += ['-DPROGNAME="Dvbv5-Gtk"']
    libgstvideo = cc.find_library('libgstvideo-1.0', required: true)
    dvbv5_deps += [dependency('gstreamer-base-1.0'), libgstvideo]
  endif

  subdir('gtk')

endif
