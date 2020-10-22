== Broadcast discovery with GLib

This code is a MWE of discovery via broadcast, as used in [Aravis](https://github.com/AravisProject/aravis). The discovery code currently works under Linux but fails to receive packets under Windows (with mingw32-w64). The failure happens both in Wine and in true Windows.

`make build` will compile both native and wine versions, provided you have [crossroad](https://pypi.org/project/crossroad/) installed (run `pip3 install crossroad` if not).

`make run` will run the program both natively and through wine. Typically, you will see this (shortened):

```
=========== NATIVE ==============
build-native/broad 192.168.2.12
Sent 8 bytes discover to 255.255.255.255 over 192.168.2.12
Received 256 bytes from 192.168.2.123.
=========== WINE ==============
Sent 8 bytes discover to 255.255.255.255 over 192.168.2.12

** (process:8): CRITICAL **: 11:31:15.436: g_poll timed out.
```

If you can fix this, you will receive immense gratitude.

This [Gtk+-dev thread](http://gtk.10911.n7.nabble.com/g-io-channel-win32-poll-Problem-on-Windows-td92219.html) discusses the exact issue. It also links [this GLib commit](https://gitlab.gnome.org/GNOME/glib/commit/425a9f5864f69f804f11279b558ff925d421b546) which contains `g_poll` test (by the same LRN) for Windows ([current version](https://gitlab.gnome.org/GNOME/glib/-/blob/master/glib/tests/gpoll.c)). 
