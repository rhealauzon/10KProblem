/**********************************************************************
**	SOURCE FILE:	epoll_server.cpp - Server made using Epoll
**
**	PROGRAM:	Scalable Server -- Epoll based server
**
**	FUNCTIONS:
** int createChildren(int)
** int receiveOnPipe()
** int epollState()
** void controlHandler(int)
** int acceptConnection()
** int readData(int)
**
**	DATE: 		February 7th, 2016
**
**
**	DESIGNER:	Rhea Lauzon A00881688
**
**
**	PROGRAMMER: Rhea Lauzon A00881688
**
**	NOTES:
** This server uses EPoll to accept and handle clients. Can handle
** over 10,000 clients simulatenously. 
*************************************************************************/
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <assert.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "tcpsocket.h"
#include "epoll_server.h"

using namespace std;

/** Listening socket for new clients **/
TCPSocket listenSocket;

/** Select variables **/
//client list
TCPSocket clients[FD_SETSIZE];
fd_set allSockets;
fd_set readySet;
int maxFileDescriptors;
int maxIndex;

/** Shared pipe for communication **/
int sharedPipe[2];
vector<int> children;
const string PROCESS_DONE_MSG = "Process Done";
const string PROCESS_CONNECTED_MSG = "Process Connected";

//fork return for signal checking
int pId;

/* Signal handler structures */
struct sigaction SA;
struct sigaction old;

int epollDescriptor;

/*****************************************************************
** Function: main
**
** Date: February 8th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**		     int main()
**
** Returns:
**			int -- 0 on successful return
**              -- -1 if an error occurs
**
** Notes:
** Connects the listening socket and creates the pool of worker
** processes that will be handling clients.
**********************************************************************/
int main()
{
    //initialize the listening socket & bind it
    if (!listenSocket.connectServer(LISTENING_PORT))
    {
        return SOCKET_ERROR;
    }

    //set the listening socket into non blocking
    if (fcntl(listenSocket.getSocketValue(), F_SETFL, O_NONBLOCK | fcntl(listenSocket.getSocketValue(), F_GETFL, 0)) == -1)
    {
        cerr << "Unable to set listening socket to non-blocking" << endl;
        return -1;
    }

    //set the socket into listening mode
    if(!listenSocket.startListen(MAX_QUEUED))
    {
        return SOCKET_ERROR;
    }

    //make the pipe
    if (pipe(sharedPipe) < 0)
    {
        cerr << "Unable to create pipe." << endl;
        exit(RETURN_ERROR);
    }

    //set up signal handler
	SA.sa_handler = controlHandler;
	sigemptyset(&SA.sa_mask);
	sigaction(SIGINT, &SA, &old);

    //create the children
    createChildren(MIN_FREE_PROCESSES);

    //wait for data on the main process via the pipe
    receiveOnPipe();

    return 0;
}

/*****************************************************************
** Function: createChildren
**
** Date: February 4th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**		    int createChildren(int numChildren)
**              int numChildren -- number of children to create
**
** Returns:
**			int -- 0 on successful return
**              -- -1 on a failure
**
** Notes:
** enter the epoll state and wait for new connections.
** Creates a number of child processes, each of which will
**********************************************************************/
int createChildren(int numChildren)
{
    pid_t processId = -2;

    //create all the workers
    for (int i = 0; i <= numChildren + 1; i++)
    {
        switch (processId)
        {
            //fork error
            case -1:
                cerr << "Error creating a child process." << endl;
                return RETURN_ERROR;
            break;

            //child process
            case 0:
                 epollState();
                 _exit(0);
            break;

            //parent process
            default:
                if (i < numChildren)
                {
                    //fork off a new child
                    processId = fork();
                    children.push_back(processId);
                    pId = processId;
                }
            break;
        }
    }

    printf("%d children created.\n", numChildren);
}

/*****************************************************************
** Function: receiveOnPipe
**
** Date: February 8th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**		    int receiveOnPipe()
**
** Returns:
**			int -- 0 on successful return
*               -- -1 on a failure
**
** Notes:
** Waits for data from the children.
**********************************************************************/
int receiveOnPipe()
{
    cout << "===================================" << endl;
    cout << "Waiting for connections:" << endl;
    cout << "===================================" << endl;

    char in_buff[PIPE_BUFFER_LENGTH];

    int totalConnections = 0;
    int currentConnections = 0;
    int connectionChange = 0;

    //keep checking for new data on the pipe
    while (true)
    {
        //a new value has been added
        if (read(sharedPipe[0], in_buff, PIPE_BUFFER_LENGTH) > 0)
        {
            if (strcmp(PROCESS_CONNECTED_MSG.c_str(), in_buff) == 0)
            {
                //increment the current connections & total count
                currentConnections++;
                totalConnections++;
            }

            else if (strcmp(PROCESS_DONE_MSG.c_str(), in_buff) == 0)
            {
                currentConnections--;
            }

            cout << "-------------------------------------" << endl;
            printf("Current Connections:        %d \n", currentConnections);
            printf("Total Clients:              %d \n", totalConnections);

        }
    }

    return 0;
}


