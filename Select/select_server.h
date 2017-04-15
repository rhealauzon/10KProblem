#ifndef SELECTSERVER_H
#define SELECTSERVER_H

#define MIN_FREE_PROCESSES 30

#define LISTENING_PORT 9000
#define MAX_QUEUED 1024

#define PIPE_BUFFER_LENGTH 128

#define SOCKET_ERROR -1
#define RETURN_ERROR -1
#define CHILD_EXIT 0

/** Parent Process functions **/
int createChildren(int);
int waitForData();

/** Child process functions **/
void selectState();
void controlHandler(int);
int acceptConnection();
int readData(int);

#endif //SELECTSERVER_H
