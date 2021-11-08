#include "np_single_proc.h"


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

/* Built-in Commands */
void built_in(char *cmd_token, char *cmd_rest, char flag)
{
    char *buf = 0;                      /* output buffer */
    struct built_in_arg arg = {"", ""}; /* args for built-in commands */
    switch (flag) {
    /* setenv */
    case 's':
        cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest);
        strncpy(arg.name, cmd_token, strlen(cmd_token));
        cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest);
        strncpy(arg.value, cmd_token, strlen(cmd_token));
        if (setenv(arg.name, arg.value, 1) == -1) {
            perror("setenv\n");
        }
        /* update client environment variable */
        for (int i = 0; i < MAX_ENV; ++i) {
            if (client_arr[uid].env[i].isValid && 
                strncmp(client_arr[uid].env[i].key, arg.name, strlen(client_arr[uid].env[i].key)) == 0 ) {
                strncpy(client_arr[uid].env[i].val, arg.value, strlen(arg.value));
                break;
            }
        }
        break;
    /* printenv */
    case 'p':
        cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest);
        strncpy(arg.name, cmd_token, strlen(cmd_token));
        buf = getenv(arg.name);
        if (buf != NULL) {
            printf("%s\n", buf);
        }
        break;
    /* exit */
    case 'e':
        exit(EXIT_SUCCESS);
        break;
    default:
        perror("built_in no match\n");
        break;
    }
}

/* 
 * Key Concept:
 * Using a pipe array to maintain normal pipe and numbered pipe.
 * Each process will check if the array[0] is valid, it represents that 
 * the process has STDIN from other process(es).
 * If a process have to redirect its STDOUT/STDERR to a pipe, 
 * it will create a new or exploit an existing pipe in the corresponding index of array.
 * Each end of input Line will shift the pipe array left, 
 * it represent that the npshell read next line, and maintain it for the numbered pipe.
 * 
 * To sum up, a process will POP from the array[0], and PUSH a pipefd into array if needed.
 */

