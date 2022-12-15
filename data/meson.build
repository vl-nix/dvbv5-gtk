subdir('icons')

desktop = meson.project_name() + '.desktop'
r = run_command('sh', '-c', 'echo "[Desktop Entry]\nName=Dvbv5-Gtk\nComment=DVBv5 tool\nExec=dvbv5-gtk\nIcon=dvbv5-gtk\nTerminal=false\nType=Application\nCategories=GTK;AudioVideo;Player;Video;\n" > desktop', check: true)
configure_file(input: 'desktop', output: 'dvbv5-gtk.desktop', copy: true, install: true, install_dir: join_paths('share', 'applications'))

meson.add_install_script('sh', '-c', 'gtk-update-icon-cache -qtf /usr/share/icons/hicolor')
meson.add_install_script('sh', '-c', 'update-desktop-database -q /usr/share/applications')
