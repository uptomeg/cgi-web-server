#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <getopt.h>

// The maximum length of an HTTP message line
#define MAX_LINE 256
// The maximum length of an HTTP response message
#define MAX_LENGTH 16*1024
// The size of a chunk of HTTP response to read from the pipe
#define CHUNK_SIZE 1024


void printError(char *);
void printServerError();
void printResponse(char *str);

int debug = 0;


int main(int argc, char **argv) {
    char msg[MAX_LENGTH + 1];
    int result;
    int fd[2];

    FILE *fp = stdin; // default is to read from stdin

    // Parse command line options.
    int opt;
    while((opt = getopt(argc, argv, "v")) != -1) {
        switch(opt) {
            case 'v':
                debug = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-v] [filename]\n", argv[0]);
                exit(1);
        }
    }
    if(optind < argc) {
        if((fp = fopen(argv[optind], "r")) == NULL) {
            perror("fopen");
            exit(1);
        }
    }

    char read_buffer[CHUNK_SIZE + 1];
    char line_buffer[MAX_LINE + 1];
    char resource[MAX_LINE + 1];
    char rp_resource[MAX_LINE + 1];
    char query[MAX_LINE + 1];
    char version[MAX_LINE + 1];
    int has_query;
    int r;
    int n;
    while (fgets(line_buffer, MAX_LINE, fp) != NULL){
        if (debug){
            printf("%s\n",line_buffer);
        }

        // check if there is a query
        if (strchr(line_buffer, '?') == NULL){
            has_query = 0;
        }
        else{
            has_query = 1;
        }
        // skip the lines not starting with GET
        if (strcmp(strtok(line_buffer, " "), "GET")!=0){
            continue;
        }
        // read resource and query
        if (has_query == 1){
            strcpy(resource, strtok(NULL, "?"));
            strcpy(query, strtok(NULL, " "));
        }
        else {
            strcpy(resource, strtok(NULL, " "));
            strcpy(query, "");
        }
        // read version
        strcpy(version, strtok(NULL, " "));
        // modify the resource into relative path
        strcpy(rp_resource, ".");
        strcat(rp_resource, resource);

        if (debug){
            printf("resource: %s\n",rp_resource);
            printf("query: %s\n",query);
            printf("version: %s\n",version);
        }
        
        if( access( rp_resource, F_OK ) == 0 ) {
            // resourse exists
        } else {
            printError(resource); // resourse doesn't exist
            continue;
        }

        // create child processes
        // set up pipe
        if (pipe(fd) == -1){
            printServerError();
        } // we don't want the parent process to exit, so we just give a 500 error
        r = fork();

        if (r > 0){
            // we print the message returned by the child process in parent process
            if (close(fd[1]) == -1){
                printServerError();
            }
            // wait until the child process finishes
            wait(&result);
            if (debug){
                printf("%d\n", result);
            }
            
            if (WIFEXITED(result)){
                if (WEXITSTATUS(result) == 0){
                    strcpy(msg, ""); // reset msg
                    for(int i = 0; i < MAX_LENGTH/CHUNK_SIZE; i++){
                        n = read(fd[0], read_buffer, CHUNK_SIZE);
                        if (n == CHUNK_SIZE){
                            strcat(msg, read_buffer);
                        } // add the chunk to msg
                        else if(n >= 0 && n < CHUNK_SIZE){
                            read_buffer[n] = '\0';
                            strcat(msg, read_buffer);
                            break;
                        } // msg ends with a null terminator
                        else {
                            break;
                        } // read return negative value
                    }
                    if (n < 0){
                        printServerError(); // read error, give 500 error
                    }
                    printResponse(msg);
                }
                else { // the child process' return value not 0
                    printServerError();
                }
            }

            if (WIFSIGNALED(result)){ // the child is stopped by a signal
                printServerError();
            }

            if (close(fd[0]) == -1){ // close pipe
                printServerError();
            }
        }
        else if (r == 0){
            if (dup2(fd[1], 1) == -1){
                perror("dup2");
                exit(1);
            }  // send stdout to the pipe
            // we execute the command in the child process and send the output back to the parent process
            if (close(fd[0]) == -1){
                perror("close");
                exit(1);
            }
            // set environmental variables
            if (setenv("QUERY_STRING", query, 1) == -1){
                perror("setenv");
                exit(1);
            }
            
            if (debug){
                printf("resource: %s\n",rp_resource);
            }
            if (close(fd[1]) == -1){
                perror("close");
                exit(1);
            }
            if (execl(rp_resource, rp_resource,  "", "", NULL) == -1){
                perror("execl");
                exit(1);
            }
        }
        else {
            // fork error, give 500
            printServerError();
        }
    }

    if(fp != stdin) {
        if(fclose(fp) == EOF) {
            perror("fclose");
            exit(1);
        }
    }
}


/* Print an http error page  
 * Arguments:
 *    - str is the path to the resource. It does not include the question mark
 * or the query string.
 */
void printError(char *str) {
    printf("HTTP/1.1 404 Not Found\r\n\r\n");

    printf("<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n");
    printf("<html><head>\n");
    printf("<title>404 Not Found</title>\n");
    printf("</head><body>\n");
    printf("<h1>Not Found</h1>\n");
    printf("The requested resource %s was not found on this server.\n", str);
    printf("<hr>\n</body></html>\n");
}


/* Prints an HTTP 500 error page 
 */
void printServerError() {
    printf("HTTP/1.1 500 Internal Server Error\r\n\r\n");

    printf("<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n");
    printf("<html><head>\n");
    printf("<title>500 Internal Server Error</title>\n");
    printf("</head><body>\n");
    printf("<h1>Internal Server Error</h1>\n");
    printf("The server encountered an internal error or\n");
    printf("misconfiguration and was unable to complete your request.<p>\n");
    printf("</body></html>\n");
}


/* Prints a successful response message
 * Arguments:
 *    - str is the output of the CGI program
 */
void printResponse(char *str) {
    printf("HTTP/1.1 200 OK\r\n\r\n");
    printf("%s", str);
}