void execCmd(char *cmd_token, char *cmd_rest, struct cmd_arg cmd_arg)
{
    /* pipefd[0] refers to the read end of the pipe.  
       pipefd[1] refers to the write end of the pipe. */
    int pipefd_rhs[2] = {-1, -1}; /* right hand side(rhs) pipefd */
    bool isNewPipe = false;       /* check if create a new pipe */
    /* if it is a normal anonymous pipe, create a rhs pipe */
    if (cmd_arg.isPipe) {
        debug("normal anonymous pipe\n");
        if (pipe(pipefd_rhs) == -1) {
            perror("pipe rhs error");
            exit(EXIT_FAILURE);
        }
        isNewPipe = true;
    } /* check whether create new pipe, if needed, create new one */
    else if (cmd_arg.isNumPipe || cmd_arg.isErrPipe) {
        if (!pipe_arr[uid][cmd_arg.numPipeLen].isValid) {
            debug("Num/ERR Pipe len: %ld\n", cmd_arg.numPipeLen);
            if (pipe(pipefd_rhs) == -1) {
                perror("pipe rhs error");
                exit(EXIT_FAILURE);
            }
            pipe_arr[uid][cmd_arg.numPipeLen].isValid = true;
            pipe_arr[uid][cmd_arg.numPipeLen].pipefd[0] = pipefd_rhs[0];
            pipe_arr[uid][cmd_arg.numPipeLen].pipefd[1] = pipefd_rhs[1];
            isNewPipe = true;
        }
    }

    pid_t cpid;                      /* child pid */
    char *exec_argv[ARGSLIMIT] = {0}; /* execute arguments */
    bool isForkErr = false;          /* check if fork error occurred */

    switch (cpid = fork()) {
    case -1: /* fork error, reach process max limit, it will re-fork later */
        isForkErr = true;
        break;
    case 0: /* child */
        exec_argv[0] = cmd_token;
        /* extract arguments of command */
        for (int j = 1;
             (cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest)) != NULL; j++) {
            exec_argv[j] = cmd_token;
        }
        /* I/O Processing */
        /* replace STDIN */
        if (pipe_arr[uid][0].isValid) {
            debug("replace STDIN\n");
            dup2(pipe_arr[uid][0].pipefd[0], STDIN_FILENO);
            close(pipe_arr[uid][0].pipefd[0]);
            close(pipe_arr[uid][0].pipefd[1]);
        }

        /* replace STDOUT & STDERR */
        if (cmd_arg.isPipe) {
            debug("child cmd_arg.isPipe\n");
            dup2(pipefd_rhs[1], STDOUT_FILENO);
            close(pipefd_rhs[0]);
            close(pipefd_rhs[1]);
        } else if (cmd_arg.isNumPipe) {
            debug("child cmd_arg.isNumPipe\n");
            dup2(pipe_arr[uid][cmd_arg.numPipeLen].pipefd[1], STDOUT_FILENO);
            close(pipe_arr[uid][cmd_arg.numPipeLen].pipefd[0]);
            close(pipe_arr[uid][cmd_arg.numPipeLen].pipefd[1]);
        } else if (cmd_arg.isErrPipe) {
            debug("child cmd_arg.isErrPipe\n");
            dup2(pipe_arr[uid][cmd_arg.numPipeLen].pipefd[1], STDOUT_FILENO);
            dup2(pipe_arr[uid][cmd_arg.numPipeLen].pipefd[1], STDERR_FILENO);
            close(pipe_arr[uid][cmd_arg.numPipeLen].pipefd[0]);
            close(pipe_arr[uid][cmd_arg.numPipeLen].pipefd[1]);
        } else if (cmd_arg.isFileRedirect) {
            debug("child cmd_arg.isFileRedirect: %s\n", cmd_arg.filename);
            freopen(cmd_arg.filename, "w+", stdout);
        }

        /* close useless numbered pipes */
        for (int i = 1; i < 1002; i++) {
            if (pipe_arr[uid][i].isValid) {  //  && (i != cmd_arg.numPipeLen)
                close(pipe_arr[uid][i].pipefd[0]);
                close(pipe_arr[uid][i].pipefd[1]);
            }
        }

        /* execute command */
        if (execvp(exec_argv[0], exec_argv) == -1) {
            char unknown_cmd[CMDSIZE] = "Unknown command: [";
            strncat(unknown_cmd, exec_argv[0], strlen(exec_argv[0]));
            strcat(unknown_cmd, "].");
            fprintf(stderr, "%s\n", unknown_cmd);
        }
        debug("after execvp, Unknown command;\n");
        exit(EXIT_SUCCESS);
        break;
    default: /* parent */
        /* close useless input pipe */
        if (pipe_arr[uid][0].isValid) {
            debug("parent isValid\n");
            close(pipe_arr[uid][0].pipefd[0]);
            close(pipe_arr[uid][0].pipefd[1]);
            pipe_arr[uid][0].isValid = false;
        }
        /* normal pipe, add a pipefd to array */
        if (cmd_arg.isPipe) {
            debug("parent cmd_arg.isPipe\n");
            pipe_arr[uid][0].isValid = true;
            pipe_arr[uid][0].pipefd[0] = pipefd_rhs[0];
            pipe_arr[uid][0].pipefd[1] = pipefd_rhs[1];
        }

        /* Only wait the visible command situations (influencing the prompt) */
        if (cmd_arg.isFileRedirect || !(cmd_arg.isNumPipe || cmd_arg.isErrPipe || cmd_arg.isPipe)) {
            debug("Wait this command\n");
            while (waitpid(-1, NULL, WNOHANG) >= 0)
                ;
        }
        break;
    }
    debug("End of exec Func\n");
    /* re-fork */
    if (isForkErr) {
        debug("isForkErr\n");
        usleep(1000);
        /* close new pipe */
        if (isNewPipe) {
            close(pipefd_rhs[0]);
            close(pipefd_rhs[1]);
            if (cmd_arg.isNumPipe || cmd_arg.isErrPipe) {
                close(pipefd_rhs[0]);
                close(pipefd_rhs[1]);
                pipe_arr[uid][cmd_arg.numPipeLen].isValid = false;
            }
        }
        /* redo this function */
        execCmd(cmd_token, cmd_rest, cmd_arg);
    }
}

