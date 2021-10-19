#include "npshell.h"

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
    char *buf;                          /* output buffer */
    struct built_in_arg arg = {"", ""}; /* args for built-in commands */
    switch (flag) {
    case 's':
        cmd_token = strtok_r(cmd_rest, " ", &cmd_rest);
        strncpy(arg.name, cmd_token, strlen(cmd_token));
        cmd_token = strtok_r(cmd_rest, " ", &cmd_rest);
        strncpy(arg.value, cmd_token, strlen(cmd_token));
        if (setenv(arg.name, arg.value, 1) == -1) {
            perror("setenv\n");
        }
        break;
    case 'p':
        cmd_token = strtok_r(cmd_rest, " ", &cmd_rest);
        strncpy(arg.name, cmd_token, strlen(cmd_token));
        buf = getenv(arg.name);
        if (buf != NULL) {
            strcat(buf, "\n");
            write(STDOUT_FILENO, buf, strlen(buf));
        }
        break;
    case 'e':
        exit(EXIT_SUCCESS);
        break;
    default:
        perror("built_in no match\n");
        break;
    }
}

void execCmd(char *cmd_token, char *cmd_rest, struct cmd_arg cmd_arg)
{
    /* pipefd[0] refers to the read end of the pipe.  
       pipefd[1] refers to the write end of the pipe. */
    int pipefd_rhs[2] = {-1, -1};  // right hand side(rhs) pipefd
    /* if it is a normal anonymous pipe, create a rhs pipe */
    if (cmd_arg.isPipe) {
        debug("normal anonymous pipe\n");
        if (pipe(pipefd_rhs) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    } /* check whether create new pipe, if needed, create new one */
    else if (cmd_arg.isNumPipe || cmd_arg.isErrPipe) {
        if (!pipe_arr[cmd_arg.numPipeLen].isValid) {
            debug("Num/ERR Pipe len: %ld\n", cmd_arg.numPipeLen);
            if (pipe(pipefd_rhs) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            pipe_arr[cmd_arg.numPipeLen].isValid = true;
            pipe_arr[cmd_arg.numPipeLen].pipefd[0] = pipefd_rhs[0];
            pipe_arr[cmd_arg.numPipeLen].pipefd[1] = pipefd_rhs[1];
        }
    }

    pid_t cpid;                      /* child pid */
    char *exec_argv[ARGSLIMIT] = {}; /* execute arguments */

FORK_AGAIN:

    switch (cpid = fork()) {
    case -1:
        // perror("fork");
        // exit(EXIT_FAILURE);
        usleep(500);
        goto FORK_AGAIN;
        break;
    case 0: /* child */
        exec_argv[0] = cmd_token;
        /* extract arguments of command */
        for (int j = 1;
             (cmd_token = strtok_r(cmd_rest, " ", &cmd_rest)) != NULL; j++) {
            exec_argv[j] = cmd_token;
        }
        // DEL
        // for (int i = 0; i < 5; i++) {
        //     printf("%s ", exec_argv[i]);
        // }
        // printf("\n");

        /* I/O Processing */
        /* replace STDIN */
        if (pipe_arr[0].isValid) {
            debug("replace STDIN\n");
            // close(STDIN_FILENO);
            dup2(pipe_arr[0].pipefd[0], STDIN_FILENO);
            close(pipe_arr[0].pipefd[1]);
        }

        /* replace STDOUT & STDERR */
        if (cmd_arg.isPipe) {
            debug("child cmd_arg.isPipe\n");
            // close(STDOUT_FILENO);
            dup2(pipefd_rhs[1], STDOUT_FILENO);
            close(pipefd_rhs[0]);
        } else if (cmd_arg.isNumPipe) {
            debug("child cmd_arg.isNumPipe\n");
            // close(STDOUT_FILENO);
            dup2(pipe_arr[cmd_arg.numPipeLen].pipefd[1], STDOUT_FILENO);
            close(pipe_arr[cmd_arg.numPipeLen].pipefd[0]);
        } else if (cmd_arg.isErrPipe) {
            debug("child cmd_arg.isErrPipe\n");
            // close(STDOUT_FILENO);
            dup2(pipe_arr[cmd_arg.numPipeLen].pipefd[1], STDOUT_FILENO);
            // close(STDERR_FILENO);
            dup2(pipe_arr[cmd_arg.numPipeLen].pipefd[1], STDERR_FILENO);
            close(pipe_arr[cmd_arg.numPipeLen].pipefd[0]);
        } else if (cmd_arg.isFileRedirect) {
            debug("child cmd_arg.isFileRedirect: %s\n", cmd_arg.filename);
            freopen(cmd_arg.filename, "w+", stdout);
        }

        /* close useless numbered pipes */
        for (int i = 1; i < 1002; i++) {
            if (pipe_arr[i].isValid) {  //  && (i != cmd_arg.numPipeLen)
                close(pipe_arr[i].pipefd[0]);
                close(pipe_arr[i].pipefd[1]);
            }
        }

        /* execute command */
        if (execvp(exec_argv[0], exec_argv) == -1) {
            char unknown_cmd[CMDSIZE] = "Unknown command: [";
            strncat(unknown_cmd, exec_argv[0], strlen(exec_argv[0]));
            strcat(unknown_cmd, "].\n");
            write(STDERR_FILENO, unknown_cmd, strlen(unknown_cmd));
        }
        debug("after execvp _exit(EXIT_SUCCESS);\n");
        _exit(EXIT_SUCCESS);
        break;
    default: /* parent */
        if (pipe_arr[0].isValid) {
            debug("parent isValid\n");
            close(pipe_arr[0].pipefd[0]);
            close(pipe_arr[0].pipefd[1]);
            pipe_arr[0].isValid = false;
        }
        if (cmd_arg.isPipe) {
            debug("parent cmd_arg.isPipe\n");
            pipe_arr[0].isValid = true;
            pipe_arr[0].pipefd[0] = pipefd_rhs[0];
            pipe_arr[0].pipefd[1] = pipefd_rhs[1];
        }
        // check pipe DEL
        // for (int i = 0; i < 9; i++) {
        //     printf("------%d %d %d", pipe_arr[i].isValid, pipe_arr[i].pipefd[0], pipe_arr[i].pipefd[1]);
        // }
        // printf("\n");

        /* The last command in a line, and wait all previous commands finished. */
        if (cmd_arg.isFileRedirect || cmd_arg.isNumPipe || cmd_arg.isErrPipe || !cmd_arg.isPipe) {
            debug("Last command\n");
            while (waitpid(-1, NULL, WNOHANG) >= 0)
                ;
        }
        break;
    }
    debug("End of exec Func\n");
}

static void child_handler(int signum)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int main(int argc, char **argv)
{
    /* initial PATH is bin/ and ./ */
    if (setenv("PATH", "bin:.", 1) == -1) {
        perror("setenv\n");
    }

    /* signal handlers */
    struct sigaction sa;
    sa.sa_handler = child_handler;
    sigemptyset(&sa.sa_mask);
    // sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    // sigaction(SIGCHLD, &sa, NULL);

    char *read_buf;       // read buffer
    size_t read_len = 0;  // record read buffer length
    /* loop for each line */
    while (1) {
        /* print prompt */
        write(STDOUT_FILENO, "\% ", strlen("\% "));
        if (getline(&read_buf, &read_len, stdin) < 0) {
            debug("getline < 0\n");
            break;
        }

        /* The following variables will be reset each read line*/
        char *read_token; /* record readline token for split */
        char *read_rest = read_buf;
        numOfCmd = 0;
        /* loop for each command*/
        while (read_rest[0] != '\0') {
            /* The following variables will be reset each command*/
            char command[CMDSIZE] = "";
            struct cmd_arg cmd_arg = {0}; /* args for execution commands */

            /* extract a command */
            /* record different options */
            while ((read_token = strtok_r(read_rest, " \n", &read_rest)) != NULL) {
                if (strncmp(read_token, "|", strlen(read_token)) == 0) {
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
            debug("command: %s\n", command);

            /* check and execute a command*/
            char *cmd_token = 0; /* record command token for split */
            char *cmd_rest = command;
            /* prevent empty command */
            if (command[0] != '\0') {
                numOfCmd++; /* record how many command in this line */
                cmd_token = strtok_r(cmd_rest, " ", &cmd_rest);

                /* built-in command */
                if (strncmp(cmd_token, "setenv", strlen("setenv")) == 0) {
                    built_in(cmd_token, cmd_rest, 's');
                } else if (strncmp(cmd_token, "printenv", strlen("printenv")) == 0) {
                    built_in(cmd_token, cmd_rest, 'p');
                } else if (strncmp(cmd_token, "exit", strlen("exit")) == 0) {
                    built_in(cmd_token, cmd_rest, 'e');
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