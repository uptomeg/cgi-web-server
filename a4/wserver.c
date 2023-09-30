#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */

#include "wrapsock.h"
#include "ws_helpers.h"

#define MAXCLIENTS 10

int handleClient(struct clientstate *cs, char *line);

// You may want to use this function for initial testing
//void write_page(int fd);

int
main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: wserver <port>\n");
        exit(1);
    }
    unsigned short port = (unsigned short)atoi(argv[1]);
    int listenfd;
    struct clientstate client[MAXCLIENTS];


    // Set up the socket to which the clients will connect
    listenfd = setupServerSocket(port);

    initClients(client, MAXCLIENTS);
    
    int connection_count = 0;
    struct timeval tv;

    int max_fd = listenfd;
    fd_set all_fds;
    int timeout_count = 0;
    fd_set listen_fds;

    while (connection_count <= MAXCLIENTS){

        // Client socket traversal
        for (int i = 0; i < MAXCLIENTS; i++){
            // we only handle active clients
            if (client[i].sock < 0){
                continue;
            }

            if (client[i].fd[0] == -1 && client[i].output == NULL){
                // this client is not ready
                FD_ZERO(&all_fds);
                FD_SET(client[i].sock, &all_fds);
                listen_fds = all_fds;
                tv.tv_sec = 300;
                tv.tv_usec = 0; // 5 minutes timeout limit
                // listening client socket
                fprintf(stderr, "select client socket for client %d\n", i);
                if (select(client[i].sock+1, &listen_fds, NULL, NULL, &tv) == -1){
                    perror("server: select");
                    for (int i = 0; i<MAXCLIENTS;i++){
                        resetClient(client+i);
                    }
                    exit(0);
                }
                char line[MAXLINE+1];
                int num_read = read(client[i].sock, line, MAXLINE);
                line[num_read] = '\0';
                int ret = handleClient(client + i, line);

                if (ret == -1){
                    fprintf(stderr, "something went wrong handling client\n");
                    // something went wrong, close the socket
                    FD_CLR(client[i].sock, &all_fds);
                    FD_CLR(client[i].fd[0], &all_fds);
                    close(client[i].sock);
                    close(client[i].fd[0]);
                    resetClient(client + i);
                }
                else if (ret == 0){
                    // reading not finished, continue
                    continue;
                }
                else if (ret == 1){
                    fprintf(stderr, "processing request\n");
                    // client ready, process the request
                    if (processRequest(client + i) == -1){
                        perror("process request");
                    }
                    fprintf(stderr, "%d\n", client[i].fd[0]);
                }
            }

            
            if (client[i].fd[0] != -1){
                // active pipe descriptor, we read from it
                // listening pipe descriptor
                fprintf(stderr, "select pipe fd\n");
                FD_ZERO(&all_fds);
                FD_SET(client[i].fd[0], &all_fds);
                listen_fds = all_fds;
                tv.tv_sec = 300;
                tv.tv_usec = 0; // 5 minutes timeout limit
                if (select(client[i].fd[0]+1, &listen_fds, NULL, NULL, &tv) <= 0){
                    perror("server: select");
                    for (int i = 0; i<MAXCLIENTS;i++){
                        FD_CLR(client[i].sock, &all_fds);
                        FD_CLR(client[i].fd[0], &all_fds);
                        close(client[i].sock);
                        close(client[i].fd[0]);
                        resetClient(client+i);
                    }
                    exit(1);
                }
                
                fprintf(stderr, "reading from pipe\n");
                int num_read = read(client[i].fd[0], client[i].optr, MAXPAGE);
                client[i].optr += num_read;
                client[i].optr[1] = '\0';
                fprintf(stderr, "%d bytes read\n", num_read);
                //fprintf(stderr, "%s\n", client[i].output);
                fprintf(stderr, "finished reading from pipe\n");

                if (num_read <= 0){
                    // read finished, write the response to the client socket
                    fprintf(stderr, "printing to client socket\n");
                    
                    int status;
                    wait(&status);
                    if (WIFEXITED(status)){
                        if (WEXITSTATUS(status) == 100){
                            printNotFound(client[i].sock);
                        }
                        else if (WEXITSTATUS(status) == 0){
                            printOK(client[i].sock, client[i].output, client[i].optr - client[i].output);
                        }
                        else {
                            printServerError(client[i].sock);
                        }
                    }
                    else if (WIFSIGNALED(status)){
                        printServerError(client[i].sock);
                    }// clean up
                    FD_CLR(client[i].sock, &all_fds);
                    FD_CLR(client[i].fd[0], &all_fds);
                    close(client[i].sock);
                    close(client[i].fd[0]);
                    resetClient(client + i);
                }
                fprintf(stderr, "finished client %d as socket %d\n", i, client[i].sock);
            }
        }
        tv.tv_sec = 1;
        tv.tv_usec = 0; // 1 second timeout
        //fprintf(stderr, "select server socket fd, timeout: %d\n", timeout_count);
        FD_ZERO(&all_fds);
        FD_SET(listenfd, &all_fds);
        listen_fds = all_fds;
        
        if (select(max_fd + 1, &listen_fds, NULL, NULL, &tv) < 0) {
            perror("server: select");
            for (int i = 0; i<MAXCLIENTS;i++){
                FD_CLR(client[i].sock, &all_fds);
                FD_CLR(client[i].fd[0], &all_fds);
                close(client[i].sock);
                close(client[i].fd[0]);
                resetClient(client+i);
            }
            exit(1);
        }
        else {
            if (FD_ISSET(listenfd, &listen_fds)) {
                timeout_count = 0;
                // Is it the original socket? Create a new connection ...
                int new_client_fd = accept(listenfd, NULL, NULL);
                if (new_client_fd < 0) {
                    perror("server: accept");
                    for (int i = 0; i<MAXCLIENTS;i++){
                        FD_CLR(client[i].sock, &all_fds);
                        FD_CLR(client[i].fd[0], &all_fds);
                        close(client[i].sock);
                        close(client[i].fd[0]);
                        resetClient(client+i);
                    }
                    exit(1);
                }
                
                client[connection_count].sock = new_client_fd;
                connection_count++;
                if (new_client_fd > max_fd) {
                    max_fd = new_client_fd;
                }
                FD_SET(new_client_fd, &all_fds);
                printf("Accepted connection\n");
            }
            else {
                timeout_count++;
                    if (timeout_count >= 300){
                    perror("server: timeout");
                    for (int i = 0; i<MAXCLIENTS;i++){
                        FD_CLR(client[i].sock, &all_fds);
                        FD_CLR(client[i].fd[0], &all_fds);
                        close(client[i].sock);
                        close(client[i].fd[0]);
                        resetClient(client+i);
                    }
                    exit(1);
                }
            }
        }

        
    }
    // final clean up
    for (int i = 0; i < MAXCLIENTS; i++){
        if (client[i].sock != -1){
            FD_CLR(client[i].sock, &all_fds);
            FD_CLR(client[i].fd[0], &all_fds);
            close(client[i].sock);
            close(client[i].fd[0]);
            resetClient(client+i);
        }
    }
    return 0;
}

