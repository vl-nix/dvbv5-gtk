project('dvbv5-light-gtk', 'c', version: '1.20.24')

common_flags = [
  '-DVERSION="' + meson.project_version() + '"',
  '-D_GNU_SOURCE'
]

cc = meson.get_compiler('c')
add_project_arguments(common_flags, language: 'c')

bindir = join_paths(get_option('prefix'), get_option('bindir'))
exec_path = bindir + '/' + meson.project_name()

desktop_conf = configuration_data()
set_desktop = [
  ['NAME', 'Dvbv5-Light-Gtk'],
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

c = run_command('sh', '-c', 'for file in gtk/src/*.h gtk/src/*.c; do echo $file; done')
src = c.stdout().strip().split('\n')

guidepend  = [dependency('gtk+-3.0'), dependency('libdvbv5')]

executable(meson.project_name(), src, dependencies: guidepend, install: true)
