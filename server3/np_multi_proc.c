#include "np_multi_proc.h"
#include "npshell.h"

/* The following array use `uid` as index */
struct client_unit *client_arr = 0;
int currfd = 0;             /* record current client fd */
int uid = 0;                /* record current user id */
int stdiofd[3] = {0};       /* record stdiofd */
struct broadcast_unit *broadcast_region = 0; /* broadcast share memory */


void welcome(int fd)
{
    send(fd, "****************************************\n", strlen("****************************************\n"), 0);
    send(fd, "** Welcome to the information server. **\n", strlen("** Welcome to the information server. **\n"), 0);
    send(fd, "****************************************\n", strlen("****************************************\n"), 0);
}

void broadcast(char msg[])
{
    memset(broadcast_region->msg, 0, MAX_BROADCAST);
    strncpy(broadcast_region->msg, msg, strlen(msg));
    /* send broadcast signal to active client*/
    for (int i = 0; i < MAX_CLIENT; ++i) {
        if (client_arr[i].isValid){
            kill(client_arr[i].pid, SIGUSR1);
        }
    }
}

void unicast(char msg[], pid_t pid)
{
    memset(broadcast_region->msg, 0, MAX_BROADCAST);
    strncpy(broadcast_region->msg, msg, strlen(msg));
    /* send unicast signal to active client*/
    kill(pid, SIGUSR1);
}

void server_handler(int signum)
{
    /* interrupt, server terminated */
    if (signum == SIGINT) {
        while (waitpid(-1, NULL, WNOHANG) > 0) // TODO
        ;
        for (int i = 0; i < MAX_CLIENT; ++i) {
            if (client_arr[i].isValid)
                sem_destroy(&client_arr[i].sem);
        }
        sem_destroy(&broadcast_region->sem);
        shm_unlink("user_info");
        shm_unlink("broadcast");
        for (int i = 0; i < MAX_CLIENT; ++i) {
            char path_user_pipe[20] = "";
            sprintf(path_user_pipe, "user_pipe_%d", i);
            shm_unlink(path_user_pipe);
        }
        exit(EXIT_SUCCESS);
    } /* child terminated */
    else if (signum == SIGCHLD) {
        while (waitpid(-1, NULL, WNOHANG) > 0)
            ;
    }
}

void client_handler(int signum)
{
    /* print broadcast message */
    if (signum == SIGUSR1) {
        printf("%s", broadcast_region->msg);
        fflush(stdout);
    } /* open readside fd */
    else if (signum == SIGUSR2) {
        unsigned int i = __builtin_ffs(client_arr[uid].who_send_mask) - 1;
        client_arr[uid].who_send_mask = 0;
        if (readfd_arr[i] != -1) {
            close(readfd_arr[i]);
        }
        char path_fifo[20] = "";
        sprintf(path_fifo, "user_pipe/%u__%d", i, uid);
        readfd_arr[i] = open(path_fifo, O_RDONLY, 0);
    }
}

/* create user info share memory */
void create_shm_user_info()
{
    char path_info[20] = "user_info";
    int infofd = shm_open(path_info, O_CREAT | O_RDWR, 0666);
    if (infofd == -1)
        perror("shm_open infofd");

    if (ftruncate(infofd, sizeof(struct client_unit) * MAX_CLIENT) == -1)
        perror("ftruncate");

    client_arr = mmap(NULL, sizeof(struct client_unit) * MAX_CLIENT,
                   PROT_READ | PROT_WRITE, MAP_SHARED, infofd, 0);

    if (client_arr == MAP_FAILED)
        perror("mmap");

    /* initialize all clients */
    for (int i = 0; i < MAX_CLIENT; ++i) {
        client_arr[i].isValid = false;
        strcpy(client_arr[i].name, "(no name)");
        client_arr[i].sockfd = -1;
        client_arr[i].port = -1;
        client_arr[i].pid = -1;
        strcpy(client_arr[i].ip, "");
        client_arr[i].who_send_mask = 0;
    }
}