/* Update the client state cs with the request input in line.
 * Intializes cs->request if this is the first read call from the socket.
 * Note that line must be null-terminated string.
 *
 * Return 0 if the get request message is not complete and we need to wait for
 *     more data
 * Return -1 if there is an error and the socket should be closed
 *     - Request is not a GET request
 *     - The first line of the GET request is poorly formatted (getPath, getQuery)
 * 
 * Return 1 if the get request message is complete and ready for processing
 *     cs->request will hold the complete request
 *     cs->path will hold the executable path for the CGI program
 *     cs->query will hold the query string
 *     cs->output will be allocated to hold the output of the CGI program
 *     cs->optr will point to the beginning of cs->output
 */
int handleClient(struct clientstate *cs, char *line) {


    if (cs->request == NULL){
        cs->request = malloc(1024 * sizeof(char));
        strcpy(cs->request, line);
    }
    else {
        strcat(cs->request, line);
    }

    int finished = 0;
    for (int i = 0; i < strlen(cs->request); i++){
        if (cs->request[i] == '\r' && cs->request[i+1] == '\n' && cs->request[i+2] == '\r' && cs->request[i+3] == '\n'){
            finished = 1;
            cs->request[i] = '\0';
            break;
        }
    }
    //fprintf(stderr, "%s\n", cs->request);
    if (finished == 0){
        return 0;
    }
    fprintf(stderr, "%s\n", cs->request);

    cs->path = malloc(MAXLINE * sizeof(char));
    char* path = getPath(cs->request);
    cs->query_string = malloc(MAXLINE * sizeof(char));
    char* query = getQuery(cs->request);
    if (path != NULL && query != NULL){
        strcpy(cs->path, path);
        strcpy(cs->query_string, query);
        cs->output = malloc(MAXPAGE * sizeof(char));
        strcpy(cs->output, "");
        cs->optr = cs->output;
    }
    else {
        return -1;
    }

    // If the resource is favicon.ico we will ignore the request
    if(strcmp("favicon.ico", cs->path) == 0){
        // A suggestion for debugging output
        fprintf(stderr, "Client: sock = %d\n", cs->sock);
        fprintf(stderr, "        path = %s (ignoring)\n", cs->path);
		printNotFound(cs->sock);
        return -1;
    }


    // A suggestion for printing some information about each client. 
    // You are welcome to modify or remove these print statements
    fprintf(stderr, "Client: sock = %d\n", cs->sock);
    fprintf(stderr, "        path = %s\n", cs->path);
    fprintf(stderr, "        query_string = %s\n", cs->query_string);

    return 1;
}

