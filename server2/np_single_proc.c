#include "np_single_proc.h"
#include "npshell.h"

/* The following array use `uid` as index */
struct client_unit client_arr[MAX_CLIENT + 1] = {0};
int currfd = 0; /* record current client fd */
int uid = 0; /* record current user id */
int stdiofd[3] = {0}; /* record stdiofd */

void welcome(int fd)
{
    send(fd, "****************************************\n", strlen("****************************************\n"), 0);
    send(fd, "** Welcome to the information server. **\n", strlen("** Welcome to the information server. **\n"), 0);
    send(fd, "****************************************\n", strlen("****************************************\n"), 0);
}

void broadcast(char *msg)
{
    for(int i = 0; i < MAX_CLIENT; ++i){
        if(client_arr[i].isValid)
            send(client_arr[i].sockfd, msg, strlen(msg), 0);
    }
}

int main(int argc, char **argv)
{
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
    listen(servfd, 5);

    /* create epoll */
    int epollfd;
    struct epoll_event ev, events[MAX_EVENTS+5];
    
    if ((epollfd = epoll_create1(0)) == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    /* Add server fd into epoll list */
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = servfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, servfd, &ev) == -1) {
        perror("epoll_ctl: servfd");
        exit(EXIT_FAILURE);
    }

    /* save STDIN/OUT/ERR */
    for(int i = 0; i < 3; ++i) {
        stdiofd[i] = dup(i);
    }

    while (1) {
        int nfds = 0;
        do {
            nfds = epoll_wait(epollfd, events, MAX_EVENTS+5, -1);
        } while (nfds < 0 && errno == EINTR);
        if(nfds < 0)
            printf("errno: %d\n", errno);

        // printf("--------------after epoll_wait\n");

        for (int n = 0; n < nfds; ++n) {
            /* server */
            if (events[n].data.fd == servfd) {
                struct sockaddr_in clientAddr;
                socklen_t addrlen = sizeof(clientAddr);

                int clientfd = accept(servfd, (struct sockaddr *) &clientAddr, &addrlen);
                // printf("accept fd: %d\n", clientfd);
                if (clientfd == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                
                /* initialize a new client */
                char ip_tmp[INET_ADDRSTRLEN + 2] = {'\0'};
                inet_ntop(AF_INET, &clientAddr.sin_addr, ip_tmp, INET_ADDRSTRLEN);
                debug("ip: %s\n", ip_tmp);

                struct client_unit tmp = {true, "(no name)", clientfd, ntohs(clientAddr.sin_port), ""};
                strncpy(tmp.ip, ip_tmp, strlen(ip_tmp));
                for(int i = 0; i < MAX_ENV; ++i)
                    tmp.env[i].isValid = false;
                tmp.env[0].isValid = true;
                strcpy(tmp.env[0].key, "PATH");
                strcpy(tmp.env[0].val, "bin:.");

                debug("%s %s %s %s %d %d\n", tmp.name, tmp.ip, tmp.env[0].key, tmp.env[0].val, tmp.port, tmp.sockfd);

                /* put new client into client array */
                for(int i = 0; i < MAX_CLIENT; i++) {
                    if(!client_arr[i].isValid) {
                        uid = i;
                        client_arr[i] = tmp;
                        // printf("Add client into uid: %d\n", i);
                        break;
                    }
                }
                dprintf(stdiofd[1], "User: %d enter \n", uid+1);


                /* send welcome message */
                welcome(clientfd);

                /* broadcast login message */
                char buf[100] = "";
                sprintf(buf, "*** User '%s' entered from %s:%d. ***\n", 
                    client_arr[uid].name, client_arr[uid].ip, client_arr[uid].port);
                broadcast(buf);

                /* send prompt */
                send(clientfd, "% ", 2, 0);

                /* Add client fd into epoll list */
                ev.events = EPOLLIN | EPOLLET; // edge trigger
                ev.data.fd = clientfd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev) == -1) {
                    perror("epoll_ctl: clientfd");
                    exit(EXIT_FAILURE);
                }
            } /* client */
            else {
                currfd = events[n].data.fd;
                for(int i = 0; i < MAX_CLIENT; i++) {
                    if(client_arr[i].isValid && client_arr[i].sockfd == currfd) {
                        uid = i;
                        break;
                    }
                }
                // printf("UID: %d, currfd: %d\n", uid, currfd);

                /* redirect IO to socket */
                for (int i = 0; i < 3; ++i) {
                    dup2(currfd, i);
                }
                /* run np shell */
                if(npshell() == 1) {
                    /* exit */
                    dprintf(stdiofd[1], "User: %d exit \n", uid+1);
                    /* close client's client_arr */
                    client_arr[uid].isValid = false;

                    /* close client's pipe_arr */
                    for (int i = 0; i < 1003; ++i) {
                        if(pipe_arr[uid][i].isValid) {
                            pipe_arr[uid][i].isValid = false;
                            close(pipe_arr[uid][i].pipefd[0]);
                            close(pipe_arr[uid][i].pipefd[1]);
                        }
                    }

                    /* close client's user pipe arr */
                    for (int i = 0; i < MAX_CLIENT; ++i) {
                        usr_pipe_arr[i][uid].isValid = false;
                        close(usr_pipe_arr[i][uid].pipefd[0]);
                        close(usr_pipe_arr[i][uid].pipefd[1]);
                    }
                    for (int i = 0; i < MAX_CLIENT; ++i) {
                        usr_pipe_arr[uid][i].isValid = false;
                        close(usr_pipe_arr[uid][i].pipefd[0]);
                        close(usr_pipe_arr[uid][i].pipefd[1]);
                    }
                    
                    /* Delete client fd from epoll list */
                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, currfd, NULL) == -1) {
                        perror("epoll_ctl del currfd");
                        exit(EXIT_FAILURE);
                    }

                    /* close this fd */
                    close(currfd);

                    /* broadcast logout message */
                    char buf[100] = "";
                    sprintf(buf, "*** User '%s' left. ***\n", client_arr[uid].name);
                    broadcast(buf);

                }

                /* restore stdiofd */
                for (int i = 0; i < 3; ++i) {
                    dup2(stdiofd[i], i);
                }
            }
        }
    }



    return 0;
}