#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#include "xtux.h"
#include "version.h"

char *cliplevel_name[NUM_CLIPLEVELS] = {
    "NOCLIP",
    "AICLIP",
    "GROUNDCLIP",
    "ALLCLIP",
};


static int read_header(FILE *file, map_t *map);
static int read_map_data(FILE *file, map_t *map, int layers);
static int read_layer(FILE *file, map_t *map, maplayer_t layer);
static int read_text_line(char *line, maptext_t **text_root);
static int set_clipmap(char *tileset_filename, map_t *map);

/* Returns a pointer to a map structure */
map_t *map_load(char *filename, int layers, int map_type)
{
    FILE *file;
    map_t *map;

    /* Open the map file */
    if( !(file = open_data_file("maps", filename)) ) {
	printf("%s: Couldn't open \"%s\"\n", __FILE__, filename);
	return NULL;
    }

    /* Allocate space for the map structure */
    if( (map = (map_t *)malloc( sizeof(map_t) )) == NULL ) {
	perror("Malloc");
	return NULL;
    } else {
	map->base = NULL;
	map->object = NULL;
	map->toplevel = NULL;
	map->event = NULL;
	map->text_root = NULL;
    }

    map->map_type = map_type;

    /* Read Header */
    if( read_header(file, map) < 0 ) {
	fprintf(stderr, "%s: Error reading map header file\n", __FILE__);
	fclose(file);
	map_close(&map);
	return NULL;
    }

    if( read_map_data(file, map, layers) < 0 ) {
	printf("%s: Error reading map data for %s\n", __FILE__, filename);
	fclose(file);
	map_close(&map);
	return NULL;
    }

    fclose(file);

    /* Change each entry in map->object to the cliplevel (read from tileset) */
    if( map_type == CLIPMAP )
	if( set_clipmap(map->tileset, map) < 0 ) {
	    printf("%s: Error setting clipmap\n", __FILE__);
	    map_close(&map);
	    return NULL;
	}

    /*
      printf("MAP: \"%s\"(%d,%d) using tileset \"%s\" loaded.\n", map->name,
      map->width, map->height, map->tileset);
    */
    map->dirty = 1;
    return map;

}


void map_close(map_t **map)
{
    maptext_t *ptr, *next;

    if( *map == NULL ) {
	return;
    }

    if( (*map)->base )
	free( (*map)->base );

    if( (*map)->object )
	free( (*map)->object );

    if( (*map)->toplevel )
	free( (*map)->toplevel );

    if( (*map)->event )
	free( (*map)->event );

    ptr = (*map)->text_root;
    while( ptr != NULL ) {
	next = ptr->next;
	free( ptr->string );
	free( ptr );
	ptr = next;
    }

    free( *map );
    *map = NULL; /* In case we try to free it again */

}


#define HEADER_LINES 4
static int read_header(FILE *file, map_t *map)
{
    int l;
    char line[MAXLINE];

    for( l = 1 ; l <= HEADER_LINES ; l++ ) {
	fgets(line, MAXLINE, file); /* Read in a line */
	CHOP(line);
	if( line[0] == '#' ) {
	    l--;
	    continue; /* Skip comment */
	}
	
	/* Handle line */
	if( l == 1 ) { /* Map name */
	    strncpy( map->name, line, 32 );
	} else if( l == 2 ) { /* Map dimensions */
	    if( sscanf(line, "%d %d", &map->width, &map->height) != 2 ) {
		fprintf(stderr, "%s: Error reading width and height\n",
			__FILE__);
		return -1;
	    }
	} else if( l == 3 ) { /* What tileset to use. */
	    strncpy( map->tileset, line, 32 );
	} else if( l == 4 ) {
	    strncpy( map->author, line, 32 );
	    break;
	} else
	    return -1;

    }
    
    return l;
    
}


static char *layer_name[MAP_LAYERS] = {
    "BASE",
    "OBJECT",
    "TOPLEVEL",
    "EVENT",
    "TEXT",
    "EVENTMETA",
    "ENTITY"
};


