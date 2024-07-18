#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <atomic_ops.h>

#define MAX 1024
#define MAX_CLIENT 5
#define bool int
#define false 0
#define true 1

bool lockter = 0;
bool disconnected = 0;
pthread_mutex_t lock;

typedef struct Client
{
    pthread_t tr;
    int socket;
    char ip[INET6_ADDRSTRLEN];
    int port;
    bool is_selected;
    bool is_alive;
    int buffer_size;
    char *buffer;
    bool is_buffer_ready;

} Client;
//-------------------------//
Client clients_arr[MAX_CLIENT] = {0};

void close_connection(Client *cli)
{
    close(cli->socket);
    cli->is_alive = false;
    if (cli->buffer_size)
    {
        free(cli->buffer);
    }
}
int concat(char **dest, char *src, int size)
{
    if (strlen(*dest) + strlen(src) >= size)
    {
        size += MAX + strlen(src);
        char *res = (char *)realloc(*dest, (size) * sizeof(char));
        strcat(res, src);
        *dest = res;
        return size;
    }
    else
    {
        strcat(*dest, src);
        return size;
    }
}

void *reader(void *_arg)
{
    Client *cli = (Client *)_arg;
    char buffer[MAX] = {0};
    while (true)
    {

        if (recv(cli->socket, buffer, MAX, 0) <= 0)
        { // error in recv
            printf("\nconnection closed by client %s:%d\n", cli->ip, cli->port);
            disconnected = 1;
            lockter = 0;
            pthread_mutex_unlock(&lock);
            // printf("press something and then enter please \n");
            close_connection(cli);
            pthread_exit(&disconnected);
        }
        else
        { // have it
            if (cli->is_selected)
            { // print it
                pthread_mutex_lock(&lock);
                lockter = 1;
                printf("%s", buffer);
                lockter = 0;
                pthread_mutex_unlock(&lock);
                memset(buffer, 0, MAX);
            }
            else
            {
                // save it
                int new_size = concat(&cli->buffer, buffer, cli->buffer_size);
                cli->is_buffer_ready = true;
                cli->buffer_size = new_size;
            }
        }
    }
}

void runCommand(char *cmd, int client_index, bool is_quiet)
{
    // if (is_quiet)
    // {
    //     int cli_select_status=clients_arr[client_index].is_selected;
    //     clients_arr[client_index].is_selected=0;
    //
    // }
    // else
    // {
    // }
    strcat(cmd, "\n");
    send(clients_arr[client_index].socket, cmd, strlen(cmd), 0);
}
void *listening(void *_args)
{
    int sockfd = *(int *)_args;
    int cc = 0;
    while (true)
    {
        struct sockaddr_storage their_addr;
        socklen_t addr_size = sizeof their_addr;
        bool find = false; // finding free space
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            if (!clients_arr[i].is_alive)
            {
                find = true;
                break;
            }
        }
        if (!find)
        {
            // we don't have free space for a new client
            sleep(1);
            continue;
        }
        int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1)
        { // there was a problem in accepting the new client
            cc++;
            printf("can't accept [%d]\n", cc);
            usleep(900000);
            continue;
        }
        else
        {
            for (int i = 0; i < MAX_CLIENT; i++)
            {
                if (!clients_arr[i].is_alive)
                { // setting default values for the client
                    find = true;
                    if (their_addr.ss_family == AF_INET)
                    { // if it's ipv4
                        struct sockaddr_in *s = (struct sockaddr_in *)&their_addr;
                        clients_arr[i].port = ntohs(s->sin_port);                                      // getting the port number
                        inet_ntop(AF_INET, &s->sin_addr, clients_arr[i].ip, sizeof clients_arr[i].ip); // getting the ip address
                    }
                    else
                    { // if it's ipv6
                        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&their_addr;
                        clients_arr[i].port = ntohs(s->sin6_port);
                        inet_ntop(AF_INET, &s->sin6_addr, clients_arr[i].ip, sizeof clients_arr[i].ip);
                    }
                    printf("\n!!!A new client came %s:%d!!!\n", clients_arr[i].ip, clients_arr[i].port);
                    clients_arr[i].socket = new_fd;
                    clients_arr[i].is_alive = true;
                    clients_arr[i].is_selected = false;
                    clients_arr[i].is_buffer_ready = false;
                    clients_arr[i].buffer = malloc(MAX * sizeof(char));
                    clients_arr[i].buffer_size = MAX;
                    memset(clients_arr[i].buffer, 0, MAX);
                    pthread_create(&clients_arr[i].tr, NULL, reader, &clients_arr[i]);
                    break;
                }
            }
        }
    }
}

