CFLAGS=
all:server client
server:server.c
	gcc $^ -o $@ $(CFLAGS)
client:client.c
	gcc $^ -o $@ $(CFLAGS)
clean:
	rm -f server client
