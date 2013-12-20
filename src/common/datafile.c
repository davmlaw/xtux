#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>

#include "xtux.h"
#include "datafile.h"

/* PATH_MAX is 4096 on my Linux system, but 1k should be enough... */
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

static char fullpathname[PATH_MAX];

char *data_file(char *filename)
{
    snprintf(fullpathname, PATH_MAX, "%s/%s", DATADIR, filename);
    return fullpathname;
}


/* Open a filename in the data directory  for reading */
FILE *open_data_file(char *dir, char *filename)
{
    FILE *fp;

    if( filename == NULL || filename[0] == '\0' )
	return NULL;

    if( dir )
	snprintf(fullpathname, PATH_MAX, "%s/%s/%s", DATADIR, dir, filename);
    else
	snprintf(fullpathname, PATH_MAX, "%s/%s", DATADIR, filename);

    if( (fp = fopen( fullpathname, "r")) == NULL ) {
	perror("fopen");
	printf("open_data_file: Error opening \"%s\"\n", fullpathname);
    }

    return fp;

}


DIR *open_data_dir(char *dirname)
{
    DIR *dp;

    snprintf(fullpathname, PATH_MAX, "%s/%s", DATADIR, dirname);
    
    if( (dp = opendir(fullpathname)) == NULL ) {
	perror("opendir");
	printf("Error opening \"%s\"\n", fullpathname);
    }

    return dp;

}


char *get_home_dir(void)
{
    struct passwd *pwd;
    char *home_dir = NULL;

    if( (home_dir = getenv("HOME")) == NULL ) {
	/* Try looking in password file for home dir */
	if ((pwd = getpwuid(getuid())))
	    home_dir = pwd->pw_dir;
	else
	    return NULL;
    }

    if( access(home_dir, R_OK | W_OK | X_OK) < 0 ) {
	perror("access");
	printf("User doesn't have RWX permissions on home directory %s\n",
	       home_dir);
	return NULL;
    }

    return home_dir;

}
