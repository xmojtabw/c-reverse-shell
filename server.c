#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <atomic_ops.h>

#define MAX 1024
int lockter = 0;
int disconnected = 0;
pthread_mutex_t lock;

void *reader(void *_arg)
{
    int socket_fd = *(int *)_arg;
    char buffer[MAX] = {0};
    while (1)
    {

        if (recv(socket_fd, buffer, MAX, 0) <= 0)
        { // error in recv
            printf("\nconnection closed by client\n");
            disconnected = 1;
            lockter = 0;
            pthread_mutex_unlock(&lock);
            printf("press something and then enter please \n");
            pthread_exit(&disconnected);
        }
        else
        { // print it
            pthread_mutex_lock(&lock);
            lockter = 1;
            printf("%s", buffer);
            lockter = 0;
            pthread_mutex_unlock(&lock);
            memset(buffer, 0, MAX);
        }
    }
}

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("usage : ./server [port]\n");
        return 1;
    }
    char infocommand[70] = "echo \"client is $(whoami) and host is $(hostname)\"\n";
    struct addrinfo hints, *res;
    int status;
    char outbuff[MAX];
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof their_addr;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // to use for server(can listen to)
    if ((status = getaddrinfo(NULL, argv[1], &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }
    printf("creating socket...\n");
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
    { // there was a problem in creating socket
        printf("can't create socket\n");
    }
    printf("binding...\n");
    int b = bind(sockfd, res->ai_addr, res->ai_addrlen); // bind the socket
    if (b == -1)
    {
        printf("can't bind\n");
        return 4;
    }
    int l = listen(sockfd, 3); // up to 3 connection requets can be queued (backlog = 3)
    if (l == -1)
    { // there was a problem in listening
        printf("can't listen\n");
    }
    printf("listening...\n");
    //--------------------
    pthread_mutex_init(&lock, NULL);
    int cc = 0;
    while (1)
    {
        // wating for clients to connect
        printf("waiting for new connection ...\n");
        int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1)
        { // there was a problem in accepting the new client
            cc++;
            printf("can't accept [%d]\n", cc);
            usleep(900000);
            continue;
        }
        printf("new connection\n");
        // creating a thread for reading from socket
        pthread_t tr;
        pthread_create(&tr, NULL, reader, &new_fd);
        // getting information of new client
        char ipstr[INET6_ADDRSTRLEN];
        int port;
        if (their_addr.ss_family == AF_INET)
        { // if it's ipv4
            struct sockaddr_in *s = (struct sockaddr_in *)&their_addr;
            port = ntohs(s->sin_port);                             // getting the port number
            inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr); // getting the ip address
        }
        else
        { // if it's ipv6
            struct sockaddr_in6 *s = (struct sockaddr_in6 *)&their_addr;
            port = ntohs(s->sin6_port);
            inet_ntop(AF_INET, &s->sin6_addr, ipstr, sizeof ipstr);
        }
        // sending info command to see who is this
        send(new_fd, infocommand, strlen(infocommand), 0);
        lockter = 1;
        while (1)
        {

            if (!lockter) // if terminal is not locked
            {
                memset(outbuff, 0, MAX);
                if (disconnected)
                { // if client has been disconnected
                    pthread_join(tr, NULL);
                    pthread_mutex_unlock(&lock);
                    lockter = 0;
                    disconnected = 0;
                    close(new_fd);
                    break;
                }
                pthread_mutex_lock(&lock);
                printf("\n%s:%d$", ipstr, port);
                scanf("%[^\n]%*c", outbuff);
                if (strcmp(outbuff, "shutdown-server") == 0)
                { // if you want to shotdown your server
                    pthread_mutex_unlock(&lock);
                    pthread_cancel(tr);
                    pthread_join(tr, NULL);
                    printf("shutting down ...  \n");
                    close(new_fd);
                    close(sockfd);
                    pthread_mutex_destroy(&lock);
                    freeaddrinfo(res);
                    return 0;
                }
                if (strcmp("exit", outbuff) == 0)
                { // if you want to close the connection and connect again
                    pthread_cancel(tr);
                    pthread_join(tr, NULL);
                    pthread_mutex_unlock(&lock);
                    lockter = 0;
                    disconnected = 0;
                    close(new_fd);
                    printf("closing the connection\n");
                    break;
                }
                if (strcmp("close", outbuff) == 0)
                { // if you want to close the connection and never connect again

                    pthread_cancel(tr);
                    pthread_join(tr, NULL);
                    pthread_mutex_unlock(&lock);
                    lockter = 0;
                    disconnected = 0;
                    char exit[10] = "exit\n";
                    send(new_fd, exit, strlen(exit), 0);
                    char not_again[10] = "n\n";
                    send(new_fd, not_again, strlen(not_again), 0); // don't connect agian -_-
                    close(new_fd);
                    printf("closing the connection\n");
                    break;
                }
                if (strcmp("", outbuff) == 0)
                { // cleaning the std input
                    char c;
                    while ((c = getchar()) != '\n' && c != EOF)
                        ;
                }
                strcat(outbuff, "\n");
                int n = send(new_fd, outbuff, strlen(outbuff), 0);
                if (n == 0)
                { // means that we didn't send any
                    pthread_cancel(tr);
                    pthread_join(tr, NULL);
                    pthread_mutex_unlock(&lock);
                    lockter = 0;
                    disconnected = 0;
                    close(new_fd);
                    printf(" can't send anything ... closing the connetion\n");
                    break;
                }
            }
            // let the client send the output of our command
            pthread_mutex_unlock(&lock);
            usleep(250000);
        }
        // cleanup socket for next client
        printf("please wait 0.5s for cleanup ...\n");
        usleep(500000);
    }
    freeaddrinfo(res);
    pthread_mutex_destroy(&lock);
    return 0;
}
