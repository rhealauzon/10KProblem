/**********************************************************************
**	SOURCE FILE:	client.cpp - Client to be used with all 3 servers
**
**	PROGRAM:	Scalable Server -- Client
**
**	FUNCTIONS:
**
**	DATE: 		February 5th, 2015
**
**
**	DESIGNER:	Rhea Lauzon A00881688
**
**
**	PROGRAMMER: Rhea Lauzon A00881688
**
**	NOTES:
** Creates many clients in order to load test with the various
** server types. This client is written with Epoll.
*************************************************************************/
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include "tcpsocket.h"
#include "client.h"

using namespace std;

/** Shared pipe for communication **/
int sharedPipe[2];

int numClients;

vector<int> childProcesses;

/* Signal handler structures */
struct sigaction SA;
struct sigaction old;
int pId;

int childNum;

const string PROCESS_DONE_MSG = "Process Done";

int main(int argc, char **argv)
{
    int port = 0;
    char *host;

    //get the command line arguments for port and hosts
    switch(argc)
	{
		case 3:
            // Host name
            host =	argv[1];
            numClients = atoi(argv[2]);
            port =	DEFAULT_PORT;
        break;

		case 4:
            host =	argv[1];
            numClients = atoi(argv[2]);
            // User specified port
            port =	atoi(argv[3]);
        break;

		default:
			fprintf(stderr, "Usage: %s host numClients [port]\n", argv[0]);
			exit(-1);
	}
    //make the pipe
    if (pipe(sharedPipe) < 0)
    {
        cerr << "Unable to create pipe." << endl;
        exit(-1);
    }

    //create all the child processes
    if (!createChildren(host, port))
    {
        cerr << "Error creating child processes" << endl;
        exit(-1);
    }

    //set up signal handler
	SA.sa_handler = controlHandler;
	sigemptyset( &SA.sa_mask );
	sigaction( SIGINT, &SA, &old );

    //receive messages until all clients finish
    receivePipeMessages();

    //parent returns here when all clients are done; tidy up the application
    tidyUp();
}

/*****************************************************************
** Function: createChildren
**
** Date: January 10th, 2015
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**		    bool createChildren()
**
** Returns:
**			bool -- true on successful return
**              -- false on a failure to create children
**
** Notes:
** Creates a number of child processes, each of which
** go forward to find prime numbers.
**********************************************************************/
bool createChildren(char *host, int port)
{
    pid_t processId = -2;
    childNum = 0;

    //create all the workers
    int workingTotal = numClients;

    while(workingTotal > 0)
    {
        //fork off a new child
        childNum++;
        processId = fork();

        if (processId == 0)
        {
            childInitialization(host, port);
            exit(0);
        }
        else
        {
            //decrement the counter and add the child so we can kill it later
            workingTotal --;
            pId = processId;
            childProcesses.push_back(processId);
        }

    }

    return true;
}

void receivePipeMessages()
{
    int clientsDone = 0;
    char in_buff[PIPE_BUFFER_SIZE];

    while (clientsDone < numClients)
	{
        //a new value has been added
        if (read(sharedPipe[0], in_buff, PIPE_BUFFER_SIZE) > 0)
        {
            if (strcmp(PROCESS_DONE_MSG.c_str(), in_buff) == 0)
            {
                //increment the done counter
                clientsDone++;

                if (clientsDone % 1 == 0)
                {
                    cout << "----------------------------------------" << endl;
                    printf("      Clients finished:       %d\n", clientsDone);
                    cout << "----------------------------------------" << endl;
                }
            }
        }
	}

    cout << "========================================" << endl;
    printf("All %d clients have finished.\n", numClients);
    cout << "========================================" << endl;

}

bool childInitialization(char *host, int port)
{
    //close the read descriptor of the pipe
    close(sharedPipe[0]);

    if (!socketTransfer(host, port))
    {
        cerr << "Unable to create the socket" << endl;
        return false;
    }

    return true;
}

bool socketTransfer(char *host, int port)
{
    //create the socket and connect to the server
    TCPSocket newClient;
    if (!newClient.connectClient(port, (string) host))
    {
        return false;
    }

    //generate the number of iterations each client will send and random size
    int randomIterations = 200;
    int randomSize = 1024;

    while(randomIterations > 0)
    {
        //send the first message to the Server
        newClient.sendMessage(generateString(randomSize, 1));

        newClient.receiveMessage();
        randomIterations--;
    }

    //let the parent proces know that this child is done
    write(sharedPipe[1], PROCESS_DONE_MSG.c_str(), PIPE_BUFFER_SIZE);

    //close the socket
    newClient.closeSocket();

    return true;
}

string generateString(int size, int clientNum)
{
    string newString = "";

    for (int i = 0; i < size; i++)
    {
        newString += 'A' + i % 24;
    }

    return newString;
}

void tidyUp()
{
    //kill all the child processes
    for (int i = 0; i < childProcesses.size(); i++)
    {
        kill(childProcesses[i], SIGTERM);
    }
}

void controlHandler(int signal)
{
    if ( pId != 0)
	{
		//restore default signal handler
		sigaction(SIGINT, &old, NULL);
        tidyUp();

        exit(0);
	}
}
