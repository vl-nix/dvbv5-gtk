project('dvbv5-gtk', 'c', version: '22.11', meson_version: '>= 0.50.0', default_options: ['warning_level=2', 'c_std=gnu11'])

c_args = ['-DVERSION="' + meson.project_version() + '"']

dvb5_deps = [dependency('gtk+-3.0', version: '>= 3.22'), dependency('libdvbv5', version: '>= 1.18')]

c = run_command('sh', '-c', 'for file in src/*.h src/*.c; do echo $file; done', check: true)

src = c.stdout().strip().split('\n')

res = []
subdir('data')

src += res

executable(meson.project_name(), src, dependencies: dvb5_deps, c_args: c_args, install: true)
