#ifndef NP_SINGLE_PROC_H
#define NP_SINGLE_PROC_H

#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>


#define PATHSIZE 200
#define ARGSLIMIT 30
#define CMDSIZE 300
#define MAX_EVENTS 31   /* max epoll event */
#define MAX_NAME 25     /* max user name */
#define MAX_CLIENT 30   /* max client number */
#define MAX_ENV  25     /* max environment number */
#define ENVSIZE 150     /* max env length */

struct built_in_arg {
    char name[PATHSIZE];
    char value[PATHSIZE];
};

struct cmd_arg {
    bool isPipe;
    bool isFileRedirect;
    bool isNumPipe;
    bool isErrPipe;
    size_t numPipeLen;
    char filename[PATHSIZE];
};

struct pipe_unit {
    int pipefd[2];
    bool isValid;
};

struct env_unit {
    char key[ENVSIZE];
    char val[ENVSIZE];
    bool isValid;
};

struct client_unit {
    bool isValid;
    char name[MAX_NAME];
    int sockfd;
    int port;
    char ip[INET_ADDRSTRLEN + 5];
    struct env_unit env[MAX_ENV];
};

void built_in(char *cmd_token, char *cmd_rest, char flag);
void child_handler(int signum);
int npshell();

/* The following array use `uid` as index */
struct pipe_unit pipe_arr[MAX_CLIENT][1010] = {0}; /* record number pipe */
struct client_unit client_arr[MAX_CLIENT + 1] = {0};
int currfd = 0; /* record current client fd */
int uid = 0; /* record current user id */
int stdiofd[3] = {0}; /* record stdiofd */

#endif