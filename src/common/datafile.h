#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

char *data_file(char *filename);
FILE *open_data_file(char *dir, char *filename);
DIR *open_data_dir(char *dirname);
char *get_home_dir(void);
