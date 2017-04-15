/**********************************************************************
**	SOURCE FILE:	basic_server.cpp - Server made using select
**
**	PROGRAM:	Scalable Server -- Select-based
**
**	FUNCTIONS:
** int createChildren(int)
** int waitForData()
** void selectState()
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
** This server uses select to handle clients.
*************************************************************************/
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include "tcpsocket.h"
#include "select_server.h"

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
const string END_CONNECTION_MSG = "Goodbye!";

//fork return for signal checking
int pId;

/* Signal handler structures */
struct sigaction SA;
struct sigaction old;


/*****************************************************************
** Function: main
**
** Date: February 7th, 2016
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

    // Initialize the clients array
    for (int i = 0; i < FD_SETSIZE; i++)
    {
        clients[i] = -1;
    }

    FD_ZERO(&allSockets);
    FD_SET(listenSocket.getSocketValue(), &allSockets);

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

    //wait for data on the main process
    waitForData();

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
** Creates a number of child processes, each of which
** go forward to find prime numbers.
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
                 selectState();
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
** Function: waitForData
**
** Date: February 6th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**		    int waitForData()
**
** Returns:
**			int -- 0 on successful return
*               -- -1 on a failure
**
** Notes:
** Waits for data from the children on the shared pipe.
**********************************************************************/
int waitForData()
{
    cout << "===================================" << endl;
    cout << "Waiting for connections:" << endl;
    cout << "===================================" << endl;

    char in_buff[PIPE_BUFFER_LENGTH];

    int totalConnections = 0;
    int currentConnections = 0;

    //keep checking for new data on the pipe
    while (true)
    {
        //a new value has been added
        if (read(sharedPipe[0], in_buff, PIPE_BUFFER_LENGTH) > 0)
        {
            //client has connected
            if (strcmp(PROCESS_CONNECTED_MSG.c_str(), in_buff) == 0)
            {
                //increment the current connections & total count
                currentConnections++;
                totalConnections++;
            }

            //a client has finished
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
** Function: selectState
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
**		    void selectState()
**
** Returns:
**			void
**
** Notes:
** Uses select to accept new connections as well as read data from clients
** and close them when they are done.
**********************************************************************/
void selectState()
{
    //close the pipe's read description
    close(sharedPipe[0]);

    //prepare for listening
    int numReadySockets;
    maxFileDescriptors = listenSocket.getSocketValue();
    maxIndex = -1;

    while(true)
    {
        //set the ready set to all sockets
        readySet = allSockets;

        //block until a new connection or data is received
        numReadySockets = select(maxFileDescriptors + 1, &readySet, NULL, NULL, NULL);

        //Unblock; check for connection or data!
        if (listenSocket.newConnectFound(readySet))
        {
            //accept the newly found connection
            acceptConnection();

            //make sure there isn't other sockets to handle
            if ((--numReadySockets) <= 0)
            {
                continue;
            }
        }

        //if not a connection there must be data!
        int socketFileDescriptor;

        //check all clients for data
        for (int i = 0; i <= maxIndex; i++)
        {
            socketFileDescriptor = clients[i].getSocketValue();

            //check if the file descriptor was set
            if(socketFileDescriptor < 0)
            {
                continue;
            }

            if(FD_ISSET(socketFileDescriptor, &readySet))
            {
                if (readData(socketFileDescriptor) == 0)
                {
                    close(socketFileDescriptor);
                    FD_CLR(socketFileDescriptor, &allSockets);
                    clients[i].resetSocket();
                }

                if(--numReadySockets <= 0)
                {
                    break;
                }
            }
        }
    }
}


/*****************************************************************
** Function: acceptConnection
**
** Date: February 7th, 2016
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
    //accept the connection
    TCPSocket newClient = listenSocket.acceptConnection();

    if (newClient.getSocketValue() == -1)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            cerr << "Issue accepting new clients" << endl;
            return -1;
        }

        else if (errno == EAGAIN)
        {
            errno = 0;
            return 0;
        }
    }

    if (fcntl(newClient.getSocketValue(), F_SETFL, O_NONBLOCK | fcntl(newClient.getSocketValue(), F_GETFL, 0)) == -1)
    {
        perror("Failed to set to non-blocking");
        cout << "Unable to make the new socket non-blocking" << endl;
        return -1;
    }


	int i;
	for (i = 0; i < FD_SETSIZE; i++)
	{
		if (clients[i].getSocketValue() < 0)
		{
			clients[i] = newClient;
			break;
		}
	}

	if (i == FD_SETSIZE)
	{
		cerr << "Too many clients." << endl;
		return -1;
	}

	//add the new descriptor to the list
	FD_SET(newClient.getSocketValue(), &allSockets);

	if(newClient.getSocketValue() > maxFileDescriptors)
	{
		maxFileDescriptors = newClient.getSocketValue();
	}

	if ( i > maxIndex)
	{
		maxIndex = i;
	}

    //notify the parent that there is a new connection
    write(sharedPipe[1], PROCESS_CONNECTED_MSG.c_str(), PIPE_BUFFER_LENGTH);

	return 0;
}

/*****************************************************************
** Function: readData
**
** Date: February 7th, 2016
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

    // read all of the data and echo it back until there is no more
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
