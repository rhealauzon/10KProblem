#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#define BUFFER_LENGTH 1025

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

class TCPSocket
{
    public:
        /** Initializers **/
        TCPSocket(int);
        TCPSocket();

        bool connectServer(int);
        bool connectClient(int, std::string);
        bool startListen(int);

        /** Getters & Setters **/
        int getPort();
        void setFileDescriptorSet(fd_set);
        int getSocketValue();
        void setSocketValue(int);
        std::string getIP();

        /** Connection methods **/
        bool newConnectFound(fd_set);
        TCPSocket acceptConnection();
        void closeSocket();

        /** Sending & receiving data **/
        void sendMessage(std::string);
        std::string receiveMessage();
        char * receiveFileData(int *);
        std::string receiveVariableData(int);
        void sendFileData(char *);
        void sendVariableData(std::string);
        void resetSocket();



    private:
        //socket & port
        int port;
        int sock;

        //0 is server, 1 is client
        bool mode;

        //socket address
        struct sockaddr_in serverAddress;
        struct sockaddr_in clientAddress;
        struct hostent *hostAddress;

        void setAddress(sockaddr_in);


};

#endif //TCPSOCKET_H
