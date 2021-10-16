#include "npshell.h"

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
        exit(0);
        break;
    default:
        perror("built_in no match\n");
        break;
    }
}

int main(int argc, char **argv)
{
    /* initial PATH is bin/ and ./ */
    if (setenv("PATH", "bin:.", 1) == -1) {
        perror("setenv\n");
    }

    char *read_buf;       // read buffer
    size_t read_len = 0;  // record read buffer len
    /* loop for each line */
    while (1) {
        /* print prompt */
        write(STDOUT_FILENO, "\% ", strlen("\% "));
        if (getline(&read_buf, &read_len, stdin) < 0) {
            break;
        }

        char *read_token; /* record readline token for split */
        char *read_rest = read_buf;
        /* loop for each command*/
        while (read_rest[0] != '\0') {
            char command[266] = "";
            bool isPipe = false;
            bool isFileRedirect = false;
            bool isNumPipe = false;
            size_t numPipeLen = 0;

            /* extract a command */
            /* record different options */
            while ((read_token = strtok_r(read_rest, " \n", &read_rest)) != NULL) {
                if (strncmp(read_token, "|", strlen(read_token)) == 0) {
                    isPipe = true;
                    break;
                } else if (strncmp(read_token, ">", strlen(">")) == 0) {
                    isFileRedirect = true;
                    break;
                } else if (read_token[0] == '|') {
                    numPipeLen = strtol(read_token + 1, NULL, 10);
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
            // printf("command: %s\n", command);


            /* check and execute a command*/
            char *cmd_token = 0; /* record command token for split */
            char *cmd_rest = command;
            if (command[0] != '\0') {
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
                    int pipefd[2];
                    pid_t cpid;
                    char *exec_argv[ARGSLIMIT] = {}; /* execute arguments */
                    if (pipe(pipefd) == -1) {
                        perror("pipe");
                        exit(EXIT_FAILURE);
                    }
                    switch (cpid = fork()) {
                    case -1:
                        perror("fork");
                        exit(EXIT_FAILURE);
                        break;
                    case 0: /* child */
                        exec_argv[0] = cmd_token;
                        for (int j = 1;
                             (cmd_token = strtok_r(cmd_rest, " ", &cmd_rest)) != NULL; j++) {
                            exec_argv[j] = cmd_token;
                        }
                        if (execvp(exec_argv[0], exec_argv) == -1) {
                            char unknown_cmd[100] = "Unknown command: [";
                            strncat(unknown_cmd, exec_argv[0], strlen(exec_argv[0]));
                            strcat(unknown_cmd, "].\n");
                            write(STDERR_FILENO, unknown_cmd, strlen(unknown_cmd));
                        }
                        _exit(EXIT_SUCCESS);
                        break;
                    default: /* parent */
                        // printf("parent %d\n", getpid());
                        wait(NULL);
                        break;
                    }
                }
            }

            // if (tmp == EOF) {
            //     printf("EOF\n");
            // }
            fflush(stdout);
            // printf("--------A command line---------\n");
        }
    }
    return 0;
}