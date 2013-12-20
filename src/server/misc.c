#include <stdlib.h>
#include <stdio.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "clients.h"
#include "misc.h"

extern map_t *map;

/* Give co-ordinates of top left hand corner of view window centering around
   the clients entity */
shortpoint_t calculate_screenpos(client_t *cl)
{
    shortpoint_t p;
    animation_t *ani;
    int x, y;
    int width, height;

    width = MIN( cl->view_w, map->width * TILE_W );
    height = MIN( cl->view_h, map->height * TILE_H );

    ani = entity_type_animation( cl->ent->type_info, cl->ent->mode );

    /* x,y = middle of focus entity */
    x = cl->ent->x + ani->img_w/2;
    y = cl->ent->y + ani->img_h/2;

    /* Center on focus */
    x -= width / 2;
    y -= height / 2;

    /* Check view area is inside map boundaries. */
    if( x >= map->width * TILE_W - width )
        x = map->width * TILE_W - width;
    else if( x < 0 )
	x = 0;

    if( y >= map->height * TILE_H - height)
        y = map->height * TILE_H - height;
    else if( y < 0 )
	y = 0;

    p.x = x;
    p.y = y;
    return p;

}
