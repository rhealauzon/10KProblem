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
#include "messageQueue.h"

using namespace std;

TCPSocket listeningSocket;
int maxClients = 0;

/** Epoll variables **/
int epoll_fd;
struct epoll_event events[UPPER_BOUND_CLIENTS], event;

/** Message queue for IPC **/
int queueId;

vector<TCPSocket *> clientSockets;
vector<int> numIterations;
vector<int> stringSizes;
vector<int> childProcesses;

/* Signal handler structures */
struct sigaction SA;
struct sigaction old;
int pId;

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
            maxClients = atoi(argv[2]);
            port =	DEFAULT_PORT;
        break;

		case 4:
            host =	argv[1];
            maxClients = atoi(argv[2]);
            // User specified port
            port =	atoi(argv[3]);
        break;

		default:
			fprintf(stderr, "Usage: %s host numClients [port]\n", argv[0]);
			exit(-1);
	}

    //create the message queue
    queueId = setupQueue();

    //create the epoll descriptor
    epoll_fd = epoll_create(maxClients);
    if (epoll_fd == -1)
    {
        cerr << "Unable to create Epoll file descriptor" << endl;
        exit(EPOLL_ERROR);
    }

    //generate all the client sockets
    if (!generateSockets(host, port))
    {
        cerr << "Error creating client sockets" << endl;
        exit(EPOLL_ERROR);
    }

    //start connecting clients
    if (!connectClients())
    {
        exit(-1);
    }

    if (!createChildren())
    {
        cerr << "Error creating child processes" << endl;
        exit(-1);
    }

    //set up signal handler
	SA.sa_handler = controlHandler;
	sigemptyset( &SA.sa_mask );
	sigaction( SIGINT, &SA, &old );

    //receive messages until all clients finish
    receiveQueueMessages();

    //parent returns here when all clients are done; tidy up the application
    tidyUp();
}

bool generateSockets(char *host, int port)
{
    //create all the new sockets
    for (int i = 1; i <= maxClients; i++)
    {
        //create the socket and connect to the server
        TCPSocket *newClient = new TCPSocket();
        if (!newClient->basicInitialize(port, (string) host))
        {
            return false;
        }

        //add it to the list of sockets
        clientSockets.push_back(newClient);

        //generate the number of iterations each client will send and random size
        int randomIterations = rand() % (MAX_ITERATIONS - MIN_ITERATIONS + 1) + MIN_ITERATIONS;
        int randomSize  = rand() % (MAX_MESSAGE_SIZE - MIN_MESSAGE_SIZE + 1) + MIN_MESSAGE_SIZE;


        //add it to the lists for future references
        numIterations.emplace_back(randomIterations);
        stringSizes.emplace_back(randomSize);


        // Add the server socket to the epoll event loop
    	event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
    	event.data.fd = newClient->getSocketValue();

    	if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, newClient->getSocketValue(), &event) == -1)
        {
            return false;
        }
    }
    cout << "-----------------------------------" << endl;
    return true;
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
bool createChildren()
{
    pid_t processId = -2;

    //create all the workers
    for (int i = 0; i <= MAX_CHILDREN + 1; i++)
    {
        switch (processId)
        {
            //fork error
            case -1:
                cerr << "Error creating a child process." << endl;
                return false;
            break;

            //child process
            case 0:
                waitForData();
                exit(0);
            break;

            //parent process
            default:
                if (i < MAX_CHILDREN)
                {
                    //fork off a new child
                    processId = fork();
                    pId = processId;
                    childProcesses.push_back(processId);
                }
            break;
        }
    }
    return true;
}

void receiveQueueMessages()
{
    bool socketsDone[maxClients] = {false};
    bool done = false;

    while (!done)
	{
        //wait for a message on the queue
		Message *received = readMessage(queueId, -(PARENT_MESSAGE));

        //assuming we got data, convert it to the requested index and respond
        if (received->msg_data != "")
		{
            int requestedValue = atoi(received->msg_data);


            if (socketsDone[requestedValue] == true)
            {
                stringstream ss;
                ss << -99;

                //send the response
                while (sendMessage(queueId, ss.str(), PARENT_MESSAGE + (received->msg_type - WORKER_MESSAGE)) < 0)
                {
                    cerr << "Message queue send error. Re-trying the send." << endl;
                }
                continue;
            }

            else if ((numIterations[requestedValue] == 0) && socketsDone[requestedValue] == false)
            {
                printf("Client Finished:    %d\n", requestedValue);
                socketsDone[requestedValue] = true;
            }

            stringstream ss;
            ss << numIterations[requestedValue];
            numIterations[requestedValue]--;

            //send the response
            while (sendMessage(queueId, ss.str(), PARENT_MESSAGE + (received->msg_type - WORKER_MESSAGE)) < 0)
            {
                cerr << "Message queue send error. Re-trying the send." << endl;
            }

            bool socketsChecker = true;
            for (int i = 0; i < maxClients; i++)
            {
                if (socketsDone[i] == false)
                {
                    socketsChecker = false;
                    break;
                }
            }

            if (socketsChecker == true)
            {
                done = true;
            }
		}
        else
        {
            cerr << "Too small of a message" << endl;
        }
	}

    cout << "========================================" << endl;
    printf("All %d clients have finished.\n", maxClients);
    cout << "========================================" << endl;

}

