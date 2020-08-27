### Dvbv5-Gtk - Gtk+3 interface to [DVBv5 tool](https://www.linuxtv.org/wiki/index.php/DVBv5_Tools)

* Scan, Zap, Fe
* Zap: Preview / Record / Stream
* Drag and Drop: Scan, Zap

* Based in [v4l-utils-1.20.0](https://linuxtv.org/downloads/v4l-utils/)

#### License

* libdvbv5: [GNU Lesser General Public License 2.1](License-lib)
* dvbv5-gtk: [GNU General Public License 2.0](License)

#### Dependencies

* libgtk 3.0 ( & dev )
* libudev ( & dev )
* libdvbv5 ( & dev ) - if libdvbv5 is not installed, this library will be built and included in the program
* gstreamer-base-1.0 ( & dev )

#### Build

1. Clone: git clone git@github.com:vl-nix/dvbv5-gtk.git

2. Configure:
   * Binary: meson build --prefix /usr --strip
   * Library: meson build --prefix /usr --strip -D lib=true
   * Library-dev: meson build --prefix /usr --strip -D lib=true -D dev=true

3. Build: ninja -C build

4. Install: ( sudo ) ninja -C build install

5. Uninstall:
   * Library: ( sudo ) ninja -C build uninstall
   * Binary ( full ): ( sudo ) ninja -C build uc uninstall
     1. Schema: ( sudo ) ninja -C build uc
     2. Binary: ( sudo ) ninja -C build uninstall

6. Debug: DVB_DEBUG=1 build/dvbv5-gtk
