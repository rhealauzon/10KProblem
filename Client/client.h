#ifndef CLIENT_H
#define CLIENT_H

#define EPOLL_ERROR -1

#define DEFAULT_PORT 9000
#define PIPE_BUFFER_SIZE 128

#define SOCKETS_PER_CHILD 800
#define UPPER_BOUND_CLIENTS 100000


/** Client definitions **/
#define MAX_MESSAGE_SIZE 1024

/** Function prototypes **/
int waitForData(int);
int readData(int);
bool connectClients();
std::string generateString(int size);
void receivePipeMessages();
void tidyUp();
void controlHandler(int);
long getCurrentTime();


/** Child Process functions **/
bool createChildren(char *, int);
bool childInitialization(char *, int, int);
bool generateSockets(char *, int, int);

#endif //CLIENT_H
