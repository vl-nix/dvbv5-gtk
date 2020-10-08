subdir('data')

c = run_command('sh', '-c', 'for file in src/*.h src/*.c; do echo $file; done')
dvbv5_src = c.stdout().strip().split('\n')

dvbv5_src += res

executable(meson.project_name(), dvbv5_src, dependencies: dvbv5_deps, c_args: c_args, install: true)