void displayTerminal(int cli_ind)
{
    char outbuff[MAX] = {0};
    while (true)
    {

        if (!lockter) // if terminal is not locked
        {
            memset(outbuff, 0, MAX);
            if (!clients_arr[cli_ind].is_alive)
            { // if client has been disconnected
                pthread_join(clients_arr[cli_ind].tr, NULL);
                pthread_mutex_unlock(&lock);
                lockter = false;
                disconnected = 0;
                printf("\nClient doesn't exist. Exiting the terminal ... \n");
                break;
            }
            pthread_mutex_lock(&lock);
            if (clients_arr[cli_ind].is_buffer_ready)
            {
                // print what client was sending to us when he was not selected
                printf("%s\n", clients_arr[cli_ind].buffer);
                clients_arr[cli_ind].is_buffer_ready = false;
            }
            printf("\n%s:%d$", clients_arr[cli_ind].ip, clients_arr[cli_ind].port);
            scanf("%[^\n]%*c", outbuff);
            if (strcmp("exit", outbuff) == 0)
            { // if you want to close the connection and let it connect agian
                pthread_cancel(clients_arr[cli_ind].tr);
                pthread_join(clients_arr[cli_ind].tr, NULL);
                pthread_mutex_unlock(&lock);
                lockter = false;
                disconnected = 0;
                send(clients_arr[cli_ind].socket, "y\n", 2, 0); // connect agian (:
                close_connection(clients_arr + cli_ind);
                printf("closing the connection\n");
                break;
            }
            if (strcmp("close", outbuff) == 0)
            {
                printf("closing connection for ever!!!\n");
                pthread_cancel(clients_arr[cli_ind].tr);
                pthread_join(clients_arr[cli_ind].tr, NULL);
                pthread_mutex_unlock(&lock);
                lockter = 0;
                disconnected = 0;
                char exit[10] = "exit\n";
                send(clients_arr[cli_ind].socket, exit, strlen(exit), 0);
                char not_again[10] = "n\n";
                send(clients_arr[cli_ind].socket, not_again, strlen(not_again), 0); // don't connect agian -_-
                close_connection(clients_arr + cli_ind);
                printf("closing the connection\n");
                break;
            }
            if (strcmp("back", outbuff) == 0)
            {
                pthread_mutex_unlock(&lock);
                lockter = false;
                printf("back to main menu\n");
                clients_arr[cli_ind].is_selected = false;
                return;
            }
            if (strcmp("", outbuff) == 0)
            { // cleaning the std input
                char c;
                while ((c = getchar()) != '\n' && c != EOF)
                    ;
            }
            strcat(outbuff, "\n");
            int n = send(clients_arr[cli_ind].socket, outbuff, strlen(outbuff), 0);
            if (n == 0)
            { // means that we didn't send any
                pthread_cancel(clients_arr[cli_ind].tr);
                pthread_join(clients_arr[cli_ind].tr, NULL);
                pthread_mutex_unlock(&lock);
                lockter = 0;
                disconnected = 0;
                close_connection(clients_arr + cli_ind);
                printf(" can't send anything ... closing the connetion\n");
                break;
            }
        }
        // let the client send the output of our command
        pthread_mutex_unlock(&lock);
        usleep(250000);
        // memset(outbuff, 0, MAX);
    }
}
int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("usage : ./server [port]\n");
        return 1;
    }

    struct addrinfo hints, *res;
    int status;
    char outbuff[MAX];

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
        return 3;
    }
    printf("binding...\n");
    int b = bind(sockfd, res->ai_addr, res->ai_addrlen); // bind the socket
    if (b == -1)
    {
        printf("Can't bind to this socket. There are 2 ways change the port or wait for 30s\n");
        return 4;
    }
    int l = listen(sockfd, MAX_CLIENT); // up to MAX_CLIENT connection requests can be queued (backlog = MAX_CLIENT)
    if (l == -1)
    { // there was a problem in listening
        printf("Can't listen to socket\n");
        return 5;
    }
    printf("listening...\n");
    //--------------------
    pthread_mutex_init(&lock, NULL);
    pthread_t listener;
    printf("waiting for new connection ...\n");
    pthread_create(&listener, NULL, listening, &sockfd);
    printf("<===main menu===>\n enter help for more details or enter a server command\n");
    char cmd[20] = {0};
    char info_command[120] = {0};
    while (true)
    {
        printf("$:");
        memset(cmd, 0, sizeof(cmd));
        scanf("%s", cmd);
        if (strcmp("list", cmd) == 0)
        {
            printf("<==list==>\n");
            for (int i = 0; i < MAX_CLIENT; i++)
            {
                if (clients_arr[i].is_alive)
                {
                    printf("[%d] %s:%d\n", i + 1, clients_arr[i].ip, clients_arr[i].port);
                }
            }
        }
        else if (strcmp("list-info", cmd) == 0)
        {
            printf("<==list info==>\n");
            for (int i = 0; i < MAX_CLIENT; i++)
            {
                if (clients_arr[i].is_alive)
                {
                    clients_arr[i].is_selected = true; // to see output of the commands we make them selected
                    memset(info_command, 0, sizeof(info_command));
                    sprintf(info_command, "echo \"[%d] %s:%d => name : $(whoami) - host : $(hostname)\"",
                            i + 1, clients_arr[i].ip, clients_arr[i].port);
                    runCommand(info_command, i, 1);
                }
            }
            sleep(1); // make the clients unselected
            for (int i = 0; i < MAX_CLIENT; i++)
                if (clients_arr[i].is_alive)
                    clients_arr[i].is_selected = false;
        }
        else if (strcmp("select", cmd) == 0)
        {
            int client_ind = 0;
            scanf("%d", &client_ind);
            char c;
            while ((c = getchar()) != '\n' && c != EOF) // clean the std input;
                ;
            if (client_ind < MAX_CLIENT && client_ind > 0)
            {
                client_ind--;
                if (clients_arr[client_ind].is_alive)
                {
                    clients_arr[client_ind].is_selected = true;
                    displayTerminal(client_ind); // open the terminal
                }
                else
                {
                    printf("client doesn't extis\n");
                }
            }
            else
            {
                printf("out of range number\n");
            }
        }
        else if (strcmp("send-to-all", cmd) == 0)
        {

            char c;
            while ((c = getchar()) != '\n' && c != EOF)
                ; // clean the std input;
            printf("#:");
            scanf("%[^\n]%*c", outbuff);

            for (int i = 0; i < MAX_CLIENT; i++) // run the command for all clients
            {
                if (clients_arr[i].is_alive)
                {
                    runCommand(outbuff, i, 0);
                }
            }
            printf("done ! check each client for response");
            usleep(100000);
        }
        else if (strcmp("shutdown", cmd) == 0)
        {
            printf("shutting down ...  \n");
            pthread_mutex_unlock(&lock);
            pthread_cancel(listener); // don't listen anymore
            pthread_join(listener, NULL);
            for (int i = 0; i < MAX_CLIENT; i++) // close the connection for all clients join their threads
            {
                if (clients_arr[i].is_alive)
                {
                    pthread_cancel(clients_arr[i].tr);
                    pthread_join(clients_arr[i].tr, NULL);
                    close_connection(clients_arr + i);
                }
            }
            close(sockfd); // close server socket
            pthread_mutex_destroy(&lock);
            freeaddrinfo(res);
            printf("good bye !!!\n");
            return 0;
        }
        else if (strcmp("help", cmd) == 0)
        {
            printf("          <==  commands in the main menu  ==>\n");
            printf("list            list of clients with their ip and port\n");
            printf("list-info       list of clients with name and host name, sorted by their response time\n");
            printf("select [number] open client terminal for client with that number\n");
            printf("help            helps you\n");
            printf("send-to-all     runs a command for all clients\n");
            printf("shutdown        shuts down the server\n");
            printf("          <==  terminal commands  ==>\n");
            printf("exit            closes the connection but client will connect again\n");
            printf("close           closes the connection fully\n");
            printf("back            back to main menu with out closing the connection\n");
        }
        else
        {
            printf("wrong server command ): try help\n");
        }
        printf("---\n");
    }
    return 0;
}
