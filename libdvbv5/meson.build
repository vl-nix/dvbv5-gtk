libversion = '@0@.@1@.@2@'.format(0, 0, 0)

c = run_command('sh', '-c', 'for file in *.h *.c ./*/*.c; do echo $file; done')
lib_src = c.stdout().strip().split('\n')

lib_both = both_libraries('dvbv5', sources: lib_src, version: libversion, soversion: 0, 
  include_directories: incdir, dependencies: libdepend, c_args: lib_c_args, install: dev_libdvbv5)

if dev_libdvbv5

  c = run_command('sh', '-c', 'for file in ../include/libdvbv5/*.h; do echo $file; done')
  dvbv5_public_h = c.stdout().strip().split('\n')

  install_headers(dvbv5_public_h, install_dir: join_paths(get_option('prefix'), get_option('includedir'), project_name))

  pkg = import('pkgconfig')
  pkg.generate( name: project_name, description: 'DVBv5 utility library', requires: [udevdepend], 
    version: project_version, libraries: lib_both.get_shared_lib(), install_dir: join_paths(libdir, 'pkgconfig'))

endif
