project('broad','c',version: '0.0')
deps = [
	dependency ('glib-2.0', version: '>=2.44', required: true),
	dependency ('gobject-2.0', required: true),
	dependency ('gio-2.0', required: true)
]
cc=meson.get_compiler('c')
if host_machine.system()=='windows'
	deps += [
		cc.find_library('ws2_32', required: true),  # socket functions
		cc.find_library('iphlpapi', required: true) # GetAdaptersAddresses
	]
endif
executable('listen-glib',['gpoll_local.c','gpoll_local.h','listen-glib.c'],dependencies: deps)
if host_machine.system()=='windows'
	executable('listen-win32','listen-win32.c',dependencies: deps)
endif
