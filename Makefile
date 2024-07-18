all: client server parserver
	@echo "ready..."
client: client.o
	@gcc client.o -o client
client.o: client.c
	@gcc -c client.c
server: server.o
	@gcc server.o -o server
server.o: server.c
	@gcc -c server.c
parserver: parserver.o
	@gcc parserver.o -o parserver
parserver.o: parserver.c
	@gcc -c parserver.c
clean:
	@echo "cleaning ..."
	@rm *.o server client parserver
