/**********************************************************************
**	SOURCE FILE:	tcpsocket.cpp - Custom TCP socket wrapper class
**
**	PROGRAM:	File Transfer
**
**	FUNCTIONS:
**      TCPSocket(int);
**      TCPSocket();
**      bool connectServer(int);
**      bool connectClient(int, string);
**      bool startListen(int);
**      int getPort();
**      void setFileDescriptorSet(fd_set);
**      int getSocketValue();
**      void setSocketValue(int);
**      string getIP();
**      bool newConnectFound(fd_set);
**      TCPSocket acceptConnection();
**      void closeSocket();
**      void sendMessage(string);
**      string receiveMessage();
**      char * receiveFileData(int *);
**      void sendFileData(char *);
**
**	DATE: 		September 27th, 2015
**
**
**	DESIGNER:	Rhea Lauzon A00881688
**
**
**	PROGRAMMER: Rhea Lauzon A00881688
**
**	NOTES:
** Wrapper class of a TCP socket. Though not perfect, this class
** happily creates TCP sockets for server and client purposes
** as well as handles accepts, listens, message transfers, message receiving,
** and binary data.
*************************************************************************/
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <netdb.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <string>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include "tcpsocket.h"

using namespace std;


/*****************************************************************
** Function: TCPSocket
**
** Date: September 26th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			TCPSocket()
**
** Returns:
**			N/A
**
** Notes:
** Base constructor for the TCPSocket.
*********************************************************************/
TCPSocket::TCPSocket()
{
    //empty Initializer
    port = 0;
}


/*****************************************************************
** Function: TCPSocket
**
** Date: September 26th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			TCPSocket(int sockVal)
**          int sockVal -- a socket descriptor that has been initialized
**
** Returns:
**			N/A
**
** Notes:
** Secondary default constructor for the socket
*********************************************************************/
TCPSocket::TCPSocket(int sockVal)
{
        sock = sockVal;
        port = 0;
}


bool TCPSocket::basicInitialize(int portNum, string address)
{
    port = portNum;

    //create a new socket
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        return false;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    //get the host by name
    if ((hostAddress = gethostbyname(address.c_str())) == NULL)
    {
        //unable to get host; unknown server address
        cerr << "Unable to get host" << endl;
        return false;
    }

    bcopy(hostAddress->h_addr, (char *)&serverAddress.sin_addr, hostAddress->h_length);

    int arg = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1)
    {
        cerr << "Failed to set socket options.";
        return false;
    }

    return true;
}

/*****************************************************************
** Function: connectServer
**
** Date: September 27th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			bool connectServer(int portNum)
**          int portNum -- port to connect to
**
** Returns:
**			bool -- true if the socket is able to bind successfully
**               -- false if there is issues
** Notes:
** Creates a TCP socket server-style, that is, for other clients to
** connect to.
*********************************************************************/
bool TCPSocket::connectServer(int portNum)
{
    port = portNum;

    //create the stream (TCP) socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        cerr << "Cannot create socket.";
        return false;
    }

    //set the REUSEADDR for port reuse
    int arg = 1;

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(int)) == -1)
    {
        cerr << "Failed to set socket options." << endl;
        return false;
    }

    //setup the server address structur
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind the address
    if (bind(sock, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1)
    {
        cerr << "Failed to bind the server-style socket." << endl;
        return false;
    }

    mode = 0;
    return true;
}

/*****************************************************************
** Function: connectClient
**
** Date: September 27th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			bool connectClient(int portNum, string address)
**          int portNum -- port to connect to
**          string addresss -- location of the server
**
** Returns:
**			bool -- true if the socket is able to bind successfully
**               -- false if there is issues
** Notes:
** Creates a TCP socket client-style and connects to a server.
*********************************************************************/
bool TCPSocket::connectClient(int portNum, string address)
{
    port = portNum;

    //create a new socket
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        return false;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    //get the host by name
    if ((hostAddress = gethostbyname(address.c_str())) == NULL)
    {
        //unable to get host; unknown server address
        cerr << "Unable to get host" << endl;
        return false;
    }

    bcopy(hostAddress->h_addr, (char *)&serverAddress.sin_addr, hostAddress->h_length);

    int arg = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1)
    {
        cerr << "Failed to set socket options.";
        return false;
    }

    //initialize the connection with the server
    if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        //failed to connect
        cerr << "Failed to connect to server." << endl;
        return false;
    }

    mode = 1;
    return true;
}


