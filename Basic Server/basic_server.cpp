/**********************************************************************
**	SOURCE FILE:	basic_server.cpp - Server made with processes
**
**	PROGRAM:	Scalable Server -- Multi-Processed
**
**	FUNCTIONS:
** int createChildren(int)
** int waitForData()
** void waitForClient()
** void connectedState(TCPSocket)
** void controlHandler(int)
**
**	DATE: 		January 4th, 2016
**
**
**	DESIGNER:	Rhea Lauzon A00881688
**
**
**	PROGRAMMER: Rhea Lauzon A00881688
**
**	NOTES:
** Creates a basic server that runs on forking new processes. A pool
** of new processes is forked before any connections occur and is
** topped off everytime it dips below a specific amount.
*************************************************************************/
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "tcpsocket.h"
#include "basic_server.h"

using namespace std;

/** Listening socket for new clients **/
TCPSocket listeningSocket;

/** Shared pipe for communication **/
int sharedPipe[2];

int processesAvail = 0;
vector<int> children;

const string PROCESS_DONE_MSG = "Process Done";
const string PROCESS_CONNECTED_MSG = "Process Connected";

//fork return for signal checking
int pId;

/* Signal handler structures */
struct sigaction SA;
struct sigaction old;


/*****************************************************************
** Function: main
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
**		     int main()
**
** Returns:
**			int -- 0 on successful return
**              -- -1 if an error occurs
**
** Notes:
** Connects the listening socket and creates the initial pool
** of pre-forked processes.
**********************************************************************/
int main()
{
    //initialize the listening socket & bind it
    if (!listeningSocket.connectServer(LISTENING_PORT))
    {
        return SOCKET_ERROR;
    }

    //set the socket into listening mode
    if(!listeningSocket.startListen(MAX_QUEUED))
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
        sigaction(SIGCHLD, &SA, &old);

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
** go forward to block on accept until a client connects
**********************************************************************/
int createChildren(int numChildren)
{
    pid_t processId = -2;
    int numcreated = 0;

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
                 waitForClient();
                 _exit(0);
            break;

            //parent process
            default:
                if (i < numChildren)
                {
                    processesAvail++;
                    numcreated++;
                    //fork off a new child
                    processId = fork();
                    children.push_back(processId);
                    pId = processId;
                }
            break;
        }
    }

    printf("%d children created.\n", numcreated);
}

/*****************************************************************
** Function: waitForData
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
**		    int waitForData()
**
** Returns:
**			int -- 0 on successful return
*               -- -1 on a failure
**
** Notes:
** Waits for data from the children on a pipe.
**********************************************************************/
int waitForData()
{
    cout << "===================================" << endl;
    cout << "Waiting for connections:" << endl;
    cout << "===================================" << endl;

    char in_buff[PIPE_BUFFER_SIZE];

    int totalConnections = 0;
    int currentConnections = 0;

    //keep checking for new data on the pipe
    while (true)
    {
        //a new value has been added
        if (read(sharedPipe[0], in_buff, PIPE_BUFFER_SIZE) > 0)
        {
            if (strcmp(PROCESS_CONNECTED_MSG.c_str(), in_buff) == 0)
            {
                //decrement the number of available processes
                processesAvail--;

                //increment the current connections & total count
                currentConnections++;
                totalConnections++;

                //Top up the number of free processes
                if (processesAvail < MIN_FREE_PROCESSES - NEW_ADDITION_INCREMENT)
                {
                    createChildren(MIN_FREE_PROCESSES);
                }
            }
            else if (strcmp(PROCESS_DONE_MSG.c_str(), in_buff) == 0)
            {
                currentConnections--;
            }

            printf("-------------------------------------\n Current Connections:        %d \n Total Clients:              %d \n", currentConnections, totalConnections);
            cout.flush();
        }
    }

    return 0;
}


/*****************************************************************
** Function: waitForClient
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
**		    void waitForClient()
**
** Returns:
**			void
**
** Notes:
** Waits for a client to connect to the server and accepts it.
**********************************************************************/
void waitForClient()
{
    //close the pipe's read description
    close(sharedPipe[0]);

    //block until a new connection comes in
    TCPSocket newClient = listeningSocket.acceptConnection();

    //notify the parent process that this process is used up now
    write(sharedPipe[1], PROCESS_CONNECTED_MSG.c_str(), PIPE_BUFFER_SIZE);

    connectedState(newClient);
}


/*****************************************************************
** Function: connectedState
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
**		    void connectedState(TCPSocket client)
**          TCPSocket client -- The client that connected to the server
**
** Returns:
**			void
**
** Notes:
** Reads all the client's data and responds until all iterations
** are complete.
**********************************************************************/
void connectedState(TCPSocket client)
{
    //read until the goodbye message is received
    bool done = false;
    while(!done)
    {
        string recv = client.receiveMessage();

        if (recv.compare("") == 0)
        {
            done = true;
            continue;
        }

        //echo it back
        client.sendVariableData(recv);
    }

    //notify the parent process that this connection is finished
    write(sharedPipe[1], PROCESS_DONE_MSG.c_str(), PIPE_BUFFER_SIZE);

    client.closeSocket();
}



/*****************************************************************
** Function: controlHandler
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
    		sigaction(SIGINT | SIGCHLD, &old, NULL);

            for (int i = 0; i < children.size(); i++)
            {
                kill(children[i], SIGTERM);
            }

            //close up the pipe
            close(sharedPipe[0]);
            close(sharedPipe[1]);
            listeningSocket.closeSocket();
    	}
        exit(0);
    }

    if (signal == SIGCHLD)
    {
        waitpid(-1, 0, WNOHANG);
    }
}