static int read_map_data(FILE *file, map_t *map, int layers)
{
    char line[MAXLINE];
    int i, read, done;
    maplayer_t layer;
    int text = 0;

    if( layers == 0 ) {
	printf("Error, no layers specified.\n");
	return -1;
    }

    /* Bitmasks, attempt map levels set in read, one's that were successful
       are stored in done */
    read = layers;
    done = 0;
    layer = -1;

    while( !feof(file) ) {
	if( fgets( line, MAXLINE, file ) == NULL ) {
	    continue;
	}

	CHOP( line );
	for( i=0 ; i<MAP_LAYERS ; i++ ) {
	    if( !strcasecmp(line, layer_name[i])) {
		layer = i;
	    }
	}

	if( layer >= 0 && read >= 0 ) {
	    if( read & (1<<layer) ) {
		if( layer == TEXT ) {
		    if( read_text_line(line, &map->text_root) > 0 ) {
			done |= 1<<layer; /* At least one line ok */
			text++;
		    }
		} else {
		    if( read_layer(file, map, layer) > 0 ) {
			done |= 1<<layer; /* Read layer ok */
		    } else {
			read &= ~(1<<layer); /* skip from now on */
			printf("Error reading layer %s\n", layer_name[layer]);
		    }
		    layer = -1; /* Finished with this layer */
		}
	    }
	}

    }

    if( layers != done ) {
	/* There MUST be OBJECT and BASE levels */
	if( !(done & L_BASE) || !(done & L_OBJECT) ) {
	    printf("BASE or OBJECT not READ!\n");
	    return -1;
	}

	/* Some optional but requested levels are not found, no big deal */
#ifdef DEBUG
	if( (read & L_TOPLEVEL) && !(done & L_TOPLEVEL) )
	    printf("TOPLEVEL section is empty....\n");
	if( (read & L_EVENT) && !(done & L_EVENT) )
	    printf("EVENT section is empty...\n");
	if( (read & L_TEXT) && !(done & L_TEXT) )
	    printf("TEXT section is empty...\n");
#endif
    } 

    return 1;

}


static int read_layer(FILE *file, map_t *map, maplayer_t layer)
{
    char line[MAXLINE];
    char *ptr;
    int i, x, y;

    /* Allocate enough room for data */
    if( (ptr = (char *)malloc(map->width * map->height)) == NULL ) {
	perror("malloc");
	return -1;
    }

    i = 0;
    for( y = 0 ; y < map->height ; y++ ) {
	if( feof(file) ) {
	    printf("%s: Unexpectedly hit EOF\n",__FILE__);
	    free(ptr);
	    return -1;
	}
	/* Read in a line */
	if( fgets(line, MAXLINE, file) == NULL )
	    continue;

	if( strlen(line) < map->width ) {
	    printf("%s: Error with line %d, layer %s = (\"%s\")\n",
		   __FILE__, y, layer_name[layer], line);
	    printf("line length = %d, expected %d\n", strlen(line),map->width);
	    free(ptr);
	    return -1;
	}

	/* Copy characters in line to ptr */
	for( x = 0 ; x < map->width ; x++, i++ ) {
	    if( layer != BASE ) {
		if( line[x] == ' ' || line[x] == '-' ) {
		    ptr[i] = 0; /* No object */
		    continue;
		}
	    }
	    ptr[i] = line[x];
	}
    }

    if( i != map->width * map->height ) {
	printf("ERROR, didn't read the correct amount of data for layer %s\n",
	       layer_name[layer]);
	free(ptr);
	return -1;
    }


    /* make map->(layer) point to the right spot */
    if( layer == BASE )
	map->base = ptr;
    else if( layer == OBJECT )
	map->object = ptr;
    else if( layer == TOPLEVEL )
	map->toplevel = ptr;
    else if( layer == EVENT )
	map->event = ptr;

    return 1;
    
}


static maptext_t *maptext_alloc(void)
{
    maptext_t *mt;

    if( (mt = (maptext_t *)malloc( sizeof(maptext_t) )) == NULL )
	perror("Malloc");

    return mt;

}


