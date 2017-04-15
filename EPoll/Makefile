
# server MakeFile

CC=g++ -ggdb -std=c++11
CCR=g++ -std=c++11

basic_server: epoll_server.o tcpsocket.o
	$(CC) -o epoll_server_debug epoll_server.o tcpsocket.o $(CLIB)

clean:
	rm -f *.o core.* server_release server_debug

release: epoll_server_r.o tcpSocket_r.o
	$(CCR) -o epoll_server_release epoll_server.o tcpsocket.o $(CLIB)

epoll_server.o:
	$(CC) -c epoll_server.cpp

epoll_server_r.o:
	$(CCR) -c epoll_server.cpp

tcpsocket.o:
	$(CC) -c tcpsocket.cpp

tcpsocket_r.o:
	$(CCR) -c tcpsocket.cpp
