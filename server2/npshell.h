#ifndef NPSHELL_H
#define NPSHELL_H

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
#define MAX_CLIENT 30   /* max client number */

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

/* The following array use `uid` as index */
extern struct pipe_unit pipe_arr[MAX_CLIENT][1010]; /* record number pipe */


#ifdef DEBUG
#define debug(...)           \
    do {                     \
        printf(__VA_ARGS__); \
    } while (0)
#else
#define debug(...) \
    do {           \
        (void) 0;  \
    } while (0)
#endif


#endif