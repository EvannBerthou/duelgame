net_protocol_builder: net_protocol_builder.c
	gcc net_protocol_builder.c -o net_protocol_builder -I.

net_protocol.h: net_protocol_base.h
	./net_protocol_builder > net_protocol.h

main_game.o: main.c ui.c common.c net_protocol.h
	gcc -Wall -Wextra main.c common.c ui.c -o main_game -I ./include -L ./lib -lraylib -lm -ggdb