static int read_text_line(char *line, maptext_t **text_root)
{
    int x, y, font, spaces;
    char color[16];
    maptext_t *mt, *end;

    if( *line == '\0' || *line == '#' )
	return 0;

    if( sscanf(line, "%d %d %s %d", &x, &y, color, &font) != 4 )
	return -1;

    spaces = 0;
    while( spaces < 4 ) {
	if( *line == '\0' )
	    return -1;
	else if( *line == ' ' )
	    spaces++;
	line++;
    }


    /* Find end of list */
    end = *text_root;
    if( end ) {
	while( end->next )
	    end = end->next;
    }

    if( (mt = maptext_alloc()) == NULL ) {
	printf("Error allocating text line \"%s\"\n", line);
	return -1;
    }

    mt->next = NULL;
    mt->x = x;
    mt->y = y;
    mt->font = font;
    strncpy(mt->color, color, 16);
    mt->string = strdup(line);

    /* Put mt on the end of the list */
    if( end )
	end->next = mt;
    else
	*text_root = mt;

    return 1;

}


static int set_clipmap(char *tileset_filename, map_t *map)
{
    FILE *file;
    char buf[80], name[80], cliplevel[80];
    char clipmap[256][2];
    int i, l;
    maplayer_t layer;
    char clip, o_clip, b_clip, c;

    /* We're in the "data" directory */
    if( !(file = open_data_file(NULL, tileset_filename)) ) {
	printf("%s: Using default tileset.\n", __FILE__);
	if( !(file = open_data_file(NULL, DEFAULT_TILESET)) ) {
	    printf("Can't open default tileset either!\n");
	    return -1;
	} else {
	    strncpy( map->tileset, DEFAULT_TILESET, 32 );
	}
    }

    layer = BASE;

    /* Read till EOF */
    for( l = 1 ; !feof( file ) ; l++ ) {
	if( fgets(buf, 80, file) == NULL )
	    break; /* Read in a line */

	if( buf[0] == ' ' || buf[0] == '\n' )
	    continue; /* Skip line */

	/* Base or Object */
	if( strncasecmp("BASE", buf, 4) == 0 ) {
	    layer = BASE;
	} else if( strncasecmp("OBJECT", buf, 6) == 0 ) {
	    layer = OBJECT;
        } else if( sscanf(buf, "%c %s %s", &c, name, cliplevel) != 3 ) {
	    fprintf(stderr, "%s: Error reading line %d of tileset \"%s\"\n",
		    __FILE__, l, tileset_filename);
	} else {

	    clip = NOCLIP; /* Default */
	    for( i=0 ; i<NUM_CLIPLEVELS ; i++ ) {
		if( !strcasecmp(cliplevel_name[i], cliplevel) )
		    clip = i;
	    }

	    clipmap[ (int)c ][ layer ] =  clip;

	}
    }

    fclose(file);

    /* printf("Creating clipmask...\n"); */
    for( i=0 ; i < map->width * map->height ; i++ ) {
	b_clip = clipmap[ (int)map->base[i] ][BASE];
	/* If map->object[i] == 0, then it's an empty tile (' ' or '-') */
	if( map->object[i] )
	    o_clip = clipmap[ (int)map->object[i] ][OBJECT];
	else
	    o_clip = NOCLIP;
	/* set clipmap to be the LEAST RESTRICTIVE of the two */
	map->base[i] = b_clip > o_clip? b_clip : o_clip;
    }

    /* Don't need it anymore (only base is used for clipping in server) */
    free( map->object );
    map->object = NULL;

    return l; /* Total number of lines read */

}


static string_node *mapfile_new(string_node *previous)
{
    string_node *mf;
    
    if( (mf = (string_node *)malloc( sizeof(string_node) )) == NULL)
	perror("Malloc");
    else {
	mf->string = NULL;
	mf->next = NULL;
	mf->prev = previous;
	if( previous != NULL )
	    previous->next = mf;
    }

    return mf;

}


string_node *map_make_filename_list(void)
{
    DIR *dp;
    struct dirent *current;
    string_node *mf, *prev;

    if( (dp = open_data_dir("maps")) == NULL) {
        printf("Error creating map list!\n");
	return NULL;
    }

    mf = NULL;

    while( (current = readdir(dp)) != NULL ) {
	if( !strcmp(current->d_name+(strlen(current->d_name) - 4), ".map") ) {
	    prev = mf;
	    if( (mf = mapfile_new(prev)) == NULL ) {
		printf("Error allocating filename node for %s\n",
		       current->d_name);
		continue;
	    }
	    mf->string = strdup(current->d_name);
	}
    }

    closedir(dp);

    /* set mf to beginning of list */
    while( mf->prev )
	mf = mf->prev;

    return mf;

}
