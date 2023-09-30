#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "cgi.h"

/* A simple CGI program.  It grabs the form data from the environment variable
 * QUERY_STRING and prints it as an html list
 *
 * It first prints a header line to indicate the content type.  Note that
 * header lines are terminated with crlf (carriage return, line feed)
 * characters, and there must be a blank line between the end of the
 * headers and the beginning of the body of the http message 
 */

int main() {
  char *name;

  printf("Content-type: text/html\r\n\r\n");
  printf("<html><head><title>Hello World</title></head>\n");
  printf("<body>\n");
  printf("<h2>Simple CGI</h2>\n");
  if((name = getenv("QUERY_STRING")) != NULL) {
    printf("<p>QUERY_STRING = %s</p>\n", name);
  	Fdata *f;

    // We aren't allowed to modify the string returned from getenv, 
    // so make a copy 
    char *qstr = malloc(strlen(name) + 1);
    strncpy(qstr, name, strlen(name) + 1);

    f = parse_query(qstr);
    printf("%s", fdata2html(f));
  	free(qstr);
  	fdata_free(f);
  }
  printf("</body></html>\n");
  fflush(stdout);

  return 0;
}
