#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

/* A CGI program that kills itself part way through to test the web server */

int main() {
    printf("Content-type: text/html\r\n\r\n");
    printf("<html><head><title>Hello World</title></head>\n");
    printf("<body>\n");
    printf("<h2>Hello, world! I'm about to die</h2>\n");
    fflush(stdout);
    
    sleep(5);
    
    kill(getpid(), SIGKILL);
    
    printf("<p> I should never get here</p>\n");
    printf("</body></html>\n");
    return 0;
}
