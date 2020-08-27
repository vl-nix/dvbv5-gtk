### Dvbv5-Light - Gtk+3 interface to [DVBv5 tool](https://www.linuxtv.org/wiki/index.php/DVBv5_Tools)

* Scan, Zap, Fe

#### License

* [GNU General Public License 2.0](License)

#### Dependencies

* libdvbv5 ( & dev )
* libgtk or libgtkmm ( & dev )

#### Build

1. Configure: meson build --prefix /usr --strip
   * Gtkmm: cd gtkmm; meson build --prefix /usr --strip

2. Build: ninja -C build

3. Install: ( sudo ) ninja -C build install

4. Uninstall: ( sudo ) ninja -C build uninstall
