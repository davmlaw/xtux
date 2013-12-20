#include <stdio.h>
#include <string.h>

#include "xtux.h"
#include "client.h"
#include "win.h"
#include "input.h"
#include "image.h"
#include "draw.h"
#include "../common/ammo.h"

extern win_t win;
extern client_t client;

extern float sin_lookup[DEGREES];
extern float cos_lookup[DEGREES];
extern char textentry_input_buffer[TEXTMESSAGE_STRLEN];


void draw_load_screen(loadscreen_t type)
{
    image_t *blueprint;
    char buf[64];

    if( type == BLUEPRINT ) {
	clear_area(win.buf, 0, 0, client.view_w, client.view_h, "blue");
	blueprint = image("blueprint.xpm", NOMASK);
	blit(blueprint->pixmap, win.buf, 0, 0, client.view_w, client.view_h);
    } else if( type == MINIMAP ) {
	draw_mini_map(win.buf, client.map, 0, 0, client.view_w, client.view_h);
    }

    snprintf(buf, 64, "Loading %s...", client.map);
    win_center_print(win.buf, buf, client.view_h/2, 2, "white");
    win_update();

}


void draw_mini_map(Pixmap dest, char *filename, int dest_x, int dest_y,
		   int width, int height)
{
    char *clipcolor[NUM_CLIPLEVELS] = {
	"dark blue",
	"dark blue",
	"blue",
	"white",
    };
    char str[16];
    map_t *minimap;
    int i, x, y;
    int sq, use_pixels;
    int fw, fh; /* font width/height */

    if( (minimap = map_load(filename, L_BASE | L_OBJECT, CLIPMAP)) == NULL ) {
	printf("%s: ** ERROR LOADING CLIPMAP %s! **\n", __FILE__, client.map);
	client.state = MENU;
	return;
    } else {
	/* Set sq to be the smallest of the two dimensions */
	sq = MIN( width / minimap->width, height / minimap->height );

	/* Center image on destination */
	dest_x += (width - sq * minimap->width)/2;
	dest_y += (height - sq * minimap->height)/2;

	/* Minimum size of 1x1 pixel */
	if( sq > 1 )
	    use_pixels = 0;
	else {
	    use_pixels = 1;
	    sq = 1;
	}

	/* Assume everything is initially "NOCLIP" (most common case) */
	clear_area(win.buf, dest_x, dest_y, sq * minimap->width,
		   sq * minimap->height, clipcolor[0]);
	
	for( y=0 ; y < minimap->height ; y++ )
	    for( x=0 ; x < minimap->width ; x++ ) {
		i = minimap->base[ y*minimap->width + x ];
		if( i > 0 && i < NUM_CLIPLEVELS ) {
		    if( use_pixels )
			draw_pixel(win.buf, x+dest_x, y+dest_y, clipcolor[i]);
		    else
			clear_area(win.buf, x*sq + dest_x, y*sq + dest_y,
				   sq, sq, clipcolor[i]);
		}
	    }

	snprintf(str, 16, "(%d,%d)", minimap->width, minimap->height);
	/* Set FW to be the widest of the three lines */
	x = text_width(str, 2);
	y = text_width(minimap->name, 2);
	fw = x>y? x : y;
	x = text_width(minimap->author, 2);
	if( x > fw )
	    fw = x;
	fh = font_height(2);
	/* A bit of a border */
	fw += 4;
	fh += 4;

	clear_area(win.buf, (client.view_w-fw)/2, 0, fw, 3*fh, "black");
	win_center_print(win.buf, minimap->name, 0, 2, "white");
	win_center_print(win.buf, str, fh, 2, "white");
	win_center_print(win.buf, minimap->author, 2*fh, 2, "white");	

	map_close(&minimap);
    }



}


/* This keeps local copies of the pixmap and masks, so we don't have to
   search the hash-table for every tile in the map */
struct {
  Pixmap b_table[256];     /* Base Table */
  Pixmap o_table[256][2];  /* Object Table (& masks) */
} static ts;

