all: client server
CFLAGS = `pkg-config --cflags libsoup-2.4 opus` `sdl2-config --cflags`
LIBS   = `pkg-config --libs libsoup-2.4 opus` `sdl2-config --libs`

server: server.o ws_util.o
	$(CC) $(LIBS) -o $@ $^
client: client.o ws_util.o
	$(CC) $(LIBS) -o $@ $^
clean:
	rm -f client server *.o
