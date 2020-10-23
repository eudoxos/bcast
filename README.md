Receiving UDP packet with GLib
===============================

The `listener-glib` is a MWE of receiving UDP packets (as used in [Aravis](https://github.com/AravisProject/aravis)) via GLib's `g_poll`. It currently works under Linux but fails to receive packets under Windows (with mingw32-w64). The failure happens both in Wine and in true Windows and is related to `g_poll` which behaves (somewhat mysteriously) differently under Linux and under Windows.

There is also win32-API version `listener-win32` which is functional under wine, for comparison.

`make build` will compile both native and wine versions, provided you have [crossroad](https://pypi.org/project/crossroad/) installed (run `pip3 install crossroad` if not).

`make run` will run the program both natively and through wine. Before running so, run `python3 sender.py` so that there is some traffic actually coming to the port.

Typically, you will see this (shortened):

```
=========== NATIVE ==============
build-native/listen-glib
Bound to 127.0.0.1:3956
Received 8 bytes from 127.0.0.1.
[...]
==== WINE: WINSOCK2 NATIVE ====
Bound to 127.0.0.1: 3956
Received 8 bytes
[...]
========= WINE: GLIB ==========
Bound to 127.0.0.1:3956

** (process:8): CRITICAL **: 08:28:29.816: g_poll timed out.
```

If you can fix this, you will receive immense gratitude.

This [Gtk+-dev thread](http://gtk.10911.n7.nabble.com/g-io-channel-win32-poll-Problem-on-Windows-td92219.html) from 2017 discusses the exact issue which has since not been fixed in Aravis. It also links [this GLib commit](https://gitlab.gnome.org/GNOME/glib/commit/425a9f5864f69f804f11279b558ff925d421b546) which contains `g_poll` test (by the same LRN) for Windows ([current version](https://gitlab.gnome.org/GNOME/glib/-/blob/master/glib/tests/gpoll.c)).

The misleading portion of the previous discussion was that it had something to do with the broadcast part, which is in fact not correct. It is just simple listening on an IP port for UDP traffic.