/* Reads in table tname and stores it in table */
int tileset_load(char *tname)
{
    image_t *img;
    FILE *file;
    char buf[80];
    char name[80];
    int layer, masking;
    int l;
    char c;

    if( client.debug ) {
	printf("********** Loading tileset %s *********\n", tname);
    }

    /* Wipe out old tables */
    memset(ts.b_table, -1, sizeof(ts.b_table));
    memset(ts.o_table, -1, sizeof(ts.o_table));

    if( !(file = open_data_file(NULL, tname)) ) {
	printf("%s: Using default tileset.\n", __FILE__);
	if( !(file = open_data_file(NULL, DEFAULT_TILESET)) ) {
	    return 0;
	}
    }

    layer = BASE;
    masking = 0;

    /* Read till EOF */
    for( l = 1 ; !feof( file ) ; l++ ) {
	if( fgets(buf, 80, file) == NULL )
	    break; /* Read in a line */

	if( buf[0] == ' ' || buf[0] == '\n' )
	    continue; /* Skip line */

	/* Base or Object */
	if( strncasecmp("BASE", buf, 4) == 0 ) {
	    if( client.debug )
		printf("%s: Reading Base tileset table\n", __FILE__);
	    layer = BASE;
	    masking = NOMASK;
	} else if( strncasecmp("OBJECT", buf, 6) == 0 ) {
	    if( client.debug )
		printf("%s: Reading Object tileset table\n", __FILE__);
	    layer = OBJECT;
	    masking = MASKED;
        } else if( sscanf(buf, "%c %s", &c, name) != 2 ) {
	    fprintf(stderr, "%s: Error reading tileset table line %d\n",
		    __FILE__, l);
	} else {
	    img = image(name, masking);

	    if( layer == BASE )
		ts.b_table[(int)c] = img->pixmap;
	    else if( layer == OBJECT ) {
		ts.o_table[(int)c][0] = img->pixmap;
		if( masking )
		    ts.o_table[(int)c][1] = img->mask;
	    }
	}
    }

    fclose(file);
    return l; /* Total number of lines read */

}


extern map_t *map;


static void draw_map_text(netushort x_off, netushort y_off,
			  int width, int height)
{
    maptext_t *mt;

    for( mt = map->text_root ; mt ; mt = mt->next ) {
	/* Skip if X is out of range */
	if( mt->x + mt->width < x_off )
	    continue;
	if( mt->x >= x_off + width)
	    continue;

	/* Skip if Y is out of range */
	if( mt->y + mt->height < y_off )
	    continue;
	if( mt->y >= y_off + height)
	    continue;

	win_print(win.map_buf, mt->string,
		  mt->x - x_off, mt->y - y_off,
		  mt->font, mt->color);

    }

}


void redraw_map_buf(int x_off, int y_off)
{
    int i;
    int x, y;
    int x_dest, y_dest;
    int x_max, y_max;
    Pixmap tile, mask;

    x_max = MIN(map->width, x_off/TILE_W + client.x_tiles);
    y_max = MIN(map->height, y_off/TILE_H + client.y_tiles);
    
    /* Check to see if we can buffer 1 extra tile */
    if( x_off + client.view_w < map->width * TILE_W )
	x_max++;
    
    if( y_off + client.view_h < map->height * TILE_H )
	y_max++;

    for( y=y_off/TILE_H ; y < y_max ; y++ ) {
	for( x=x_off/TILE_W; x < x_max ; x++) {
	    i = y * map->width + x;
	    x_dest = (x - x_off/TILE_W) * TILE_W;
	    y_dest = (y - y_off/TILE_H) * TILE_H;

	    /* Draw base layer. */
	    if( (tile = ts.b_table[ (int)map->base[i] ]) == -1 ) {
		printf("No tile in base[%d] = %c\n", i, map->base[i]);
		tile = image("bad_tile.xpm", NOMASK)->pixmap;
	    }

	    blit( tile, win.map_buf, x_dest, y_dest, TILE_W, TILE_H);

	    /* Draw Object layer (if it exists) */
	    if( map->object[i] ) {
		if( (tile = ts.o_table[ (int)map->object[i] ][0]) == -1) {
		    printf("No tile in object[%d][0] = '%c'\n",
			   i, map->object[i]);
		    tile = image("bad_tile.xpm", MASKED)->pixmap;
		}

		if( (mask = ts.o_table[ (int)map->object[i] ][1]) == -1) {
		    printf("No mask in object[%d][1] = '%c'\n",
			   i, map->object[i]);
		    mask = image("bad_tile.xpm", MASKED)->mask;
		}

		if( mask ) {
		    trans_blit(tile, win.map_buf, x_dest, y_dest,
			       TILE_W, TILE_H, mask);
		} else {
		    blit(tile, win.map_buf, x_dest, y_dest,
			 TILE_W, TILE_H);
		}
	    }
	}
    }

    /* Draw map text */
    if( map->text_root ) {
	/* Set offsets to be the offset of map_buf */
	x_off = x_off/TILE_W * TILE_W;
	y_off = y_off/TILE_H * TILE_H;
	draw_map_text(x_off, y_off,
		      x_max * TILE_W, y_max * TILE_H);
    }

}


