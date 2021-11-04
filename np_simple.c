#include "np_simple.h"
#include "npshell.h"

int main(int argc, char **argv)
{
    // char inputBuffer[256] = {};
    // char message[] = {"Hi,this is server.\n"};
    int sockfd = 0, forClientSockfd = 0;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        printf("Fail to create a socket.");
    }

    struct sockaddr_in serverInfo, clientInfo;
    socklen_t addrlen = sizeof(clientInfo);
    bzero(&serverInfo, sizeof(serverInfo));

    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(7005);
    bind(sockfd, (struct sockaddr *) &serverInfo, sizeof(serverInfo));
    listen(sockfd, 5);

    int savefd[3] = {0};

    while (1) {
        // forClientSockfd = accept(sockfd, (struct sockaddr *) &clientInfo, &addrlen);
        // send(forClientSockfd, message, sizeof(message), 0);
        switch (fork()) {
        case -1:
            perror("fork error");
            break;
        case 0: /* child */
            forClientSockfd = accept(sockfd, (struct sockaddr *) &clientInfo, &addrlen);
            printf("accept success\n");
            // send(forClientSockfd, message, sizeof(message), 0);
            for (int i = 0; i < 3; ++i) {
                savefd[i] = dup(i);
                dup2(forClientSockfd, i);
            }
            npshell();
            exit(EXIT_SUCCESS);
            break;
        default: /* parent */
            while (waitpid(-1, NULL, WNOHANG) >= 0)
                ;
            close(forClientSockfd);
            for (int i = 0; i < 3; ++i) {
                dup2(savefd[i], i);
            }
            break;
        }
    }


    return 0;
}