/************************************************************
 * Code common between client and server programs for XTux. *
 * David Lawrence (nogoodpunk@yahoo.com)                    *
 * Start: Jan 05 2000                                       *
 ************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include "xtux.h"


/* Wrapped up in ERR_QUIT(str, status) which adds the version, file and line
   from the program in which it was called */
void err_quit(char *str, int status, char *version, char *file, int line)
{
    struct utsname sname;

    fflush(NULL); /* Flush all buffers */
    uname(&sname);
    fprintf(stderr, "************************************************\n"
	            "An error occured, please email David Lawrence \n"
	            "at nogoodpunk@yahoo.com with a copy of the error\n"
		    "messages & info about your system. Thankyou.\n"
	            "************************************************\n");
    fprintf(stderr, " VERSION: %s\n", version);

    /* Print a list of system information */
    fprintf(stderr,
	    " OS:      %9s\n"
	    " Release: %9s\n"
	    " Arch:    %9s\n",
	    sname.sysname,
	    sname.release,
	    sname.machine);

    fprintf(stderr,
	    " Error: %s (%d)\n"
	    " Exit called in %s, line: %d\n",
	    str, status,
	    file, line);

    exit(status);

} /* err_quit() */


void chomp(char *str)
{
    int i;
    char *c;

    if( *str == '\0' )
	return;

    i = strlen(str) - 1;
    c = str + i;

    if( *c == '\n' )
	*c = '\0';

}


/* Return amount of %'c' in the string, if there is another type
   of formatting in the string, return on error */
int str_format_count(char *str, char c)
{
    int i, num;

    num = 0;

    for( i=0 ; str[i] ; i++ ) {
	if( str[i] == '%' ) {
	    i++;
	    if( str[i] == '%' )
		continue;
	    else if( str[i] == c )
		num++;
	    else
		return -1;
	}
    }

    return num;

}





/* Returns the amount of times s occurs in str */
int string_count(char *str, char *s)
{
    char *ptr;
    int num, len;
    

    if( (len = strlen(s)) == 0 ) {
	printf("string_count: length of s == 0\n");
	return 0;
    }

    ptr = str;
    for( num = 0 ; (ptr = strstr(ptr, s)) != NULL ; num++ )
	ptr += len;

    return num;

}
