
# server MakeFile

CC=g++ -ggdb -std=c++11
CCR=g++ -std=c++11

basic_server: client.o tcpsocket.o
	$(CC) -o client_debug client.o tcpsocket.o $(CLIB)

clean:
	rm -f *.o core.* server_release server_debug

release: client_r.o tcpSocket_r.o
	$(CCR) -o client_release client.o tcpsocket.o $(CLIB)

client.o:
	$(CC) -c client.cpp

client_r.o:
	$(CCR) -c client.cpp

tcpsocket.o:
	$(CC) -c tcpsocket.cpp

tcpsocket_r.o:
	$(CCR) -c tcpsocket.cpp
