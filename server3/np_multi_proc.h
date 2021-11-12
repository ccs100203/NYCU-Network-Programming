#ifndef NP_MULTI_PROC_H
#define NP_MULTI_PROC_H

#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>

#define MAX_EVENTS 10   /* max epoll simultaneous event */
#define MAX_NAME 25     /* max user name */
#define MAX_ENV  25     /* max environment number */
#define ENVSIZE 150     /* max env length */
#define MAX_CLIENT 30   /* max client number */
#define MAX_BROADCAST 4000  /* max broadcast buffer size */

struct env_unit {
    char key[ENVSIZE];
    char val[ENVSIZE];
    bool isValid;
};

struct client_unit {
    sem_t sem;
    bool isValid;
    char name[MAX_NAME];
    int sockfd; // TODO
    int port;
    pid_t pid;
    char ip[INET_ADDRSTRLEN + 5];
};

struct broadcast_unit {
    sem_t sem;
    char msg[MAX_BROADCAST];
};

void broadcast(char msg[]);
void unicast(char msg[], pid_t pid);

extern int currfd; /* record current client fd */
extern int uid; /* record current user id */
extern int stdiofd[3]; /* record stdiofd */ // TODO

/* The following array use `uid` as index */
extern struct client_unit *client_arr;
extern struct broadcast_unit *broadcast_region; /* broadcast share memory */

#endif