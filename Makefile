build:
	meson . build-native
	ninja -C build-native
	crossroad w64 test1 --run cross-build.sh
run:
	echo =========== NATIVE ==============
	build-native/broad 192.168.2.12
	echo =========== WINE ==============
	crossroad w64 test1 --run cross-run.sh

