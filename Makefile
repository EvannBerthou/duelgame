all: build/main_game build/server

$(shell mkdir -p build)

build/net_protocol_builder: src/net_protocol_builder.c
	gcc src/net_protocol_builder.c -o build/net_protocol_builder -I./include

include/net_protocol.h: build/net_protocol_builder include/net_protocol_base.h
	./build/net_protocol_builder > ./include/net_protocol.h

build/main_game: src/main.c src/ui.c src/common.c src/command.c include/net_protocol.h
	gcc -Wall -Wextra src/main.c src/common.c src/ui.c src/command.c -o build/main_game \
		-DLOG_PREFIX=\"GAME\" -DDEBUG\
		-I./include -L ./lib/linux -lraylib -lm -ggdb -lpthread

build/server: src/server.c src/common.c include/net_protocol.h
	gcc -Wall -Wextra src/server.c src/common.c -o build/server -DLOG_PREFIX=\"SERVER\" -I./include -ggdb -lm

build/admin: src/admin.c src/common.c src/command.c include/net_protocol.h
	gcc src/admin.c src/common.c src/command.c -o build/admin -DLOG_PREFIX=\"ADMIN\" -I./include

run: build/server build/main_game
	killall server || true
	killall main_game || true
	./build/server &
	./build/main_game &
	./build/main_game

packer:
	gcc src/packer.c -o build/packer -I./include -L./lib/linux -lraylib -lm
	./build/packer
	xxd -i -n assets_pak assets.pak > include/assets_packed.h

PACKER_MODE=-DEMBED_ASSETS
#TODO: Static linking
release/main_game: packer src/main.c src/ui.c src/common.c src/command.c include/net_protocol.h
	gcc -Wall -Wextra src/main.c src/common.c src/ui.c src/command.c -o build/main_game_release \
		-DLOG_PREFIX=\"GAME\" \
		$(PACKER_MODE) \
		-I./include -L ./lib/linux -lraylib -lm -ggdb -lpthread

clean:
	-rm -rf build
	-rm ./include/net_protocol.h
	-rm ./assets.pak
	-rm ./include/assets_packed.h

.PHONY: all packer clean

windows: packer include/net_protocol.h
	x86_64-w64-mingw32-gcc -Wall -Wextra src/main.c src/common.c src/ui.c src/command.c -o build/main_game_windows \
		-DLOG_PREFIX=\"GAME\" \
		-DWINDOWS_BUILD \
		$(PACKER_MODE) \
		-static \
		-I./include -L ./lib/windows -lraylib -lm -ggdb -lpthread -lwinmm -lgdi32 -lws2_32