/* create broadcast share memory */
void create_shm_broadcast()
{
    char path_broadcast[20] = "broadcast";
    int broadcastfd = shm_open(path_broadcast, O_CREAT | O_RDWR, 0666);
    if (broadcastfd == -1)
        perror("shm_open broadcast");

    if (ftruncate(broadcastfd, sizeof(struct broadcast_unit)) == -1)
        perror("ftruncate");

    broadcast_region = mmap(NULL, sizeof(struct broadcast_unit),
                   PROT_READ | PROT_WRITE, MAP_SHARED, broadcastfd, 0);

    if (broadcast_region == MAP_FAILED)
        perror("mmap");

    /* initialization */
    strcpy(broadcast_region->msg, "");
    if (sem_init(&broadcast_region->sem, 1, 1) == -1)
        perror("sem_init");
}

/* create user pipe share memory */
void create_shm_user_pipe()
{
    for (int i = 0; i < MAX_CLIENT; ++i) {
        char path_user_pipe[20] = "";
        sprintf(path_user_pipe, "user_pipe_%d", i);
        int user_pipefd = shm_open(path_user_pipe, O_CREAT | O_RDWR, 0666);
        if (user_pipefd == -1)
            perror("shm_open user_pipe");

        if (ftruncate(user_pipefd, sizeof(struct usrpipe_unit) * MAX_CLIENT) == -1)
            perror("ftruncate");

        usr_pipe_arr[i] = mmap(NULL, sizeof(struct usrpipe_unit) * MAX_CLIENT,
                    PROT_READ | PROT_WRITE, MAP_SHARED, user_pipefd, 0);

        if (usr_pipe_arr[i] == MAP_FAILED)
            perror("mmap");

        /* initialization */
        for (int j = 0; j < MAX_CLIENT; ++j) { 
            /* i: send  --->  i: recv */
            usr_pipe_arr[i][j].isValid = false;
            strcpy(usr_pipe_arr[i][j].cmd, "");
        }
    }
}