int waitForData()
{
    int num_ready = -1;
    struct epoll_event current_event;

    while (true)
    {
    //    printf("Process %d       waiting for epoll\n", getpid());
        num_ready = epoll_wait(epoll_fd, events, maxClients, -1);
    //    printf("Process %d      got epoll data\n", getpid());
        cout.flush();

        if (num_ready < 0)
        {
            cerr << "Error on epoll waiting" << endl;
            return -1;
        }
        else
        {
            //data has been received
            for (int i = 0; i < num_ready; i++)
            {
                current_event = events[i];

                if (!(current_event.events & EPOLLIN))
                {
                    continue;
                }

                //check for errors first (EX: server has ended the connection)
                else if (current_event.events & (EPOLLHUP))
                {
                    //cout << "hangup" << endl;
                    //cout.flush();
                    close(current_event.data.fd);
                    continue;
                }

                else if (current_event.events & (EPOLLERR))
                {
                    //perror("Epoll Error");
                    continue;
                }

                //data is to be read
                else if (current_event.events & (EPOLLIN))
                {
                    if(readData(current_event.data.fd) < 0)
                    {
                        close(current_event.data.fd);
                    }
                }
                else
                {
                    cerr << "Unknown why Epoll got this" << endl;
                }
            }
        }
    }

    return 0;
}

int readData(int socket)
{
    int location = -1;

    //determine which socket received data
    for (int i = 0; i < clientSockets.size(); i++)
    {
        if (clientSockets[i]->getSocketValue() == socket)
        {
            location = i;
            break;
        }
    }

    if (location != -1)
    {
        //get the iteration value from the parent process
        stringstream ss;
        ss << location;

        while (sendMessage(queueId, ss.str(), WORKER_MESSAGE + getpid()) < 0)
        {
            cerr << "Message queue error. Sending again." << endl;
        }

        //wait for a response
        int counter = atoi(readMessage(queueId, PARENT_MESSAGE + getpid())->msg_data);

        //read the echo from the server
        string recv = clientSockets[location]->receiveMessage();

        //if there is still an iteration, reply to the server
        if (counter > 0)
        {
            //reply to the server
            clientSockets[location]->sendMessage(generateString(stringSizes[location], location));
        }

        else if (counter == -99)
        {
            //close the socket
            clientSockets[location]->closeSocket();
            close(socket);

        }

        //now that the number of iterations are done, send a goodbye message to the server
        else
        {
            //end the socket connection
            clientSockets[location]->sendMessage("Goodbye!");

            printf("CLIENT #%d      | DONE\n", location + 1);
            //close the socket
            clientSockets[location]->closeSocket();
            close(socket);

            return -1;
        }
    }
    else
    {
        cout << "No socket found.." << endl;
        cout.flush();
    }

    return 0;
}

bool connectClients()
{
    for (int i = 0; i < maxClients; i++)
    {
        //connect the client
        if (!clientSockets[i]->basicConnect())
        {
            printf("Client #%d failed to connect\n", i);
            return false;
        }

        //make the socket non-blocking
        if (fcntl (clientSockets[i]->getSocketValue(), F_SETFL, O_NONBLOCK | fcntl(clientSockets[i]->getSocketValue(), F_GETFL, 0)) == -1)
        {
            printf("Client #%d failed to become non-blocking\n", i);
            return false;
        }

        //send the first message to the Server
        clientSockets[i]->sendMessage(generateString(stringSizes[i], i));
        numIterations[i]--;
    }

    cout << "All clients connnected" << endl;

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

    //close all the sockets
    for (int i = 0; i < clientSockets.size(); i++)
    {
        clientSockets[i]->closeSocket();
    }

    //remove the message queue
    if (msgctl(queueId, IPC_RMID, 0) < 0)
    {
        cerr << "Failed to remove message queue." << endl;
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
