libversion = '@0@.@1@.@2@'.format(0, 0, 0)

incdir = include_directories('include')

subdir('include')

lib_clone ='git clone git://linuxtv.org/v4l-utils.git'
cmd_link_i = 'ln -s ../v4l-utils/lib/include/libdvbv5 include/libdvbv5'
cmd_link_ig = 'ln -s ../v4l-utils/include/gettext.h include/gettext.h'
cmd_link_ip = 'ln -s ../v4l-utils/lib/libdvbv5/parse_string.h include/parse_string.h'
cmd_link_l = 'ln -s v4l-utils/lib/libdvbv5 libdvbv5'

run_command('sh', '-c', 'if ! [ -d v4l-utils/ ]; then ' + lib_clone + '; fi')
run_command('sh', '-c', cmd_link_i)
run_command('sh', '-c', cmd_link_ig)
run_command('sh', '-c', cmd_link_ip)
run_command('sh', '-c', cmd_link_l)

lib_c_args = '-D_GNU_SOURCE'

libm        = cc.find_library('libm', required: true)
librt       = cc.find_library('librt', required: true)
libpthread  = cc.find_library('libpthread', required: true)

udevdepend = [dependency('libudev')]
libdepend  = [udevdepend, libm, librt, libpthread]

c = run_command('sh', '-c', 'for file in libdvbv5/*.h libdvbv5/*.c libdvbv5/*/*.c; do echo $file; done')
lib_src = c.stdout().strip().split('\n')

lib_both = both_libraries('dvbv5', sources: lib_src, version: libversion, soversion: 0, 
  include_directories: incdir, dependencies: libdepend, c_args: lib_c_args, install: true)


#if dev_libdvbv5

  c = run_command('sh', '-c', 'for file in include/libdvbv5/*.h; do echo $file; done')
  dvbv5_public_h = c.stdout().strip().split('\n')

  install_headers(dvbv5_public_h, install_dir: join_paths(get_option('prefix'), get_option('includedir'), 'libdvbv5'))

  pkg = import('pkgconfig')
  pkg.generate( name: 'libdvbv5', description: 'DVBv5 utility library', requires: [udevdepend], 
    version: '1.20.0', libraries: lib_both.get_shared_lib(), install_dir: join_paths(libdir, 'pkgconfig'))

#endif
