#ifndef CLIENT_H
#define CLIENT_H

#define EPOLL_ERROR -1
#define DEFAULT_PORT 9000
#define PIPE_BUFFER_SIZE 128

#define NUM_CHILDREN_PER_PROCESS 10

#define UPPER_BOUND_CLIENTS 100000


/** Client definitions **/
#define MIN_ITERATIONS 5
#define MAX_ITERATIONS 5
#define MAX_MESSAGE_SIZE 1024
#define MIN_MESSAGE_SIZE 32

/** Function prototypes **/
bool connectClients();
std::string generateString(int, int);
void receivePipeMessages();
void tidyUp();
void controlHandler(int);


/** Child Process functions **/
bool createChildren(char *, int);
bool childInitialization(char *, int);
bool socketTransfer(char *, int);

#endif //CLIENT_H
