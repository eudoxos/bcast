build:
	meson . build-native
	ninja -C build-native
	crossroad w64 test1 --run cross-build.sh
run:
	echo 'Don't forget to run sender.py with the same IP:port to receive something.'
	echo =========== NATIVE ==============
	build-native/broad
	echo =========== WINE ==============
	crossroad w64 test1 --run cross-run.sh