/*****************************************************************
** Function: basicConnect
**
** Date: February 5th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			bool basicConnect()
**
** Returns:
**			bool -- true if the socket is able to bind successfully
**               -- false if there is issues
** Notes:
** Connects an already initialized socket to a server
*********************************************************************/
bool TCPSocket::basicConnect()
{
    //initialize the connection with the server
    if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        //failed to connect
        cerr << "Failed to connect to server." << endl;
        return false;
    }

    mode = 1;
    return true;
}

/*****************************************************************
** Function: startListen
**
** Date: September 27th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			bool startListen(int maxQueued)
**          int maxQueued -- Max number of sockets to queue
**
** Returns:
**			bool -- True if the socket successfully listens
**               -- false if it fails to be set to listen
**
** Notes:
** Sets the socket into listening mode.
*********************************************************************/
bool TCPSocket::startListen(int maxQueued)
{
    //set the socket into listening
    if (listen(sock, maxQueued) < 0)
    {
        cerr << "Unable to starting listening" << endl;
        return false;
    }

    return true;
}


/*****************************************************************
** Function: setFileDescriptorSet
**
** Date: September 28th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			void setFileDescriptorSet(fd_set allSockets)
**          fd_set allSockets -- Set of socket file descriptors
**
** Returns:
**			void
**
** Notes:
** Sets a group of socket file descriptors onto a socket for select
** call usages.
*********************************************************************/
void TCPSocket::setFileDescriptorSet(fd_set allSockets)
{
    //set the file descriptors on the socket
    FD_SET(sock, &allSockets);
}


/*****************************************************************
** Function: getSocketValue
**
** Date: September 28th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			int getSocketValue()
**
** Returns:
**			int -- Socket value
**
** Notes:
** Returns the socket value.
*********************************************************************/
int TCPSocket::getSocketValue()
{
    return sock;
}


/*****************************************************************
** Function: setSocketValue
**
** Date: September 28th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			void setSocketValue(int val)
**          int val -- New value of a socket
**
** Returns:
**			void
**
** Notes:
** Sets the socket value to a new value.
*********************************************************************/
void TCPSocket::setSocketValue(int val)
{
    sock = val;
}


/*****************************************************************
** Function: getPort
**
** Date: September 29th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			int getPort()
**
** Returns:
**			int -- port number
**
** Notes:
** Returns the port number that the socket is connected to.
*********************************************************************/
int TCPSocket::getPort()
{
    return port;
}


/*****************************************************************
** Function: newConnectFound
**
** Date: September 30th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			bool newConnectFound(fd_set readSet)
**
** Returns:
**			bool -- true if there is a new connection awaiting
**               -- false if there is no new connections
**
** Notes:
** Check if a new connection is present in the ready set of sockets.
*********************************************************************/
bool TCPSocket::newConnectFound(fd_set readySet)
{
    if (FD_ISSET(sock, &readySet))
    {
        return true;
    }

    return false;
}


/*****************************************************************
** Function: acceptConnection
**
** Date: September 30th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			TCPSocket acceptConnection()
**
** Returns:
**			TCPSocket -- New socket that was created out of the accept
**
** Notes:
** Accepts a new connection to the server and creates a new socket.
*********************************************************************/
TCPSocket TCPSocket::acceptConnection()
{
    unsigned int numClients = sizeof(clientAddress);

    //create the new socket
    int newSocketVal;

    sockaddr_in client;
    client.sin_family = AF_INET;
    socklen_t c_len = sizeof(client);


    //accept the new socket
    if ((newSocketVal = accept(sock, (struct sockaddr *) &client, &c_len)) == -1)
    {
        cerr << "Accept error";
    }

    //update the new socket
    TCPSocket newSocket(newSocketVal);
    newSocket.setAddress(client);

    return newSocket;
}


