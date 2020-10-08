### Dvbv5-Gtk - Gtk+3 interface to [DVBv5 tool](https://www.linuxtv.org/wiki/index.php/DVBv5_Tools)

* Scan, Zap, Fe
* Drag and Drop: Scan, Zap

#### Dependencies

* libudev ( & dev )
* libdvbv5 ( & dev )
* libgtk 3.0 ( & dev )
* ( for light - no need ) gstreamer-base-1.0 ( & dev )

#### Dvr

* Zap: Preview / Record / Stream

#### Demux

* Zap ( TreeViewColumn ) Prw: Preview
* Zap ( TreeViewColumn ) Rec: Record
* All channels from the current transponder

#### Quality

* Good - Magenta; Ok - Aqua; Poor - Orange

#### Build

1. Clone: git clone git@github.com:vl-nix/dvbv5-gtk.git

2. Configure: meson build --prefix /usr --strip
   * Light: meson build --prefix /usr --strip -D light=true ( Without - Preview / Stream )
   * Library: meson build --prefix /usr --strip -D lib=true

3. Build: ninja -C build

4. Install: sudo ninja -C build install

5. Uninstall: sudo ninja -C build uc uninstall
   * Schema: sudo ninja -C build uc

6. Debug: DVB_DEBUG=1 dvbv5-gtk

