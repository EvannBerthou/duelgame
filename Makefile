all: build/main_game build/server build/admin

$(shell mkdir -p build)

build/net_protocol_builder: src/net_protocol_builder.c
	gcc src/net_protocol_builder.c -o build/net_protocol_builder -I./include

include/net_protocol.h: build/net_protocol_builder include/net_protocol_base.h
	./build/net_protocol_builder > ./include/net_protocol.h

build/main_game: src/main.c src/ui.c src/common.c include/net_protocol.h
	gcc -Wall -Wextra src/main.c src/common.c src/ui.c -o build/main_game -I./include -L ./lib -lraylib -lm -ggdb

build/server: src/server.c src/common.c include/net_protocol.h
	gcc -Wall -Wextra src/server.c src/common.c -o build/server -I./include -ggdb -lm

build/admin: src/admin.c src/common.c include/net_protocol.h
	gcc src/admin.c src/common.c -o build/admin -I./include

run: build/server build/main_game
	killall server || true
	killall main_game || true
	./build/server &
	./build/main_game &
	./build/main_game

clean:
	rm -rf build
	rm ./include/net_protocol.h
