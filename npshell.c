#include "npshell.h"

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
        char command[266] = "";
        size_t index = 0;
        bool isPipe = false;
        for (size_t isEnd = false, i = 0; index < read_len && !isPipe && !isEnd;
             index++, i++) {
            switch (read_buf[index]) {
            case '|':
                isPipe = true;
                break;
            case '\n':
                // printf("read_len: %ld, %ld\n", read_len, index);
                isEnd = true;
                break;
            default:
                command[i] = read_buf[index];
                break;
            }
        }

        /* check and execute a command*/
        // printf("command: %s\n", command);
        char *cmd_token = 0;
        if (command[0] != '\0') {
            // printf("command: %s\n", command);
            cmd_token = strtok(command, " ");

            if (strncmp(cmd_token, "setenv", strlen("setenv")) == 0) {
                struct built_in_arg arg = {"", ""};
                cmd_token = strtok(NULL, " ");
                strncpy(arg.name, cmd_token, strlen(cmd_token));
                cmd_token = strtok(NULL, " ");
                strncpy(arg.value, cmd_token, strlen(cmd_token));
                built_in('s', arg);
            } else if (strncmp(cmd_token, "printenv", strlen("printenv")) ==
                       0) {
                struct built_in_arg arg = {"", ""};
                cmd_token = strtok(NULL, " ");
                strncpy(arg.name, cmd_token, strlen(cmd_token));
                built_in('p', arg);
            } else if (strncmp(cmd_token, "exit", strlen("exit")) == 0) {
                struct built_in_arg arg = {"", ""};
                built_in('e', arg);
            }
        }



        // if (tmp == EOF) {
        //     printf("EOF\n");
        // }
        fflush(stdout);
    }
    return 0;
}