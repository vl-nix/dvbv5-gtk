exec_path = bindir + '/' + meson.project_name()

desktop_conf = configuration_data()

set_desktop = [
  ['NAME', 'Dvbv5-Gtk'],
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
  input: 'desktop',
  output: desktop,
  configuration: desktop_conf,
  install: true,
  install_dir: join_paths('share', 'applications')
)

if not get_option('light')

  subdir('icons')

  path_gschema = join_paths(get_option('prefix'), get_option('datadir'), 'glib-2.0/schemas')
  gschema_file = configure_file(input: 'gschema', output: 'org.gnome.dvbv5-gtk.gschema.xml', copy: true)
  install_data(gschema_file, install_dir: path_gschema)

  cmd_compile_schema = 'glib-compile-schemas ' + path_gschema
  meson.add_install_script('sh', '-c', cmd_compile_schema)

  # Uninstall schema: ninja -C build uc
  cmd_rm_schema = 'rm -f ' + path_gschema + '/org.gnome.dvbv5-gtk.gschema.xml ' + path_gschema + '/gschemas.compiled'
  run_target('uc', command: ['sh', '-c', cmd_rm_schema + '; ' + cmd_compile_schema])

endif
