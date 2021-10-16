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

/* Execution of commands */
void exec_cmd() {}

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
        write(STDOUT_FILENO, "\% ", strlen("\% "));
        if (getline(&read_buf, &read_len, stdin) < 0) {
            break;
        }
        // char read_buf[15100] = "";  // read buffer
        // char tmp;
        /* read a line */
        // while (read(STDIN_FILENO, &tmp, 1) > 0) {
        //     read_buf[read_len++] = tmp;
        //     if (tmp == '\n') {
        //         break;
        //     }
        // }

        // printf("read: %s", read_buf);

        /* extract a command*/
        // size_t index = 0;  // index for read_buf
        // for (size_t isEnd = false, i = 0; index < read_len && !isPipe &&
        // !isEnd;
        //      index++, i++) {
        //     switch (read_buf[index]) {
        //     case '|':
        //         isPipe = true;
        //         break;
        //     case '\n':
        //         // printf("read_len: %ld, %ld\n", read_len, index);
        //         isEnd = true;
        //         break;
        //     default:
        //         command[i] = read_buf[index];
        //         break;
        //     }
        // }
        char *read_token; /* record readline token for split */
        char *read_rest = read_buf;
        /* loop for each command*/
        while (read_rest[0] != '\0') {
            char command[266] = "";
            bool isPipe = false;
            bool isFileRedirect = false;
            bool isNumPipe = false;
            size_t numPipeLen = 0;

            /* extract a command*/
            while ((read_token = strtok_r(read_rest, " \n", &read_rest)) !=
                   NULL) {
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
                } else if (strncmp(cmd_token, "printenv", strlen("printenv")) ==
                           0) {
                    struct built_in_arg arg = {"", ""};
                    cmd_token = strtok_r(cmd_rest, " ", &cmd_rest);
                    strncpy(arg.name, cmd_token, strlen(cmd_token));
                    built_in('p', arg);
                } else if (strncmp(cmd_token, "exit", strlen("exit")) == 0) {
                    struct built_in_arg arg = {"", ""};
                    built_in('e', arg);
                } /* execute command */
                else {
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