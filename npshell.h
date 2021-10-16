#ifndef NPSHELL_H
#define NPSHELL_H

#include <errno.h>
// #include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define PATHSIZE 100

struct built_in_arg {
    char name[PATHSIZE];
    char value[PATHSIZE];
};

void built_in(char flag, struct built_in_arg args);


#endif