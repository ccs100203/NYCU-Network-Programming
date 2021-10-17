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
void built_in(char flag, struct built_in_arg args)
{
    char *buf;
    switch (flag) {
    case 's':
        if (setenv(args.name, args.value, 1) == -1) {
            perror("setenv\n");
        }
        break;
    case 'p':
        buf = getenv(args.name);
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
    int pipefd_r[2] = {-1, -1};  // right pipefd
    /* check whenever create new pipe, if needed, create new one */
    if (cmd_arg.isPipe) {
        debug("cmd_arg.isPipe\n");
        if (pipe(pipefd_r) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    } else if (cmd_arg.isNumPipe || cmd_arg.isErrPipe) {
        if (!pipe_arr[cmd_arg.numPipeLen].isValid) {
            debug("cmd_arg.isNumPipe + ERR len: %ld\n", cmd_arg.numPipeLen);
            if (pipe(pipefd_r) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            pipe_arr[cmd_arg.numPipeLen].isValid = true;
            pipe_arr[cmd_arg.numPipeLen].pipefd[0] = pipefd_r[0];
            pipe_arr[cmd_arg.numPipeLen].pipefd[1] = pipefd_r[1];
        }
    }

    pid_t cpid;
    char *exec_argv[ARGSLIMIT] = {}; /* execute arguments */

    switch (cpid = fork()) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);
        break;
    case 0: /* child */
        exec_argv[0] = cmd_token;
        /* extract arguments of command */
        for (int j = 1;
             (cmd_token = strtok_r(cmd_rest, " ", &cmd_rest)) != NULL; j++) {
            exec_argv[j] = cmd_token;
        }

        /* I/O Processing */
        /* replace STDIN */
        if (pipe_arr[0].isValid) {
            debug("child pipe_arr[0].isValid\n");
            close(STDIN_FILENO);
            dup2(pipe_arr[0].pipefd[0], STDIN_FILENO);
        }

        /* replace STDOUT */
        if (cmd_arg.isPipe) {
            debug("child cmd_arg.isPipe\n");
            close(STDOUT_FILENO);
            dup2(pipefd_r[1], STDOUT_FILENO);
        } else if (cmd_arg.isNumPipe) {
            debug("child cmd_arg.isNumPipe\n");
            close(STDOUT_FILENO);
            dup2(pipe_arr[cmd_arg.numPipeLen].pipefd[1], STDOUT_FILENO);
        } else if (cmd_arg.isErrPipe) {
            debug("child cmd_arg.isErrPipe\n");
            close(STDOUT_FILENO);
            dup2(pipe_arr[cmd_arg.numPipeLen].pipefd[1], STDOUT_FILENO);
            close(STDERR_FILENO);
            dup2(pipe_arr[cmd_arg.numPipeLen].pipefd[1], STDERR_FILENO);
        } else if (cmd_arg.isFileRedirect) {
            debug("child cmd_arg.isFileRedirect\n");
            freopen(cmd_arg.filename, "w+", stdout);
        }

        /* close useless numbered pipes */
        for (int i = 1; i < 1002; i++) {
            if (pipe_arr[i].isValid && (i != cmd_arg.numPipeLen)) {
                close(pipe_arr[i].pipefd[0]);
                close(pipe_arr[i].pipefd[1]);
            }
        }

        /* execute command */
        if (execvp(exec_argv[0], exec_argv) == -1) {
            char unknown_cmd[100] = "Unknown command: [";
            strncat(unknown_cmd, exec_argv[0], strlen(exec_argv[0]));
            strcat(unknown_cmd, "].\n");
            write(STDERR_FILENO, unknown_cmd, strlen(unknown_cmd));
        }
        debug("_exit(EXIT_SUCCESS);\n");
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
            pipe_arr[0].pipefd[0] = pipefd_r[0];
            pipe_arr[0].pipefd[1] = pipefd_r[1];
        }
        // check pipe
        // for (int i = 0; i < 5; i++) {
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
            break;
        }

        /* The following variables will be reset each read line*/
        char *read_token; /* record readline token for split */
        char *read_rest = read_buf;
        pipeLen = 0;
        /* loop for each command*/
        while (read_rest[0] != '\0') {
            /* The following variables will be reset each command*/
            char command[266] = "";
            struct cmd_arg cmd_arg = {0};

            /* extract a command */
            /* record different options */
            while ((read_token = strtok_r(read_rest, " \n", &read_rest)) != NULL) {
                if (strncmp(read_token, "|", strlen(read_token)) == 0) {
                    cmd_arg.isPipe = true;
                    break;
                } else if (strncmp(read_token, ">", strlen(">")) == 0) {
                    cmd_arg.isFileRedirect = true;
                    read_token = strtok_r(read_rest, " \n", &read_rest);
                    strncpy(cmd_arg.filename, read_token, strlen(read_token));
                    break;
                } else if (read_token[0] == '|') {
                    cmd_arg.isNumPipe = true;
                    cmd_arg.numPipeLen = strtol(read_token + 1, NULL, 10);
                    if (errno != 0) {
                        perror("strtol");
                        exit(EXIT_FAILURE);
                    }
                    break;
                } else if (read_token[0] == '!') {
                    cmd_arg.isErrPipe = true;
                    cmd_arg.numPipeLen = strtol(read_token + 1, NULL, 10);
                    if (errno != 0) {
                        perror("strtol");
                        exit(EXIT_FAILURE);
                    }
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
            if (command[0] != '\0') {
                pipeLen++;  // record how many command in this line
                cmd_token = strtok_r(cmd_rest, " ", &cmd_rest);

                /* built-in command */
                if (strncmp(cmd_token, "setenv", strlen("setenv")) == 0) {
                    struct built_in_arg arg = {"", ""};
                    cmd_token = strtok_r(cmd_rest, " ", &cmd_rest);
                    strncpy(arg.name, cmd_token, strlen(cmd_token));
                    cmd_token = strtok_r(cmd_rest, " ", &cmd_rest);
                    strncpy(arg.value, cmd_token, strlen(cmd_token));
                    built_in('s', arg);
                } else if (strncmp(cmd_token, "printenv", strlen("printenv")) == 0) {
                    struct built_in_arg arg = {"", ""};
                    cmd_token = strtok_r(cmd_rest, " ", &cmd_rest);
                    strncpy(arg.name, cmd_token, strlen(cmd_token));
                    built_in('p', arg);
                } else if (strncmp(cmd_token, "exit", strlen("exit")) == 0) {
                    struct built_in_arg arg = {"", ""};
                    built_in('e', arg);
                } /* execute command */
                else {
                    execCmd(cmd_token, cmd_rest, cmd_arg);
                }
            }

            fflush(stdout);
            debug("--------A command---------\n");
        }
        /* shift-left 1 for number pipe */
        if (pipeLen > 0) {
            memmove(pipe_arr, pipe_arr + 1, sizeof(struct pipe_unit) * 1000);
            debug("--------A read line---------\n");
        }
    }
    return 0;
}