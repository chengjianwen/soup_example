all: client server
CFLAGS = `pkg-config --cflags libsoup-2.4`
LIBS   = `pkg-config --libs libsoup-2.4`

server: server.o
	$(CC) $(LIBS) -o $@ $<
client: client.o
	$(CC) $(LIBS) -o $@ $<
