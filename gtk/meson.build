subdir('data')

c = run_command('sh', '-c', 'for file in src/*.h src/*.c; do echo $file; done')
bin_src = c.stdout().strip().split('\n')

bin_src += res

if libdvbv5.found()
  executable(meson.project_name(), bin_src, dependencies: guidepend, c_args: bin_c_args, install: true)
else
  executable(meson.project_name(), bin_src, include_directories: incdir, dependencies: lib_dep, c_args: bin_c_args, install: true)
endif
