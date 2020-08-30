### Dvbv5-Gtk - Gtk+3 interface to [DVBv5 tool](https://www.linuxtv.org/wiki/index.php/DVBv5_Tools)

* Scan, Zap, Fe
* Zap: Preview / Record / Stream
* Drag and Drop: Scan, Zap

#### Based in [v4l-utils-1.20.0](https://linuxtv.org/downloads/v4l-utils/)

#### License

* libdvbv5: [GNU Lesser General Public License 2.1](License-lib)
* dvbv5-gtk: [GNU General Public License 2.0](License)

#### Dependencies

* libudev ( & dev )
* libdvbv5 ( & dev )
* libgtk 3.0 ( & dev )
* ( for light - no need ) gstreamer-base-1.0 ( & dev )

#### Build

1. Clone: git clone git@github.com:vl-nix/dvbv5-gtk.git

2. Configure:
   * Library ( & dev ): meson build --prefix /usr --strip -D lib=true
   * Binary: meson build --prefix /usr --strip
     1. Light: meson build --prefix /usr --strip -D light=true ( Without - Preview / Record / Stream )

3. Build: ninja -C build

4. Install: sudo ninja -C build install

5. Uninstall:
   * Library: sudo ninja -C build uninstall
   * Binary: sudo ninja -C build uc uninstall
     1. Schema: sudo ninja -C build uc

6. Debug: DVB_DEBUG=1 build/gtk/src/dvbv5-gtk
