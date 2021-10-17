#ifndef NPSHELL_H
#define NPSHELL_H

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PATHSIZE 100
#define ARGSLIMIT 30

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

void built_in(char flag, struct built_in_arg args);
static void child_handler(int signum);

struct pipe_unit pipe_arr[1010] = {0};
size_t pipeLen = 0;

#endif