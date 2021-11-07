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


#define PATHSIZE 200
#define ARGSLIMIT 30
#define CMDSIZE 300

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

void built_in(char *cmd_token, char *cmd_rest, char flag);
void child_handler(int signum);
int npshell();

struct pipe_unit pipe_arr[30][1010] = {0};
int currSock = 0;

#endif