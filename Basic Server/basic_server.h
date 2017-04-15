#ifndef BASICSERVER_H
#define BASICSERVER_H

#define MIN_FREE_PROCESSES 30
#define NEW_ADDITION_INCREMENT 10
#define LISTENING_PORT 9000
#define MAX_QUEUED 1024
#define PIPE_BUFFER_SIZE 128

#define SOCKET_ERROR -1
#define RETURN_ERROR -1
#define CHILD_EXIT 0

/** Parent Process functions **/
int createChildren(int);
int waitForData();

/** Child process functions **/
void waitForClient();
void connectedState(TCPSocket);
void controlHandler(int);

#endif //BASICSERVER_H
