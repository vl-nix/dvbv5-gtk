project('dvbv5-light-gtkmm', 'cpp', version: '1.20.24')

common_flags = [
  '-DVERSION="' + meson.project_version() + '"'
]

cpp = meson.get_compiler('cpp')
add_project_arguments(common_flags, language: 'cpp')

bindir = join_paths(get_option('prefix'), get_option('bindir'))
exec_path = bindir + '/' + meson.project_name()

desktop_conf = configuration_data()
set_desktop = [
  ['NAME', 'Dvbv5-Light-Gtkmm'],
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
  input: '../gtk/desktop',
  output: desktop,
  configuration: desktop_conf,
  install: true,
  install_dir: join_paths('share', 'applications')
)

c = run_command('sh', '-c', 'for file in src/*.h src/*.cc; do echo $file; done')
src = c.stdout().strip().split('\n')

libpthread = cpp.find_library('libpthread', required: true)
guidepend  = [dependency('gtkmm-3.0'), dependency('libdvbv5'), libpthread]

executable(meson.project_name(), src, dependencies: guidepend, install: true)