/*****************************************************************
** Function: setAddress
**
** Date: October 4th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			void setAddress(sockaddr_in c)
**          sockaddr_in -- client address to connect to
**
** Returns:
**			void
**
** Notes:
** Sets the socket's client address to the specified.
*********************************************************************/
void TCPSocket::setAddress(sockaddr_in c)
{
    clientAddress = c;
}


/*****************************************************************
** Function: getIP
**
** Date: October 4th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			string getIP()
**
** Returns:
**			string -- IP in string format
**
** Notes:
** Fetches the IP of the socket
*********************************************************************/
string TCPSocket::getIP()
{
    stringstream ss;

     ss << inet_ntoa(clientAddress.sin_addr);

    return ss.str();
}


/*****************************************************************
** Function: sendMessage
**
** Date: September 28th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			void sendMessage(string message)
**          string message -- message to send
**
** Returns:
**			void
**
** Notes:
** Sends a message over the socket.
*********************************************************************/
void TCPSocket::sendMessage(string message)
{
    //send the message to the server
    if (send(sock, message.c_str(), BUFFER_LENGTH, 0) < 0)
    {
        //perror("Send Message Error:");
    }
}


/*****************************************************************
** Function: sendFileData
**
** Date: September 30th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			void sendFileData(char *data)
**          char * data -- message to send
**
** Returns:
**			void
**
** Notes:
** Sends a message over the socket. Used for binary data
** which has null terminators around.
*********************************************************************/
void TCPSocket::sendFileData(char * data)
{
    //send a message
    if (send(sock, data, BUFFER_LENGTH, 0) < 0)
    {
        perror("Send File Data Error");
    }
}


/*****************************************************************
** Function: sendVariableData
**
** Date: January 30th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			void sendFileData(string data)
**          cstring data -- message to send
**
** Returns:
**			void
**
** Notes:
** Sends a message for use of variable data sizes.
*********************************************************************/
void TCPSocket::sendVariableData(string data)
{
    //send a message
    if (send(sock, data.c_str(), data.size(), 0) < 0)
    {
        perror("Send Variable Data Error:");
    }
}


/*****************************************************************
** Function: receiveMessage
**
** Date: September 30th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			string receiveMessage()
**
** Returns:
**			string -- Message received
**
** Notes:
** Receives a message from the socket
*********************************************************************/
string TCPSocket::receiveMessage()
{
    char readBuffer[BUFFER_LENGTH + 1] = {'\0'};

    int n = 0;

    //block on waiting for data
    n = recv(sock, &readBuffer, BUFFER_LENGTH, 0);

    return readBuffer;
}


/*****************************************************************
** Function: receiveVariableData
**
** Date: January 31st, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			string receiveVariableData(int amount)
**          int amount -- how much data should be waited for
**
** Returns:
**			string -- Message received
**
** Notes:
** Receives a message from the socket
*********************************************************************/
string TCPSocket::receiveVariableData(int amount)
{
    char readBuffer[amount + 1] = {'\0'};

    int n = 0;

    //block on waiting for data
    n = recv(sock, &readBuffer, amount, 0);

    return readBuffer;
}

/*****************************************************************
** Function: receiveFileData
**
** Date: September 30th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			char * receiveFileData(int * n)
**			int *n -- Number of bytes received
**
** Returns:
**			char * -- Message received
**
** Notes:
** Receives a message from the socket (used for binary issues
** with null pointers in the characters)
*********************************************************************/
char * TCPSocket::receiveFileData(int * n)
{
    char * readBuffer = new char[BUFFER_LENGTH + 1];

    //block on waiting for data
    *n = recv(sock, readBuffer, BUFFER_LENGTH, 0);

    return readBuffer;
}


/*****************************************************************
** Function: closeSocket
**
** Date: October 2nd, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			void closeSocket()
**
** Returns:
**			void
**
** Notes:
** Closes a tcp socket, ending the connection.
*********************************************************************/
void TCPSocket::closeSocket()
{
    close(sock);
}
