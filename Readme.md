### [Dvbv5-Gtk](https://github.com/vl-nix/dvbv5-gtk)

* Gtk interface to [DVBv5 tool](https://www.linuxtv.org/wiki/index.php/DVBv5_Tools)
* Scan & Zap & Fe
* Drag and Drop & Command line argument:
  * Initial file -> Scan; dvb_channel.conf -> Zap


#### Dependencies

* gcc
* meson
* libudev ( & dev )
* libdvbv5 ( & dev )
* libgtk 3.0 ( & dev )


#### Quality

* Good - Magenta; Ok - Aqua; Poor - Orange


#### Build

1. Clone: git clone https://github.com/vl-nix/dvbv5-gtk.git

2. Configure: meson build --prefix /usr --strip

3. Build: ninja -C build

4. Install: ninja install -C build

5. Uninstall: ninja uninstall -C build

