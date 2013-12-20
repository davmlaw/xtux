#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "xtux.h"
#include "client.h"
#include "win.h"
#include "image.h"

#define HASHSIZE 101 /* Sizeof hash table */

extern win_t win;
extern client_t client;
extern map_t *map;
static image_t *hashtab[HASHSIZE]; /* Hash table of image nodes */

static int load_xpm(char *, Pixmap *, Pixmap *,XpmAttributes *);
static image_t *lookup(char *s);
static image_t *install(char *name, int has_mask);
static void free_pixmaps(image_t *img);


/* If image is loaded, return it, otherwise load it or if that fails, load (if
   it isn't already) & return the "image not found" pixmap. This is the
   only interface to pixmap loading used in the rest of XTux */
image_t *image(char *name, int has_mask)
{
    image_t *img;
    char *notfound = "image_not_found.xpm";

    /* If image is not loaded, or it doesn't have a mask and one is requested,
       load it */
    if( (img = lookup(name)) == NULL || (has_mask != img->has_mask ) )
	if( (img = install(name, has_mask) ) == NULL ) {
	    if( !strcmp(name, notfound) )
		ERR_QUIT("Can't load fallback image!", -1);
	    printf("%s: Could not load %s!\n", __FILE__, name);
	    img = image(notfound, MASKED);
	    return img;
	}

    return img;

}

/* (From xpm.h)
 * Return ErrorStatus codes:
 * null     if full success
 * positive if partial success
 * negative if failure
 */
static char *xpm_status[] = {
    "XpmColorFailed",
    "XpmNoMemory",
    "XpmFileInvalid",
    "XpmOpenFailed",
    "XpmSuccess",
    "XpmColorError"
};


void screenshot(void)
{
    char *home;
    char filename[PATH_MAX];
    int status;
    int i = 0;

    if( (home = get_home_dir()) == NULL ) {
	printf("Error, couldn't find home directory.\n");
	return;
    }

    while( ++i ) {
	snprintf(filename, PATH_MAX, "%s/xtux_%s_%d.xpm", home, map->name, i);

	/* Make sure file doesn't exist */
	if( access( filename, F_OK ) == -1) {
	    status = XpmWriteFileFromPixmap(win.d, filename, win.w, 0, NULL);
	    if( status == 0 )
		printf("Wrote \"%s\"\n", filename);
	    else
		printf("ERROR WRITING \"%s\"! Error = %s(%d)\n",
		       filename, xpm_status[status + 4], status);
	    break;
	}
    }

}


int image_close(void)
{
    struct imgptr *np, *next;
    int images = 0;
    int i;
    
    for( i=0 ; i<HASHSIZE ; i++ ) {
	for( np = hashtab[i] ; np != NULL ; np = next ) {
	    next = np->next;
	    images++;
	    /* Delete the node */
	    free_pixmaps(np); /* Free the pixmaps in the node */
	    free(np->name); /* filename was strdup'd */
	    free(np);
	}
    }

    printf("Free'd %d images\n", images);
    return images;
    
}

static char filename[PATH_MAX];

static int load_xpm(char *xpm_name, Pixmap *pixmap, Pixmap *mask,
		    XpmAttributes *attr)
{

    int status, i = 0;

#define NUMPATHS 6
    static char *xpm_search_path[NUMPATHS] = {
	"/",
	"tiles/",
	"entities/",
	"weapons/",
	"items/",
	"events/"
    };

    attr->valuemask = XpmColormap | XpmCloseness;
    attr->colormap = win.cmap;
    attr->closeness = 1<<16;

    do {
	snprintf(filename, PATH_MAX, "%s/%s/%s%s",
		 DATADIR, "images", xpm_search_path[i], xpm_name);
	status = XpmReadFileToPixmap(win.d, win.w,filename, pixmap, mask,attr);
	
	if( status ) {
	    if( status == XpmOpenFailed ) {
		if( ++i < NUMPATHS )
		    continue; /* Try another path */
	    }
	    
	    printf("%s: xpm \"%s\" failed to load. Status=%s\n",
		   __FILE__, xpm_name, xpm_status[status+4]);
	    return status;
	}
    } while( status != XpmSuccess );

    /*
      if( client.debug )
      printf("%s: %s loaded WITH%s mask.\n",
      __FILE__, xpm_name, mask? "" : "OUT");
    */

    return status;
    
}



/**** Hash table operators *****/
static unsigned hash(char *s)
{
    unsigned hashval;

    for( hashval=0 ; *s != '\0' ; s++ )
	hashval = *s + 31 * hashval;

    return hashval % HASHSIZE;

}

static image_t *lookup(char *s)
{

    image_t *np;

    for( np = hashtab[hash(s)] ; np != NULL ; np = np->next )
	if( strcmp(s, np->name) == 0 )
	    return np;

    return NULL; /* Not found */

}


static image_t *install(char *name, int has_mask)
{
    XpmAttributes attr;
    image_t *np;
    unsigned hashval;
    Pixmap pixmap, mask;

    if( (np = lookup(name)) ) {
	if( np->has_mask == 0 && has_mask ) { /* Need the mask */
	    if( load_xpm(name, &pixmap, &np->mask, &attr) != XpmSuccess )
		return NULL;
	    np->has_mask = has_mask;
	    /* printf("Loaded mask for \"%s\"\n", name); */
	    XFreePixmap(win.d, pixmap); /* Already have a copy */ 
	}
    } else {
	if( load_xpm(name, &pixmap, &mask, &attr) != XpmSuccess )
	    return NULL;
        if( (np = (image_t *)malloc(sizeof(*np))) == NULL ) {
	    perror("malloc");
	    return NULL;
	}
	np->name = (char *)strdup(name);
	hashval = hash(name);
	np->next = hashtab[hashval];
	hashtab[hashval] = np;
	np->pixmap = pixmap;
	np->mask = mask;
	np->has_mask = has_mask;
    }

    /*
      if( has_mask && mask == None )
      printf("Tried, but no mask for \"%s\"\n", name);
    */

    return np;

}


static void free_pixmaps(image_t *img)
{

    if( img->pixmap )
        XFreePixmap(win.d, img->pixmap);
    if( img->mask )
        XFreePixmap(win.d, img->mask);

}
