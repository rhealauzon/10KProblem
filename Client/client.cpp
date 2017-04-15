/**********************************************************************
**	SOURCE FILE:	client.cpp - Client to be used with all 3 servers
**
**	PROGRAM:	Scalable Server -- Client
**
**	FUNCTIONS:
** int waitForData(int)
** int readData(int)
** bool connectClients()
** string generateString(int size)
** void receivePipeMessages()
** void tidyUp()
** void controlHandler(int)
** long getCurrentTime()
** bool createChildren(char *, int)
** bool childInitialization(char *, int, int)
b** ool generateSockets(char *, int, int)
**
**	DATE: 		February 5th, 2016
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
#include <sys/time.h>
#include <map>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include "tcpsocket.h"
#include "client.h"

using namespace std;

/** Epoll variables **/
int epoll_fd;
struct epoll_event events[UPPER_BOUND_CLIENTS], event;

/** Shared pipe for communication **/
int sharedPipe[2];

/** Command line determined variables **/
int numClients;
int numMessages;
int messageSize;
string messageToSend;

vector<TCPSocket *> clientSockets;
vector<int> childProcesses;
vector<int> numIterations;

/* Signal handler structures */
struct sigaction SA;
struct sigaction old;
int pId;

const string CLIENT_CONNECTED_MSG = "Client Connected";
const string CLIENT_DONE_MSG = "Client Done";


/*****************************************************************
** Function: main
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
**			int main(int argc, char **argv)
**          int argc -- Number of command line arguments
**          char **argv -- Array of commmand line arguments
**
** Returns:
**			N/A
**
** Notes:
** Main fucntion of the client. Parses command line arguments
** then continues into initializing child processes.
*********************************************************************/
int main(int argc, char **argv)
{
    int port = 0;
    char *host;

    //get command line arguments
    char option;
    while ((option = getopt(argc, argv, "h:p:c:s:m:")) != -1)
    {
        port =	DEFAULT_PORT;
        switch(option)
    	{
            //get the host name
            case 'h':
            {
                host = optarg;
                break;
            }

            //get the port number if it exists
            case 'p':
            {
                port = atoi(optarg);
                break;
            }

            case 'c':
            {
                numClients = atoi(optarg);
                break;
            }

            //get the size of the message
            case 's':
            {
                messageSize = atoi(optarg);

                if (messageSize > MAX_MESSAGE_SIZE)
                {
                    cerr << "Message size cannot be bigger than 1024" << endl;
                    return -1;
                }
                break;
            }

            //get the number of messages
            case 'm':
            {
                numMessages = atoi(optarg);
                break;
            }

            case '?':
            {
                if (isprint (optopt))
                {
                   fprintf(stderr,"unknown option \"-%c\".\n", optopt);
                }
                else
                {
                   fprintf(stderr,"unknown option character \"%x\".\n",optopt);
                }

                return -1;
                break;
            }

            default:
            {
                cerr << "Unknown command line argument" << endl;
                cerr << "./client -h address -c numClients -s dataSize -m numMessages [-p port]" << endl;

                return -1;
                break;
            }
    	}
    }

    if (port <= 0 || messageSize <= 0 || numMessages <= 0 || numClients <= 0)
    {
        cerr << "Not all mandatory switches set." << endl;
        cerr << "./client -h address -c numClients -s dataSize -m numMessages [-p port]" << endl;
        return -1;
    }

    //make the pipe
    if (pipe(sharedPipe) < 0)
    {
        cerr << "Unable to create pipe." << endl;
        exit(-1);
    }

    //generate the message to be sent
    messageToSend = generateString(messageSize - 1);

    //create all the child processes
    if (!createChildren(host, port))
    {
        cerr << "Error creating child processes" << endl;
        exit(-1);
    }

    if (pId != 0)
    {
        //set up signal handler
    	SA.sa_handler = controlHandler;
    	sigemptyset( &SA.sa_mask );
    	sigaction( SIGINT, &SA, &old );

        //receive messages until all clients finish
        receivePipeMessages();

        //parent returns here when all clients are done; tidy up the application
        tidyUp();
    }

    return 0;
}

/*****************************************************************
** Function: createChildren
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
**		    bool createChildren(char *host, int port)
**          char *host -- Host to connect to
**          int port -- port to connect to
**
** Returns:
**			bool -- true on success
**              -- false on a failure to create children
**
** Notes:
** Creates a number of child processes, each of which
** will generate their own set of sockets and connect to the server.
**********************************************************************/
bool createChildren(char *host, int port)
{
    pid_t processId = -2;

    //create all the workers
    int workingTotal = numClients;

    while(workingTotal > 0)
    {
        int numToAdd = (workingTotal > SOCKETS_PER_CHILD ? SOCKETS_PER_CHILD : workingTotal);
        //fork off a new child
        processId = fork();

        if (processId == 0)
        {
            return childInitialization(host, port, numToAdd);
        }
        else
        {
            //decrement the counter and add the child so we can kill it later
            workingTotal -= numToAdd;
            pId = processId;
            childProcesses.push_back(processId);
        }
    }

    return true;
}


