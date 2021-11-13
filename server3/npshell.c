#include "npshell.h"
#include "np_multi_proc.h"

struct pipe_unit pipe_arr[1010] = {0}; /* record number pipe */
struct usrpipe_unit *usr_pipe_arr[MAX_CLIENT] = {0}; /* record user pipe */
int readfd_arr[MAX_CLIENT] = {0}; /* record read side user_pipe */

/* Built-in Commands */
void built_in(char *cmd_token, char *cmd_rest, char flag)
{
    struct built_in_arg arg = {"", ""}; /* args for built-in commands */
    switch (flag) {
    /* setenv */
    case 's':
    {
        cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest);
        strncpy(arg.name, cmd_token, strlen(cmd_token));
        cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest);
        strncpy(arg.value, cmd_token, strlen(cmd_token));
        if (setenv(arg.name, arg.value, 1) == -1) {
            perror("setenv\n");
        }
        break;
    } /* printenv */
    case 'p':
    {
        cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest);
        strncpy(arg.name, cmd_token, strlen(cmd_token));
        char *buf = 0;
        buf = getenv(arg.name);
        if (buf != NULL) {
            printf("%s\n", buf);
        }
        break;
    } /* exit */
    case 'e':
    {
        exit(EXIT_SUCCESS);
        break;
    } /* who */
    case 'w':
    {
        printf("<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
        for(int i=0; i < MAX_CLIENT; ++i) {
            if(client_arr[i].isValid) {
                char buf[100] = "";
                sprintf(buf, "%d\t%s\t%s:%d", i+1, client_arr[i].name, 
                        client_arr[i].ip, client_arr[i].port);
                if(uid == i)
                    strcat(buf, "\t<-me");
                printf("%s\n", buf);
            }
        }
        break;
    } /* name */
    case 'n':
    {
		bool isRepeat = false;
		for(int i = 0; i < MAX_CLIENT; ++i){
			if(client_arr[i].isValid && strcmp(client_arr[i].name, cmd_rest) == 0){
				isRepeat = true;
				break;
			} 
		}
		if(isRepeat) {
			printf("*** User '%s' already exists. ***\n", cmd_rest);
		} else {
			strcpy(client_arr[uid].name, cmd_rest);
            char buf[100] = "";
            sprintf(buf, "*** User from %s:%d is named '%s'. ***\n", 
                    client_arr[uid].ip, client_arr[uid].port, client_arr[uid].name);
            broadcast(buf);
		}
        break;
    } /* tell */
    case 't':
    {
        cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest);
        size_t recv_id = strtol(cmd_token, NULL, 10);
        recv_id--;
        if(client_arr[recv_id].isValid) {
            char buf[MSGSIZE] = "";
            sprintf(buf, "*** %s told you ***: %s\n", client_arr[uid].name, cmd_rest);
            unicast(buf, client_arr[recv_id].pid);
        } else {
            printf("*** Error: user #%ld does not exist yet. ***\n", recv_id + 1);
        }
        break;
    } /* yell */
    case 'y':
    {
		char buf[MSGSIZE] = "";
		sprintf(buf, "*** %s yelled ***: %s\n", client_arr[uid].name, cmd_rest);
        broadcast(buf);
        break;
    }
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
        if (!pipe_arr[cmd_arg.numPipeLen].isValid) {
            debug("Num/ERR Pipe len: %ld\n", cmd_arg.numPipeLen);
            if (pipe(pipefd_rhs) == -1) {
                perror("pipe rhs error");
                exit(EXIT_FAILURE);
            }
            pipe_arr[cmd_arg.numPipeLen].isValid = true;
            pipe_arr[cmd_arg.numPipeLen].pipefd[0] = pipefd_rhs[0];
            pipe_arr[cmd_arg.numPipeLen].pipefd[1] = pipefd_rhs[1];
            isNewPipe = true;
        }
    }

    /* check if Recv user pipe is valid */
    bool isRecvErr = false; /* is recv user pipe is correct */
    if(cmd_arg.isRecvUsrPipe) {
        /* out of bound, user is not exist */
        if(cmd_arg.recvUId >= MAX_CLIENT) {
            char buf[MAX_LINE] = "";
            sprintf(buf, "*** Error: user #%ld does not exist yet. ***\n", cmd_arg.recvUId+1);
            printf("%s", buf);
            isRecvErr = true;
        } /* if pipe exists, success recv */
        else if(usr_pipe_arr[cmd_arg.recvUId][uid].isValid) {
            char buf[MAX_LINE + 200] = "";
            sprintf(buf, "*** %s (#%d) just received from %s (#%ld) by '%s' ***\n", 
                    client_arr[uid].name, uid+1, client_arr[cmd_arg.recvUId].name, 
                    cmd_arg.recvUId+1, usr_pipe_arr[cmd_arg.recvUId][uid].cmd);
            broadcast(buf);
        } /* user exists, but pipe doesn't exist */ 
        else if (client_arr[cmd_arg.recvUId].isValid) {
            char buf[MAX_LINE] = "";
            sprintf(buf, "*** Error: the pipe #%ld->#%d does not exist yet. ***\n", 
                    cmd_arg.recvUId+1, uid+1);
            printf("%s", buf);
            isRecvErr = true;
        } /* user doesn't exist */
        else {
            char buf[MAX_LINE] = "";
            sprintf(buf, "*** Error: user #%ld does not exist yet. ***\n", cmd_arg.recvUId+1);
            printf("%s", buf);
            isRecvErr = true;
        }
    }
    usleep(1000);

    /* check if Send user pipe is valid */
    bool isSendErr = false; /* is send user pipe is correct */
    if(cmd_arg.isSendUsrPipe) {
        /* out of bound, or user doesn't exist */
        if(cmd_arg.sendUId >= MAX_CLIENT || !client_arr[cmd_arg.sendUId].isValid) {
            char buf[MAX_LINE] = "";
            sprintf(buf, "*** Error: user #%ld does not exist yet. ***\n", cmd_arg.sendUId+1);
            printf("%s", buf);
            isSendErr = true;
        } /* if exist pipe, fail cause duplicate send */
        else if (usr_pipe_arr[uid][cmd_arg.sendUId].isValid) {
            char buf[MAX_LINE] = "";
            sprintf(buf, "*** Error: the pipe #%d->#%ld already exists. ***\n", 
                    uid+1, cmd_arg.sendUId+1);
            printf("%s", buf);
            isSendErr = true;
        } /* user exists, but pipe doesn't exist, success send */
        else {
            char buf[MAX_LINE + 200] = "";
            sprintf(buf, "*** %s (#%d) just piped '%s' to %s (#%ld) ***\n", 
                    client_arr[uid].name, uid+1, usr_pipe_arr[uid][cmd_arg.sendUId].cmd,
                    client_arr[cmd_arg.sendUId].name, cmd_arg.sendUId+1);
            broadcast(buf);
            // isNewPipe = true;
            usr_pipe_arr[uid][cmd_arg.sendUId].isValid = true;
            /* create fifo for valid userpipe send */
            char path_fifo[20] = "";
            sprintf(path_fifo, "user_pipe/%d__%ld", uid, cmd_arg.sendUId);
            unlink(path_fifo);
            mkfifo(path_fifo, 0666);
        }
    }
    fflush(stdout);

    pid_t cpid;                         /* child pid */
    char *exec_argv[ARGSLIMIT] = {0};   /* execute arguments */
    bool isForkErr = false;             /* check if fork error occurred */

    switch ((cpid = fork())) {
    case -1: /* fork error, reach process max limit, it will re-fork later */
        isForkErr = true;
        break;
    case 0: /* child */
    {
        exec_argv[0] = cmd_token;
        /* extract arguments of command */
        for (int j = 1;
             (cmd_token = strtok_r(cmd_rest, " \n", &cmd_rest)) != NULL; j++) {
            exec_argv[j] = cmd_token;
        }
        /* create fifo for valid userpipe send */
        // if (cmd_arg.isSendUsrPipe && !isSendErr) {
        //     char path_fifo[20] = "";
        //     sprintf(path_fifo, "user_pipe/%d__%ld", uid, cmd_arg.sendUId);
        //     unlink(path_fifo);
        //     mkfifo(path_fifo, 0666);
        // }
        // dprintf(stdiofd[1], "child %d, %d, getpgrp %d\n", getpid(), getppid(), getpgrp());

        /* I/O Processing */
        /* replace STDIN */
        if (pipe_arr[0].isValid) {
            debug("replace STDIN\n");
            dup2(pipe_arr[0].pipefd[0], STDIN_FILENO);
            close(pipe_arr[0].pipefd[0]);
            close(pipe_arr[0].pipefd[1]);
        } /* /dev/null -> stdin */
        else if (isRecvErr) {
            int nullfd = open("/dev/null", O_RDONLY, 0);
            dup2(nullfd, STDIN_FILENO);
            close(nullfd);
        } else if (cmd_arg.isRecvUsrPipe) {
            debug("replace STDIN\n");
            // char path_fifo[20] = "";
            // sprintf(path_fifo, "user_pipe/%ld__%d", cmd_arg.recvUId, uid);
            // int readfd = open(path_fifo, O_RDONLY, 0);
            dup2(readfd_arr[cmd_arg.recvUId], STDIN_FILENO);
            close(readfd_arr[cmd_arg.recvUId]);
            readfd_arr[cmd_arg.recvUId] = -1;
        }

        /* replace STDOUT & STDERR */
        if (cmd_arg.isPipe) {
            debug("child cmd_arg.isPipe\n");
            dup2(pipefd_rhs[1], STDOUT_FILENO);
            close(pipefd_rhs[0]);
            close(pipefd_rhs[1]);
        } else if (cmd_arg.isNumPipe) {
            debug("child cmd_arg.isNumPipe\n");
            dup2(pipe_arr[cmd_arg.numPipeLen].pipefd[1], STDOUT_FILENO);
            close(pipe_arr[cmd_arg.numPipeLen].pipefd[0]);
            close(pipe_arr[cmd_arg.numPipeLen].pipefd[1]);
        } else if (cmd_arg.isErrPipe) {
            debug("child cmd_arg.isErrPipe\n");
            dup2(pipe_arr[cmd_arg.numPipeLen].pipefd[1], STDOUT_FILENO);
            dup2(pipe_arr[cmd_arg.numPipeLen].pipefd[1], STDERR_FILENO);
            close(pipe_arr[cmd_arg.numPipeLen].pipefd[0]);
            close(pipe_arr[cmd_arg.numPipeLen].pipefd[1]);
        } else if (cmd_arg.isFileRedirect) {
            debug("child cmd_arg.isFileRedirect: %s\n", cmd_arg.filename);
            freopen(cmd_arg.filename, "w+", stdout);
        } /* /dev/null -> stdout */
        else if (isSendErr) {
            int nullfd = open("/dev/null", O_WRONLY, 0);
            dup2(nullfd, STDOUT_FILENO);
            close(nullfd);
        } else if (cmd_arg.isSendUsrPipe) {
            debug("child cmd_arg.isSendUsrPipe\n");
            char path_fifo[20] = "";
            sprintf(path_fifo, "user_pipe/%d__%ld", uid, cmd_arg.sendUId);
            client_arr[cmd_arg.sendUId].who_send_mask |= (1 << uid);
            /* send a signal to receiver, make him open readside */
            kill(client_arr[cmd_arg.sendUId].pid, SIGUSR2);
            int writefd = open(path_fifo, O_WRONLY, 0);
            dup2(writefd, STDOUT_FILENO);
            close(writefd);
        }

        /* close useless numbered pipes */
        for (int i = 1; i < 1002; i++) {
            if (pipe_arr[i].isValid) {
                close(pipe_arr[i].pipefd[0]);
                close(pipe_arr[i].pipefd[1]);
            }
        }

        /* close useless backup fd */
        for (int i = 0; i < 3; ++i) {
            close(stdiofd[i]);
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
    } /* parent */
    default: 
        // dprintf(stdiofd[1], "parent %d, %d, getpgrp %d\n", getpid(), getppid(), getpgrp());
        /* close useless input pipe */
        if (pipe_arr[0].isValid) {
            debug("parent isValid\n");
            close(pipe_arr[0].pipefd[0]);
            close(pipe_arr[0].pipefd[1]);
            pipe_arr[0].isValid = false;
        }
        /* normal pipe, add a pipefd to array */
        if (cmd_arg.isPipe) {
            debug("parent cmd_arg.isPipe\n");
            pipe_arr[0].isValid = true;
            pipe_arr[0].pipefd[0] = pipefd_rhs[0];
            pipe_arr[0].pipefd[1] = pipefd_rhs[1];
        }

        /* close successful user pipe after recv */
        if(cmd_arg.isRecvUsrPipe && !isRecvErr) {
            usr_pipe_arr[cmd_arg.recvUId][uid].isValid = false;
        }

        /* Only wait the visible command situations (influencing the prompt) */
        if (!(cmd_arg.isNumPipe || cmd_arg.isErrPipe || cmd_arg.isPipe
                || (cmd_arg.isSendUsrPipe && !isSendErr))) {
            debug("Wait this command\n");
            // dprintf(stdiofd[1], "HI INSIDE wait\n");
            // while (waitpid(-1, NULL, WNOHANG) >= 0)
            //     ;
            waitpid(cpid, NULL, 0);
        }
        // dprintf(stdiofd[1], "HI AFTER wait\n");
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
                pipe_arr[cmd_arg.numPipeLen].isValid = false;
            }
            if (cmd_arg.isSendUsrPipe) {
                usr_pipe_arr[uid][cmd_arg.sendUId].isValid = false;
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
    /* initialize readfd to -1 */
    for (int i = 0; i < MAX_CLIENT; ++i) {
        readfd_arr[i] = -1;
    }

    /* initialize PATH is bin/ and ./ */
    clearenv();
    if (setenv("PATH", "bin:.", 1) == -1) {
        perror("setenv\n");
    }

    /* signal handlers, recycle process immediately, don't throw to .init */
    struct sigaction sa;
    sa.sa_handler = child_handler;
    sigemptyset(&sa.sa_mask);
    // sigfillset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    while (1) {
        /* print prompt */
        printf("%% ");
        fflush(stdout);

        char read_buf[MAX_LINE] = ""; /* read buffer */
        recv(currfd, read_buf, MAX_LINE, 0);
        // read(currfd, read_buf, MAX_LINE);
        debug("uid: %d\n", uid);
        debug("After getline a command line\n");

        /* The following variables will be reset each read line*/
        char *read_token = ""; /* record readline token for split */
        char *read_rest = strdup(read_buf);
        size_t numOfCmd = 0; /* number of commands in a line */
        /* loop for each command*/
        while (read_rest[0] != '\0') {
            /* The following variables will be reset each command*/
            char command[CMDSIZE] = {0};
            struct cmd_arg cmd_arg = {0}; /* args for execution commands */

            /* extract a command */
            /* record different options */
            while ((read_token = strtok_r(read_rest, " \r\n", &read_rest)) != NULL) {
                /* yell & tell & name */
                if(strcmp(read_token, "yell") == 0 || strcmp(read_token, "tell") == 0
                    || strcmp(read_token, "name") == 0) {
                    strcpy(command, read_token);
                    strcat(command, " ");
                    while ((read_token = strtok_r(read_rest, "\r\n", &read_rest)) != NULL)
                        strcat(command, read_token);
                    read_rest = "";
                    break;
                }

                if (strncmp(read_token, "|", strlen(read_token)) == 0) {
                    debug("normal pipe: %s\n", read_token);
                    cmd_arg.isPipe = true;
                    break;
                } else if (strncmp(read_token, ">", strlen(read_token)) == 0) {
                    cmd_arg.isFileRedirect = true;
                    read_token = strtok_r(read_rest, " \r\n", &read_rest);
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
                } else if (read_token[0] == '>') {
                    cmd_arg.isSendUsrPipe = true;
                    cmd_arg.sendUId = strtol(read_token + 1, NULL, 10) - 1;
                } else if (read_token[0] == '<') {
                    cmd_arg.isRecvUsrPipe = true;
                    cmd_arg.recvUId = strtol(read_token + 1, NULL, 10) - 1;
                } else {
                    strncat(command, read_token, strlen(read_token));
                    strcat(command, " ");
                }
            }
            /* strip the last space */
            command[strlen(command) - 1] = (command[strlen(command) - 1] == ' ') ? '\0' : command[strlen(command) - 1];
            debug("command: %s len: %ld\n", command, strlen(command));
            // dprintf(stdiofd[1], "command: %s len: %ld\n", command, strlen(command));

            char* unused = "";
            char* tmp_read = strtok_r(read_buf, "\r\n", &unused);
            /* save this command line into send user pipe arr */
            if (cmd_arg.isSendUsrPipe && cmd_arg.sendUId < MAX_CLIENT 
                && !usr_pipe_arr[uid][cmd_arg.sendUId].isValid && client_arr[cmd_arg.sendUId].isValid) {
                memset(usr_pipe_arr[uid][cmd_arg.sendUId].cmd, 0, MAX_LINE);
                strncpy(usr_pipe_arr[uid][cmd_arg.sendUId].cmd, tmp_read, strlen(tmp_read));
            }
            /* save this command line into recv user pipe arr */
            if (cmd_arg.isRecvUsrPipe && cmd_arg.recvUId < MAX_CLIENT 
                && usr_pipe_arr[cmd_arg.recvUId][uid].isValid) {
                memset(usr_pipe_arr[cmd_arg.recvUId][uid].cmd, 0, MAX_LINE);
                strncpy(usr_pipe_arr[cmd_arg.recvUId][uid].cmd, tmp_read, strlen(tmp_read));
            }

            /* check and execute a command*/
            char *cmd_token = 0; /* record command token for split */
            char *cmd_rest = command;
            /* prevent empty command */
            if (command[0] != '\0') {
                numOfCmd++; /* record how many command in this line */
                cmd_token = strtok_r(cmd_rest, " \r\n", &cmd_rest);

                /* built-in command */
                if (strncmp(cmd_token, "setenv", strlen("setenv")) == 0) {
                    built_in(cmd_token, cmd_rest, 's');
                } else if (strncmp(cmd_token, "printenv", strlen("printenv")) == 0) {
                    built_in(cmd_token, cmd_rest, 'p');
                } else if (strncmp(cmd_token, "exit", strlen("exit")) == 0) {
                    return 1;
                } else if (strncmp(cmd_token, "who", strlen("who")) == 0) {
                    built_in(cmd_token, cmd_rest, 'w');
                } else if (strncmp(cmd_token, "name", strlen("name")) == 0) {
                    built_in(cmd_token, cmd_rest, 'n');
                } else if (strncmp(cmd_token, "tell", strlen("tell")) == 0) {
                    built_in(cmd_token, cmd_rest, 't');
                } else if (strncmp(cmd_token, "yell", strlen("yell")) == 0) {
                    built_in(cmd_token, cmd_rest, 'y');
                } /* execution command */
                else {
                    execCmd(cmd_token, cmd_rest, cmd_arg);
                }
                debug("--------A command---------\n");
            }
        }
        /* if it isn't a empty command line, shift-left 1 for number pipe */
        if (numOfCmd > 0) {
            memmove(pipe_arr, pipe_arr + 1, sizeof(struct pipe_unit) * 1002);
            debug("--------A read line---------%ld\n", numOfCmd);
        }
    }
    return 0;
}
