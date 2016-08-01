all: built


test: built
	pebble install --emulator aplite

install: built
	pebble install --phone phone

built:
	pebble build
