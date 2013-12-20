#include <stdio.h>
#include <stdlib.h>

#include "xtux.h"
#include "client.h"
#include "image.h"
#include "win.h"
#include "entity.h"

#define LEFT 0
#define RIGHT 1

extern win_t win;
extern client_t client;
byte num_entity_types;

/* Each frame entity net messages are stored in entity_list for sorting */
#define DEFAULT_ENTITY_LIST_LENGTH 64

static netmsg_entity *entity_list;
static int entity_list_length = 0; /* Current size of entity_list memory */
static int num_entities; /* Amount of entities in this frame */

static void draw_weapon(byte dir, byte weapon, ent_type_t *et, int x, int y);
static void draw_hand(byte hand, byte dir, int x, int y, int img_w, int img_h,
		      int shoulder_w, int shoulder_h, int side);

void entity_init(void)
{
    size_t size;

    if( (num_entity_types = entity_type_init()) <= 0 )
	ERR_QUIT("Number of entity types <= 0", 1);

    /* Size of memory to allocate for initial entity_list */
    size = DEFAULT_ENTITY_LIST_LENGTH * sizeof(netmsg_entity);
    if( (entity_list = (netmsg_entity *)malloc(size)) == NULL ) {
	perror("Malloc");
	ERR_QUIT("Error allocating entity_list", size);
    } else {
	entity_list_length = DEFAULT_ENTITY_LIST_LENGTH;
	num_entities = 0;
    }

}


/* Add a net message to the current list */
void entity_add(netmsg_entity ent)
{
    size_t new_size;

    if( num_entities >= entity_list_length ) {
	/* printf("Re-allocating entity_list\n"); */
	entity_list_length *= 2;
	new_size = entity_list_length * sizeof(netmsg_entity);
	entity_list = (netmsg_entity *)realloc(entity_list, new_size);
	if( entity_list == NULL ) {
	    perror("realloc");
	    ERR_QUIT("Error re-allocating entity_list", new_size);
	}
    }

    entity_list[ num_entities ] = ent;
    num_entities++;

}


int entity_cmp(const netmsg_entity *a, const netmsg_entity *b)
{
    animation_t *ani;
    ent_type_t *et;
    int ay, by;

    /* Lowest point of A */
    if( (et = entity_type( a->entity_type )) == NULL )
	return 0;
    ani = entity_type_animation(et, a->mode);    
    ay = a->y + ani->img_h;

    /* Lowest point of B */
    if( (et = entity_type( b->entity_type )) == NULL )
	return 0;
    ani = entity_type_animation(et, b->mode);
    by = b->y + ani->img_h;

    if( ay == by )
	return 0;
    else if( ay > by )
	return 1;
    else
	return -1;

}


void entity_draw_list(void)
{
    int i;

    if( num_entities > 1 )
	qsort((void *)entity_list, num_entities, sizeof(netmsg_entity),
	      (int (*)(const void*, const void*))entity_cmp);
    
    for( i=0 ; i<num_entities; i++ )
	entity_draw( entity_list[i] );

    num_entities = 0;

}


void entity_draw(netmsg_entity ent)
{
    animation_t *ani;
    image_t *img;
    ent_type_t *et;
    int x, y;
    int x_o, y_o;
    int hand_dir, first, second;
    byte dir;

    if( (et = entity_type( ent.entity_type )) == NULL )
	return;
    x = ent.x - client.screenpos.x;
    y = ent.y - client.screenpos.y;

    ani = entity_type_animation(et, ent.mode); /* Exits if total failure */

    /* Adjust because entites image's width & height are NOT EQUAL to their
       width & height as used in clipping on the server */
    x += (et->width - ani->img_w)/2;
    y += (et->height - ani->img_h)/2;

    /* Check if entity will be drawn on screen */
    if( x < -ani->img_w || x > client.view_w )
	return;
    if( y < -ani->img_h || y > client.view_h )
	return;

    if( ent.img >= ani->images )
	ent.img = 0;

    /* Work out which dir we should use for the entities dir (0..255) */
    dir = ent.dir + DEGREES/(ani->directions*2);
    if( ani->directions > 1 )
	dir /= DEGREES / ani->directions;
    else
	dir = 0;

    x_o = ent.img * ani->img_w;
    y_o = dir * ani->img_h;

    hand_dir = dir * (DEGREES/ani->directions);
    if( dir > ani->directions/2 ) {
	first = RIGHT;
	second = LEFT;
    } else {
	first = LEFT;
	second = RIGHT;
    }

    if( ani->draw_hand ) {
	if( dir == 0 ) {
	    draw_hand(0, hand_dir, x, y, ani->img_w, ani->img_h,
		      ani->arm_x, ani->arm_y, first);
	    draw_hand(0, hand_dir, x, y, ani->img_w, ani->img_h,
		      ani->arm_x, ani->arm_y, second);
	} else if( dir != ani->directions/2 ) {
	    draw_hand(0, hand_dir, x, y, ani->img_w, ani->img_h,
		      ani->arm_x, ani->arm_y, first);
	}
    }

    img = image( ani->pixmap, MASKED );
    trans_offset_blit(img->pixmap, win.buf, x, y, ani->img_w, ani->img_h,
		      x_o, y_o, img->mask);
    draw_weapon(hand_dir, ent.weapon, et, x, y);

    /*
    draw_circle(win.buf, x, y, MAX(ani->img_w, ani->img_h),
		et->class == BADDIE? "red" : "green", 0);
    */


    if( ani->draw_hand ) {
	if( dir == ani->directions/2 ) {
	    draw_hand(0, hand_dir, x, y, ani->img_w, ani->img_h,
		      ani->arm_x, ani->arm_y, first);
	    draw_hand(0, hand_dir, x, y, ani->img_w, ani->img_h,
		      ani->arm_x, ani->arm_y, second);
	} else if( dir != 0 ) {
	    draw_hand(0, hand_dir, x, y, ani->img_w, ani->img_h,
		      ani->arm_x, ani->arm_y, second);
	}
    }

}


/* Directions in weapon image */
#define WEAPON_DIR 8
void draw_weapon(byte dir, byte weapon, ent_type_t *et, int x, int y)
{
    image_t *img;
    weap_type_t *wt;

    /****** Draw weapon **********/
    if( (wt = weapon_type(weapon)) == NULL ) {
	printf("(Client) %s: Bad weapon type %d!\n", __FILE__, weapon);
	return;
    }

    if( et->draw_weapon && wt->has_weapon_image ) {
	img = image(wt->weapon_image, MASKED);
	trans_offset_blit(img->pixmap, win.buf, x + 6, y - 6,
			  64, 50, 0, dir/(DEGREES/WEAPON_DIR) * 50, img->mask);
    }

}



extern float sin_lookup[];
extern float cos_lookup[];

static void draw_hand(byte hand, byte dir, int x, int y, int img_w, int img_h,
		      int shoulder_w, int shoulder_h, int side)
{
    image_t *img;
    int disx, disy;
    int x1, y1;

    disx = x + img_w / 2;
    disy = y + img_h - shoulder_h;

    disx -= 22;
    /* disy -= 9; */

    x1 = shoulder_w * cos_lookup[ dir ];
    y1 = shoulder_w * sin_lookup[ dir ] * 0.5; /* Adjust for perspective */

    if( side == LEFT ) {
	x1 *= -1;
	y1 *= -1;
    }

    x1 += disx;
    y1 += disy;

    img = image("hand.xpm", MASKED);
    trans_offset_blit(img->pixmap, win.buf, x1, y1, 48, 34, 0, 0, img->mask);

}
