#define MAX_LENGTH 1024

// This struct is used to create an array of key value pairs.
typedef struct formdata {
	char *name;
	char *value;

} Fdata;

/* Functions used by CGI programs
 */

Fdata *parse_query(char *str);
char *fdata2html(Fdata *f);
void fdata_free(Fdata *f);