/* Draws the map onto buffer, it only calls redraw_map_buf when it needs to,
   which tends to be around ~20% of the time. Draw_map is pretty expensive
   (uses lots of small blits) */
void draw_map(shortpoint_t screenpos)
{
    /* Old screenpos values */
    static int x, y;

    /* If we are no longer on the same tile as we were last time, we can't
       just draw the offset, we have to redraw the whole map */
    if( x/TILE_W != screenpos.x/TILE_W || y/TILE_H != screenpos.y/TILE_H)
	map->dirty = 1;

    x = (int)screenpos.x;
    y = (int)screenpos.y;

    if( map->dirty ) {
	redraw_map_buf(x, y);
	map->dirty = 0;
    }

    offset_blit(win.map_buf, win.buf, 0, 0, client.view_w , client.view_h,
		x? -x%TILE_W : 0, y? -y%TILE_H : 0);
  
}


/* Draw the toplevel (which is drawn over buffer (ie everything)) the toplevel
   uses the same tileset table as the object layer */
void draw_map_toplevel(shortpoint_t screenpos)
{
    int x, y, i;
    int x_off, y_off;
    int x_max, y_max;
    int x_dest, y_dest;
    Pixmap tile, mask;

    if( map == NULL || map->toplevel == NULL ) {
	printf("draw_map_toplevel() called when no toplevel exists!\n");
    }

    x_off = screenpos.x / TILE_W;
    y_off = screenpos.y / TILE_H;


    x_max = MIN(map->width, x_off + client.x_tiles);
    y_max = MIN(map->height, y_off + client.y_tiles);

    /* Check to see if we can buffer 1 extra tile */
    if( screenpos.x + client.view_w < map->width * TILE_W )
	x_max++;
    
    if( screenpos.y + client.view_h < map->height * TILE_H )
	y_max++;

    for( y = y_off ; y < y_max ; y++ ) {
	for( x = x_off ; x < x_max ; x++ ) {
	    i = y * map->width + x;
	    if( i >= map->width * map->height ) {
		printf("draw_map_toplevel(): i (%d) >= (%d) = map wdth*hght\n",
		       i, map->width * map->height);
		return;
	    }
	    x_dest = (x - x_off) * TILE_W;
	    y_dest = (y - y_off) * TILE_H; 

	    if( map->toplevel[i] ) {
		if( (tile = ts.o_table[(int)map->toplevel[i]][0]) == -1){
		    printf("No tile in toplevel[%d][0] = %c\n",
			   i, map->toplevel[i]);
		    tile = image("bad_tile.xpm", MASKED)->pixmap;
		}

		if((mask = ts.o_table[(int)map->toplevel[i]][1]) == -1){
		    printf("No mask in toplevel[%d][1] = %c\n",
			   i, map->toplevel[i]);
		    mask = image("bad_tile.xpm", MASKED)->mask;
		}

		/* MAP_BUF offset = (screenpos.x%TILE_W, screenpos.y%TILE_H) */
		if( mask ) {
		    trans_blit(tile, win.buf,
			       x_dest - screenpos.x%TILE_W,
			       y_dest - screenpos.y%TILE_H,
			       TILE_W, TILE_H, mask);
		} else {
		    blit(tile, win.buf,
			 x_dest - screenpos.x%TILE_W,
			 y_dest - screenpos.y%TILE_H,
			 TILE_W, TILE_H);
		}
	    }
	}
    }

}


