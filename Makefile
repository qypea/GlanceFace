all: built


test: built
	pebble install --emulator aplite

install: built
	pebble install --phone phone
	scp build/GlanceFace.pbw apache.local:/var/www/basic/htdocs/files/

built:
	pebble build
