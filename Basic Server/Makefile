
# server MakeFile

CC=g++ -ggdb -std=c++11
CCR=g++ -std=c++11

basic_server: basic_server.o tcpsocket.o
	$(CC) -o basic_server_debug basic_server.o tcpsocket.o $(CLIB)

clean:
	rm -f *.o core.* server_release server_debug

release: basic_server_r.o tcpSocket_r.o
	$(CCR) -o basic_server_release basic_server.o tcpsocket.o $(CLIB)

basic_server.o:
	$(CC) -c basic_server.cpp

basic_server_r.o:
	$(CCR) -c basic_server.cpp

tcpsocket.o:
	$(CC) -c tcpsocket.cpp

tcpsocket_r.o:
	$(CCR) -c tcpsocket.cpp