/**** Misc drawing / graphics functions ****/
int cool_clear(int width, int height, int line_w, clear_t type)
{
    int i, j;
    char *c = "black";
    
    if( line_w < 1 ) { /* Bad input */
	clear_area(win.buf, 0, 0, width, height, c);
	return 0;
    }

    for( i = width / 2 ; i >= line_w ; i /= 2 ) {
	for( j = 0 ; j <= width/2 ; j += i ) {
	    if( get_key() != XK_VoidSymbol )
		return 1; /* Return on keypress */

	    if( type == C_HORIZONTAL || type == C_BOTH ) {
		draw_line(win.buf, 0, j, width, j, line_w, c);
		draw_line(win.buf, 0, height-j, width, height-j, line_w, c);
	    }

	    if( type == C_VERTICLE || type == C_BOTH ) {
		draw_line(win.buf, j, 0, j, height, line_w, c);
		draw_line(win.buf, width - j, 0, width - j, height, line_w, c);
	    }

	    win_update();
	    delay( 5 );
	}
    }

    return 0;

}


void interlace(Pixmap dest, int width, int height, int spacing, char *col)
{
    int i;

    for( i = 0 ; i < height ; i += spacing )
	draw_line(dest, 0, i, width, i, 1, col);

}


#define BAR_WIDTH 188
#define BAR_HEIGHT 58
#define BAR_XOFF 46
#define BAR_YOFF 27
#define CAFF_BAR_W 100
#define CAFF_BAR_H 8
#define HBAR_W 188
#define HBAR_H 51


static void draw_health_bar(int health)
{
    if( health > CAFF_BAR_W )
	health = 0;

    clear_area(win.status_buf, BAR_XOFF, BAR_YOFF,
	       CAFF_BAR_W, CAFF_BAR_H, "black");
    clear_area(win.status_buf, BAR_XOFF, BAR_YOFF,
	       health, CAFF_BAR_H, "white");

    win.dirty = 1;

}


void draw_status_bar(void)
{
    image_t *img;
    weap_type_t *wt;
    ent_type_t *et;
    int text_start, fh;
    int padding;
    int hp;  /* Health Percent of type maximum */
    char str[48];

    clear_area(win.status_buf, 0, 0, win.width, STATUS_H, "black");
    win.dirty = 1;

    /* Display Health */
    if( client.status_type == HEALTHBAR ) {
	img = image("healthbar.xpm", NOMASK);
	blit(img->pixmap, win.status_buf, 0, 0, HBAR_W, HBAR_H);
	if( (et = entity_type(client.entity_type)) != NULL )
	    hp = client.health * 100 / et->health;
	else
	    hp = 0;
	draw_health_bar( hp );
	
	snprintf(str, 48, "%3d", client.health);
	padding = BAR_XOFF + 100 - text_width(str, 1);
	win_print(win.status_buf, str, padding, 34, 1, "red");
    } else {
	snprintf(str, 48, "Health %3d", client.health);
	win_print(win.status_buf, str, 0, 0, 3, "white");
    }
    
    padding = (STATUS_H - 48) / 2;
    text_start = HBAR_W + 48 + 2 * padding;
    fh = font_height(2);
    
    /* Weapon mini icon */
    if( (wt = weapon_type(client.weapon)) && wt->has_weapon_icon ) {
	img = image(wt->weapon_icon, NOMASK);
	blit(img->pixmap, win.status_buf, HBAR_W+padding, padding, 48, 48);
    }
    
    snprintf(str, 48, "Map: %s", map->name);
    win_print(win.status_buf, str, text_start, 0, 2, "white");
    
    snprintf(str, 48, "Frags: %4d", client.frags);
    win_print(win.status_buf, str, text_start, fh, 2, "white");
    
    /* Ammunition */
    if( wt->ammo_type ) {
	snprintf(str, 48, "%s: %4d", ammo_name[ wt->ammo_type ], client.ammo);
    } else
	snprintf(str, 48, "Unlimited.");
    win_print(win.status_buf, str, text_start, fh * 2, 2, "white");

    
}


