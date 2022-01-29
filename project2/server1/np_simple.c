#include "np_simple.h"
#include "npshell.h"

int main(int argc, char **argv)
{
    /* signal handlers, recycle process immediately, don't throw to .init */
    struct sigaction sa;
    sa.sa_handler = child_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);

    uint16_t port = 7001;
    if(argv[1])
        port = strtol(argv[1], NULL, 10);
    debug("port: %u\n", port);

    int sockfd = 0, csockfd = 0;
    

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Fail to create a socket.");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    struct sockaddr_in servAddr, clientAddr;
    socklen_t addrlen = sizeof(clientAddr);
    bzero(&servAddr, sizeof(servAddr));

    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(port);
    bind(sockfd, (struct sockaddr *) &servAddr, sizeof(servAddr));
    listen(sockfd, 5);

    while (1) {
        csockfd = accept(sockfd, (struct sockaddr *) &clientAddr, &addrlen);
        printf("accept success\n");
        switch (fork()) {
        case -1:
            perror("fork error");
            break;
        case 0: /* child */
            for (int i = 0; i < 3; ++i) {
                dup2(csockfd, i);
            }
            close(csockfd);
            npshell();
            printf("not exec\n");
            exit(EXIT_SUCCESS);
            break;
        default: /* parent */
            close(csockfd);
            while (waitpid(-1, NULL, WNOHANG) >= 0)
                ;
            printf("after wait\n");
            break;
        }
    }

    return 0;
}