#ifndef NPSHELL_H
#define NPSHELL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATHSIZE 100

struct built_in_arg {
    char name[PATHSIZE];
    char value[PATHSIZE];
};

#endif