void draw_textentry(void)
{
    char str[TEXTMESSAGE_STRLEN + 2];
    int rw; /* How many rows, set in win_print_column (unused) */

    clear_area(win.status_buf, 0, 0, win.width, STATUS_H, "black");
    win.dirty = 1;

    snprintf(str, TEXTMESSAGE_STRLEN + 2, "> %s", textentry_input_buffer);
    win_print_column(win.status_buf, str, 0, 0, 1, "white", win.width, 0, &rw);

}

#define TARG_W 32
#define TARG_H 32

void draw_crosshair(netmsg_entity focus)
{
    image_t *img;
    ent_type_t *et;
    point target;
    int radius;
    int x, y;
    int h, k;

    if( (et = entity_type(focus.entity_type)) == NULL )
	ERR_QUIT("Invalid entity type for focus", -1);

    /* (h,k) = Center of my entity */
    h = focus.x + et->width/2 - client.screenpos.x;
    k = focus.y + et->height/2 - client.screenpos.y;

    if( client.sniper_mode )
	radius = client.view_h/2 - TARG_H;
    else
	radius = client.crosshair_radius;

    x = sin_lookup[ focus.dir ] * radius;
    y = -cos_lookup[ focus.dir ] * radius;

    target.x = h + x - TARG_W/2;
    target.y = k + y - TARG_H/2;

    img = image("crosshair.xpm", 1);
    trans_blit(img->pixmap, win.buf, target.x, target.y,
	       TARG_W, TARG_H, img->mask);

}


void draw_map_target(netmsg_entity focus)
{
    vector_t U, V;
    int x, y;

    /* x, y = arrow position */
    x = client.screenpos.x;
    y = client.screenpos.y + 64;

    U.x = 0;
    U.y = 1;

    V.x = (client.map_target.x * TILE_W) + TILE_W/2 - x;
    V.y = -1 * ((client.map_target.y * TILE_H) + TILE_H/2 - y);

    draw_arrow(0, 64, 48, angle_between( U, V ), "yellow");

}


void draw_arrow(int x, int y, int diameter, byte dir, char *color)
{
    XPoint arrow[4];
    int j, k;
    float rad;
    byte d;

    rad = diameter / 2.0;

    /* Center of arrow */
    j = x + rad;
    k = y + rad;

    arrow[0].x = j + sin_lookup[dir] * 0.8 * rad;
    arrow[0].y = k - cos_lookup[dir] * 0.8 * rad;

    d = dir + 100;
    arrow[1].x = j + sin_lookup[d] * rad;
    arrow[1].y = k - cos_lookup[d] * rad;

    arrow[2].x = j;
    arrow[2].y = k;

    d = dir + 156;
    arrow[3].x = j + sin_lookup[d] * rad;
    arrow[3].y = k - cos_lookup[d] * rad;

    /* draw_circle(win.buf, x, y, diameter, "black"); */
    draw_filled_polygon(win.buf, arrow, 4, color);

}


void draw_netstats(vector_t NS)
{
    char buf[64];

    if( client.netstats == NS_TOTAL )
	snprintf(buf, 64, "TOTAL in:%.0f out:%.0f", NS.x, NS.y);
    else if( client.netstats == NS_RECENT )
	snprintf(buf, 64, "b/sec in:%.2f out:%.2f", NS.x, NS.y);
    else
	return;

    win_print(win.buf, buf, 0, client.view_h - font_height(2), 2, "white");

}