void child_handler(int signum)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int npshell()
{
    /* initial PATH is bin/ and ./ */
    clearenv();
    for (int i = 0; i < MAX_ENV; ++i) {
        if (client_arr[uid].env[i].isValid && setenv(client_arr[uid].env[i].key, client_arr[uid].env[i].val, 1) == -1) {
            perror("setenv\n");
        }
    }

    /* signal handlers, recycle process immediately, don't throw to .init */
    struct sigaction sa;
    sa.sa_handler = child_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    char read_buf[15005] = ""; /* read buffer */
    // size_t read_len = 0; /* record read buffer length */
    // if (getline(&read_buf, &read_len, stdin) < 0) {
    //     debug("getline < 0\n");
    //     return -1;
    // }
    recv(currfd, read_buf, 15005, 0);
    debug("-------------After getline\n");
    // printf("%s\n", read_buf);

    /* The following variables will be reset each read line*/
    char *read_token; /* record readline token for split */
    char *read_rest = read_buf;
    size_t numOfCmd = 0; /* number of commands in a line */
    /* loop for each command*/
    while (read_rest[0] != '\0') {
        /* The following variables will be reset each command*/
        char command[CMDSIZE] = {0};
        struct cmd_arg cmd_arg = {0}; /* args for execution commands */

        /* extract a command */
        /* record different options */
        while ((read_token = strtok_r(read_rest, " \n", &read_rest)) != NULL) {
            if (strncmp(read_token, "|", strlen(read_token)) == 0) {
                debug("read_token: %s\n", read_token);
                cmd_arg.isPipe = true;
                break;
            } else if (strncmp(read_token, ">", strlen(read_token)) == 0) {
                cmd_arg.isFileRedirect = true;
                read_token = strtok_r(read_rest, " \n", &read_rest);
                strncpy(cmd_arg.filename, read_token, strlen(read_token));
                break;
            } else if (read_token[0] == '|') {
                cmd_arg.isNumPipe = true;
                cmd_arg.numPipeLen = strtol(read_token + 1, NULL, 10);
                break;
            } else if (read_token[0] == '!') {
                cmd_arg.isErrPipe = true;
                cmd_arg.numPipeLen = strtol(read_token + 1, NULL, 10);
                break;
            } else {
                strncat(command, read_token, strlen(read_token));
                strcat(command, " ");
            }
        }
        /* strip the last space */
        command[strlen(command) - 1] = (command[strlen(command) - 1] == ' ') ? '\0' : command[strlen(command) - 1];
        debug("command: %s len: %ld\n", command, strlen(command));

        /* check and execute a command*/
        char *cmd_token = 0; /* record command token for split */
        char *cmd_rest = command;
        /* prevent empty command */
        if (command[0] != '\0') {
            numOfCmd++; /* record how many command in this line */
            cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest);

            /* built-in command */
            if (strncmp(cmd_token, "setenv", strlen("setenv")) == 0) {
                built_in(cmd_token, cmd_rest, 's');
            } else if (strncmp(cmd_token, "printenv", strlen("printenv")) == 0) {
                built_in(cmd_token, cmd_rest, 'p');
            } else if (strncmp(cmd_token, "exit", strlen("exit")) == 0) {
                // built_in(cmd_token, cmd_rest, 'e');
                while (waitpid(-1, NULL, WNOHANG) >= 0);
                return 1;
            } /* execution command */
            else {
                execCmd(cmd_token, cmd_rest, cmd_arg);
            }
            debug("--------A command---------\n");
        }
    }
    /* if it isn't a empty command line, shift-left 1 for number pipe */
    if (numOfCmd > 0) {
        memmove(pipe_arr[uid], pipe_arr[uid] + 1, sizeof(struct pipe_unit) * 1002);
        debug("--------A read line---------%ld\n", numOfCmd);
    }

    /* print prompt */
    printf("%% ");
    fflush(stdout);
    
    return 0;
}

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
    struct epoll_event ev, events[MAX_EVENTS];
    
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
        // int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        int nfds = 0;
        do {
            nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        } while (nfds < 0 && errno == EINTR);

        // if (nfds == -1) {
        //     perror("epoll_wait");
        //     exit(EXIT_FAILURE);
        // }
        printf("--------------after epoll_wait\n");

        for (int n = 0; n < nfds; ++n) {
            /* server */
            if (events[n].data.fd == servfd) {
                struct sockaddr_in clientAddr;
                socklen_t addrlen = sizeof(clientAddr);

                int clientfd = accept(servfd, (struct sockaddr *) &clientAddr, &addrlen);
                printf("accept fd: %d\n", clientfd);
                if (clientfd == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                
                /* initialize a new client */
                char ip_tmp[INET_ADDRSTRLEN + 2] = {'\0'};
                inet_ntop(AF_INET, &clientAddr.sin_addr, ip_tmp, INET_ADDRSTRLEN);
                debug("ip: %s\n", ip_tmp);

                struct client_unit tmp = {true, "(no name)", clientfd, clientAddr.sin_port, ""};
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
                        printf("Add client into uid: %d\n", i);
                        break;
                    }
                }

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
                printf("UID: %d, currfd: %d\n", uid, currfd);

                /* redirect IO to socket */
                for (int i = 0; i < 3; ++i) {
                    dup2(currfd, i);
                }
                /* run np shell */
                if(npshell() == 1) {
                    /* exit */
                    // broadcast exit msg

                    /* close client_arr */
                    client_arr[uid].isValid = false;

                    /* close pipe_arr */
                    for (int i = 0; i < 1003; ++i) {
                        if(pipe_arr[uid][i].isValid) {
                            pipe_arr[uid][i].isValid = false;
                            close(pipe_arr[uid][i].pipefd[0]);
                            close(pipe_arr[uid][i].pipefd[1]);
                        }
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