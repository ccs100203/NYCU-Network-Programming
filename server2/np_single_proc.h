#ifndef NP_SINGLE_PROC_H
#define NP_SINGLE_PROC_H

#include <stdbool.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#define MAX_EVENTS 31   /* max epoll event */
#define MAX_NAME 25     /* max user name */
#define MAX_ENV  25     /* max environment number */
#define ENVSIZE 150     /* max env length */
#define MAX_CLIENT 30   /* max client number */

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

/* The following array use `uid` as index */
extern struct client_unit client_arr[MAX_CLIENT + 1];
extern int currfd; /* record current client fd */
extern int uid; /* record current user id */
extern int stdiofd[3]; /* record stdiofd */

#endif