/*****************************************************************
** Function: receivePipeMessages
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
**			void receivePipeMessages()
**
** Returns:
**			N/A
**
** Notes:
** The main process is the only process that gets here.
** It waits for messages on the pipe in order to create statistics
** and keep track of which processes are active.
*********************************************************************/
void receivePipeMessages()
{
    int clientsDone = 0;
    char in_buff[PIPE_BUFFER_SIZE];

    //map for keeping track of start and end times of clients
    map<string, long> timeTaken;
    long totalServiceTime;

    totalServiceTime = getCurrentTime();

    while (clientsDone < numClients)
	{
        //a new value has been added
        if (read(sharedPipe[0], in_buff, PIPE_BUFFER_SIZE) > 0)
        {
            //Check if the client has finished its transmission
            if (strncmp(CLIENT_DONE_MSG.c_str(), in_buff, CLIENT_DONE_MSG.size()) == 0)
            {
                //increment the done counter
                clientsDone++;

                //get the end time and place it in the map
                string client = in_buff;
                client = client.substr(CLIENT_DONE_MSG.size(), PIPE_BUFFER_SIZE);
                timeTaken[client] = getCurrentTime() - timeTaken[client];

                cout << "----------------------------------------" << endl;
                printf("      Clients finished:       %d\n", clientsDone);
                cout << "----------------------------------------" << endl;

            }

            //Check if this client is just starting its transmission
            else if (strncmp(CLIENT_CONNECTED_MSG.c_str(), in_buff, CLIENT_CONNECTED_MSG.size()) == 0)
            {
                string client = in_buff;
                client = client.substr(CLIENT_CONNECTED_MSG.size(), PIPE_BUFFER_SIZE);
                timeTaken[client] = getCurrentTime();
            }
        }
	}

    totalServiceTime = getCurrentTime() - totalServiceTime;

    //calculate the average time taken
    long averageTime;
    long totalTime;
    for(auto const &iterator : timeTaken)
    {
        totalTime += iterator.second;
    }
    averageTime = totalTime / numClients;


    //print out all statistics
    cout << "========================================" << endl;
    printf("All %d clients have finished.\n", numClients);
    cout << "========================================" << endl;
    printf("Total time taken:                  %lld ms\n", totalServiceTime);
    printf("Average time taken per client:     %lld ms\n", averageTime);
    printf("Data sent per client:              %d Bytes\n", messageSize * numMessages);

}


/*****************************************************************
** Function: childInitialization
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
**			bool childInitialization(char *host, int port, int numSockets)
**          char *host -- server address to connect to
**          int port -- port to connect to on the server
**          int numSockets -- Number of client sockets to create
**
** Returns:
**			bool -- True if all clients are able to fully initialize
**                  False if any clients were unable initialize
**
** Notes:
** Initializes each child process with their designated number
** of clients to keep track of, then enters a state of epoll
*********************************************************************/
bool childInitialization(char *host, int port, int numSockets)
{
    //create the epoll descriptor
    epoll_fd = epoll_create(numClients);

    //close the read descriptor of the pipe
    close(sharedPipe[0]);

    if (epoll_fd == -1)
    {
        cerr << "Unable to create Epoll file descriptor" << endl;
        return false;
    }

    //generate all the client sockets this process will be responsible for
    if (!generateSockets(host, port, numSockets))
    {
        cerr << "Unable to create all my sockets" << endl;
        return false;
    }

    //wait for the data on all the clients
    waitForData(numClients);

    return true;
}


/*****************************************************************
** Function: generateSockets
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
**			bool generateSockets(char *host, int port, int numSockets)
**          char *host -- server address to connect to
**          int port -- port to connect to on the server
**          int numSockets -- Number of client sockets to create
**
** Returns:
**			bool -- True if all clients are able to fully initialize
**                  False if any clients were unable initialize
**
** Notes:
** Generates a number of client sockets that connect to the
** server and sends the first message.
*********************************************************************/
bool generateSockets(char *host, int port, int numSockets)
{
    //create all the new sockets
    for (int i = 0; i < numSockets; i++)
    {
        //create the socket and connect to the server
        TCPSocket *newClient = new TCPSocket();
        if (!newClient->connectClient(port, (string) host))
        {
            return false;
        }

        //add it to the list of sockets
        clientSockets.push_back(newClient);

        //make the socket non-blocking
        if (fcntl (clientSockets[i]->getSocketValue(), F_SETFL, O_NONBLOCK | fcntl(clientSockets[i]->getSocketValue(), F_GETFL, 0)) == -1)
        {
            printf("Client #%d failed to become non-blocking\n", i);
            return false;
        }

        //add the specified number of iterations to the list
        numIterations.emplace_back(numMessages);

        // Add the socket to the epoll event loop
    	event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
    	event.data.fd = newClient->getSocketValue();

    	if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, newClient->getSocketValue(), &event) == -1)
        {
            return false;
        }

        //notify the parent process that this connection is finished
        stringstream message;
        message << CLIENT_CONNECTED_MSG << (int) getpid() << i;
        write(sharedPipe[1], message.str().c_str(), PIPE_BUFFER_SIZE);

        //send the first message to the Server
        clientSockets[i]->sendMessage(messageToSend);
        numIterations[i]--;

    }
    return true;
}


