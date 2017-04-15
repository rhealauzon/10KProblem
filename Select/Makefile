
# server MakeFile

CC=g++ -ggdb -std=c++11
CCR=g++ -std=c++11

basic_server: select_server.o tcpsocket.o
	$(CC) -o select_server_debug select_server.o tcpsocket.o $(CLIB)

clean:
	rm -f *.o core.* server_release server_debug

release: select_server_r.o tcpSocket_r.o
	$(CCR) -o select_server_release select_server.o tcpsocket.o $(CLIB)

select_server.o:
	$(CC) -c select_server.cpp

select_server_r.o:
	$(CCR) -c select_server.cpp

tcpsocket.o:
	$(CC) -c tcpsocket.cpp

tcpsocket_r.o:
	$(CCR) -c tcpsocket.cpp