/*****************************************************************
** Function: epollState
**
** Date: February 8th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**		    void epollState()
**
** Returns:
**			void
**
** Notes:
** Handles new connections, closed connections, and data received
** from clients using epoll methods.
**********************************************************************/
int epollState()
{
    //close the pipe's read description
    close(sharedPipe[0]);

    // Create the epoll file descriptor
	epollDescriptor = epoll_create(EPOLL_QUEUE_LEN);
	if (epollDescriptor == -1)
    {
        cerr << "Failed to created Epoll descriptor" << endl;
        return -1;
    }

    //attach the listening socket in its own scope
    {
        struct epoll_event event = epoll_event();
        event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
        event.data.fd = listenSocket.getSocketValue();
        if (epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, listenSocket.getSocketValue(), &event) == -1)
        {
            cerr << "Unable to add listening socket" << endl;
            return -1;
        }
    }

    while (true)
    {
        int numReady;
        static struct epoll_event events[EPOLL_QUEUE_LEN];

        numReady = epoll_wait(epollDescriptor, events, EPOLL_QUEUE_LEN, -1);

        //error occurs
        if (numReady < 0)
        {
            cerr << "Error in epoll_wait" << endl;
        }

        //epoll unblocked by this point; there is socket activity
        for (int i = 0; i < numReady; i++)
        {
            //Error condition
            if (events[i].events & (EPOLLHUP | EPOLLERR))
            {
                if (errno != 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    perror("EPOLL ERROR");
                    cerr << "EPOLL ERROR" << endl;
                    close(events[i].data.fd);
                }
                //someone else has handled this connection
                else
                {
                    errno = 0;
                }
                continue;
            }

            assert(events[i].events&EPOLLIN);

            //there must be data
            if (events[i].data.fd != listenSocket.getSocketValue())
            {
                readData(events[i].data.fd);

                //since this was socket data
                //it doesnt need to check if it was a new connection
                continue;
            }

            //New connection is being made to the listening socket
            if (events[i].data.fd == listenSocket.getSocketValue())
            {
                acceptConnection();
                continue;
            }
        }
    }

    return 0;
}


/*****************************************************************
** Function: acceptConnection
**
** Date: February 8th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**		    int acceptConnection()
**
** Returns:
**			int -- 0 on successful acception of a client
*               -- -1 on a failure
**
** Notes:
** Accepts a new connection that is attempting to connect.
**********************************************************************/
int acceptConnection()
{
    //accept the new connection
	int newClient = accept(listenSocket.getSocketValue(), 0, 0);

	if (newClient == -1)
	{
        //ignore
        if (errno != EAGAIN)
        {
        	perror("accept");
            cerr << "Error accepting" << endl;
            return -1;
        }

        else if (errno == EAGAIN)
        {
            errno = 0;
            return 0;
        }
	}

    //set the new socket into non-blocking
	if (fcntl(newClient, F_SETFL, O_NONBLOCK | fcntl(newClient, F_GETFL, 0)) == -1)
    {
        perror("Failed to set to non-blocking");
        cout << "Unable to make the new socket non-blocking" << endl;
        return -1;
    }

	// Add the new socket descriptor to the epoll loop
    static struct epoll_event event = epoll_event();
    event.events = EPOLLIN|EPOLLERR|EPOLLHUP|EPOLLET;
	event.data.fd = newClient;

	if (epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, newClient, &event) == -1)
    {
        cerr << "Unable to add the new client to epoll" << endl;
		return -1;
    }

    //notify the parent that there is a new client
    write(sharedPipe[1], PROCESS_CONNECTED_MSG.c_str(), PIPE_BUFFER_LENGTH);

	return 0;
}

/*****************************************************************
** Function: readData
**
** Date: February 8th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**		    int readData(int socket)
**
** Returns:
**			int -- returns the number of bytes read
**
** Notes:
** Reads data from the socket until there is no more to be read.
**********************************************************************/
int readData(int socket)
{
    char readBuffer[BUFFER_LENGTH + 1] = {'\0'};
    int numRead;

    // read and echo back to client
   while ((numRead = recv(socket, readBuffer, BUFFER_LENGTH, 0)) > 0)
   {
       send(socket, readBuffer, numRead, 0);
   }

   // close socket if connection is closed by the client (therefore done)
   if (numRead == 0)
   {
       // close socket
       close(socket);

       //notify the parent that this client is finished
       write(sharedPipe[1], PROCESS_DONE_MSG.c_str(), PIPE_BUFFER_LENGTH);
   }

    return numRead;
}

/*****************************************************************
** Function: controlHandler
**
** Date: February 2nd, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**		    void controlHandler(int signal)
**          int signal -- Signal that has been caught
**
** Returns:
**			void
**
** Notes:
** Catches a single (SIGINT generated by ctrl + z or children dying)
** and handles it appropriately.
**********************************************************************/
void controlHandler(int signal)
{
    if (signal == SIGINT)
    {
        if ( pId != 0)
    	{
    		//restore default signal handler
    		sigaction(SIGINT, &old, NULL);

            for (int i = 0; i < children.size(); i++)
            {
                kill(children[i], SIGTERM);
            }

            //close up the pipe
            close(sharedPipe[0]);
            close(sharedPipe[1]);
            listenSocket.closeSocket();
    	}
        exit(0);
    }
}