int main(int argc, char **argv)
{
    /* set server sigaction */
    struct sigaction sa;
    sa.sa_handler = server_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    /* deal with port */
    uint16_t port = 7001;
    if(argv[1])
        port = strtol(argv[1], NULL, 10);
    debug("port: %u\n", port);

    /* set socket */
    int servfd = 0;
    if ((servfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Fail to create a socket.");
    }

    /* set SO_REUSEADDR */
    if (setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    /* initialize server args */
    struct sockaddr_in servAddr;
    bzero(&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(port);
    bind(servfd, (struct sockaddr *) &servAddr, sizeof(servAddr));
    listen(servfd, 10);

    /* save STDIN/OUT/ERR */
    for(int i = 0; i < 3; ++i) {
        stdiofd[i] = dup(i);
    }

    /* create user info share memory */
    create_shm_user_info();

    /* create broadcast share memory */
    create_shm_broadcast();

    /* create user pipe share memory */
    create_shm_user_pipe();
    
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t addrlen = sizeof(clientAddr);
        int clientfd = accept(servfd, (struct sockaddr *) &clientAddr, &addrlen);
        debug("accept fd: %d\n", clientfd);
        if (clientfd == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        /* find the smallest uid */
        for (uid = 0; uid < MAX_CLIENT; ++uid) {
            if(!client_arr[uid].isValid)
                break;
        }

        /* initialize a new client */
        client_arr[uid].isValid = true;
        strcpy(client_arr[uid].name, "(no name)");
        client_arr[uid].sockfd = clientfd;
        client_arr[uid].port = ntohs(clientAddr.sin_port);
        client_arr[uid].pid = -1;
        char ip_tmp[INET_ADDRSTRLEN + 2] = {'\0'};
        inet_ntop(AF_INET, &clientAddr.sin_addr, ip_tmp, INET_ADDRSTRLEN);
        strncpy(client_arr[uid].ip, ip_tmp, strlen(ip_tmp));
        /* set semaphore to 0 */
        if (sem_init(&client_arr[uid].sem, 1, 0) == -1)
            perror("sem_init-sem_write");

        debug("clientfd %d, %d\n", clientfd, client_arr[uid].sockfd);
        /* send welcome message */
        welcome(clientfd);

        pid_t cpid = fork();
        switch (cpid) {
        case -1:
        {
            perror("fork error");
            break;
        }
        case 0: /* child */
        {
            /* set client sigaction */
            struct sigaction sa2;
            sa2.sa_handler = client_handler;
            sigemptyset(&sa.sa_mask);
            sa2.sa_flags = SA_RESTART;
            sigaction(SIGUSR1, &sa2, NULL);
            sigaction(SIGUSR2, &sa2, NULL);

            debug("clientfd %d\n", clientfd);
            /* redirect stdio */
            for (int i = 0; i < 3; ++i) {
                dup2(clientfd, i);
            }
            close(servfd);
            currfd = clientfd;
            
            /* wait until pid set */
            sem_wait(&client_arr[uid].sem);

            /* broadcast login message */
            char buf[100] = "";
            sprintf(buf, "*** User '%s' entered from %s:%d. ***\n", 
                client_arr[uid].name, client_arr[uid].ip, client_arr[uid].port);
            broadcast(buf);

            if(npshell() == 1) {
                /* exit */
                /* close client's client_arr */
                client_arr[uid].isValid = false;
                
                /* broadcast logout message */
                char buf[100] = "";
                sprintf(buf, "*** User '%s' left. ***\n", client_arr[uid].name);
                broadcast(buf);

                /* clear name & ip & env */
                memset(client_arr[uid].name, 0, MAX_NAME);
                memset(client_arr[uid].ip, 0, INET_ADDRSTRLEN+1);

                /* destroy semaphore */
                sem_destroy(&client_arr[uid].sem);

                /* close client's pipe_arr */
                for (int i = 0; i < 1003; ++i) {
                    if (pipe_arr[i].isValid) {
                        pipe_arr[i].isValid = false;
                        close(pipe_arr[i].pipefd[0]);
                        close(pipe_arr[i].pipefd[1]);
                    }
                }
                
                /* close client's user pipe arr */
                for (int i = 0; i < MAX_CLIENT; ++i) {
                    if (usr_pipe_arr[i][uid].isValid) {
                        usr_pipe_arr[i][uid].isValid = false;
                        memset(usr_pipe_arr[i][uid].cmd, 0, MAX_LINE);
                    }
                    char path_fifo[20] = "";
                    sprintf(path_fifo, "user_pipe/%d__%d", i, uid);
                    unlink(path_fifo);
                }
                for (int i = 0; i < MAX_CLIENT; ++i) {
                    if (usr_pipe_arr[uid][i].isValid) {
                        usr_pipe_arr[uid][i].isValid = false;
                        memset(usr_pipe_arr[uid][i].cmd, 0, MAX_LINE);
                    }
                    char path_fifo[20] = "";
                    sprintf(path_fifo, "user_pipe/%d__%d", uid, i);
                    unlink(path_fifo);
                }

                /* close readfd_arr */
                for (int i = 0; i < MAX_CLIENT; ++i) {
                    if (readfd_arr[i] != -1)
                        close(readfd_arr[i]);
                }

                exit(EXIT_SUCCESS);
            } else {
                perror("WTF");
            }
            perror("WTTFF\n");
            exit(EXIT_SUCCESS);
            break;
        }
        default: /* parent */
            close(clientfd);
            client_arr[uid].pid = cpid;
            /* notify child broadcast login message */
            sem_post(&client_arr[uid].sem);
            debug("parent uid: %d, pid %d\n", uid, client_arr[uid].pid);
            break;
        }
    }

    return 0;
}