/*****************************************************************
** Function: waitForData
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
**			int waitForData(int numClients)
**          int numClients -- Number of clients that each process
**                             should keep track of.
**
** Returns:
**			int -- 0 if all clients finish sucessfully
**                 -1 if any errors occur during this state
**
** Notes:
** Uses Epoll to wait for data on each socket until all clients
** have sucessfully finished their transmissions.
*********************************************************************/
int waitForData(int numClients)
{
    int numDone = 0;
    int num_ready = -1;
    struct epoll_event current_event;

    while (numDone < numClients)
    {
        num_ready = epoll_wait(epoll_fd, events, numClients, -1);

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
                if (current_event.events & (EPOLLHUP | EPOLLERR))
                {
                    if (errno != 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        perror("EPOLL ERROR");
                        cerr << "EPOLL ERROR" << endl;
                        close(current_event.data.fd);
                    }
                    //someone else has handled this connection
                    else
                    {
                        errno = 0;
                    }
                    continue;
                }

                //data is to be read
                else if (current_event.events & (EPOLLIN))
                {
                    if(readData(current_event.data.fd) > 0)
                    {
                        close(current_event.data.fd);
                        //increment the number of done clients by this child
                        numDone++;
                    }
                }
                else
                {
                    cerr << "Unknown why Epoll got to this state." << endl;
                }
            }
        }
    }

    return 0;
}


/*****************************************************************
** Function: readData
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
**			int readData(int socket)
**          int socket -- Socket that will be read from
**
** Returns:
**			int -- 0 if data was read successfully
**                -1 if any errors occur during this state
**                 1 if this particular client is done sending
**
** Notes:
** Reads data from a particular socket when it is available.
*********************************************************************/
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
        string received = clientSockets[location]->receiveMessage();

        //if there is still an iteration, reply to the server
        if (numIterations[location] > 0)
        {
            //reply to the server
            clientSockets[location]->sendMessage(messageToSend);
            numIterations[location]--;
        }

        //now that the number of iterations are done, close the socket
        else
        {
            //notify the parent process that this connection is finished
            stringstream message;
            message << CLIENT_DONE_MSG << (int) getpid() << location;
            write(sharedPipe[1], message.str().c_str(), PIPE_BUFFER_SIZE);

            //close the socket
            clientSockets[location]->closeSocket();
            close(socket);

            return 1;
        }
    }
    else
    {
        cerr << "No socket found.." << endl;
        return -1;
    }

    return 0;
}


/*****************************************************************
** Function: generateString
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
**			string generateString(int size)
**          int size -- Size of string to generate
**
** Returns:
**			string -- Generated string
**
** Notes:
** Generates a random text string based off the size given.
*********************************************************************/
string generateString(int size)
{
    string newString = "";

    for (int i = 0; i < size; i++)
    {
        newString += 'A' + i % 24;
    }

    return newString;
}

/*****************************************************************
** Function: getCurrentTime
**
** Date: February 11th, 2016
**
** Revisions:
**
**
** Designer: Rhea Lauzon
**
** Programmer: Rhea Lauzon
**
** Interface:
**			long getCurrentTime()
**
** Returns:
**			long -- Time in milliseconds
**
** Notes:
** Gets the current time in milliseconds for timing purposes
*********************************************************************/
long getCurrentTime()
{
    //get current time
    struct timeval timeValue;
    gettimeofday(&timeValue, 0);

    //convert the time to milliseconds
    return (timeValue.tv_sec * 1000LL + timeValue.tv_usec / 1000);
}



/*****************************************************************
** Function: getCurrentTime
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
**			void tidyUp()
**
** Returns:
**			N/A
**
** Notes:
** Kills all child processes.
*********************************************************************/
void tidyUp()
{
    //kill all the child processes
    for (int i = 0; i < childProcesses.size(); i++)
    {
        kill(childProcesses[i], SIGTERM);
    }
}

/*****************************************************************
** Function: controlhandler
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
**			void controlHandler(int signal)
**
** Returns:
**			N/A
**
** Notes:
** Signal handler for ctrl + z. Causes all processes to be cleaned up.
*********************************************************************/
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
