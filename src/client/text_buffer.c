#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xtux.h"
#include "client.h"
#include "win.h"
#include "text_buffer.h"

extern client_t client;
extern win_t win;
extern char *colortab[NUM_COLORS];

typedef struct {
    char *line;
    int color;
    align_t alignment;
} tb_t;

tb_t tb[TEXT_BUF_LINES]; /* Text buffer */

static msec_t next_shift;

/* Shift every element left */
void text_buf_shift(void)
{
    int i;

    if( tb[0].line ) {
	free( tb[0].line );

	for( i=1 ; i < TEXT_BUF_LINES ; i++ ) {
	    tb[i-1] = tb[i];
	}

	tb[TEXT_BUF_LINES - 1].line = NULL;
    } else {
	printf("shifting empty list......\n");
    }

}


/* Push line onto the end of text buffer */
void text_buf_push(char *line, int color, align_t alignment)
{
    int i;
    
    if( !line[0] ) {
	printf("No message to push......\n");
	return;
    }


    for( i=0 ; i<TEXT_BUF_LINES ; i++ ) {
	if( tb[i].line == NULL )
	    break;
    }

    if( i == TEXT_BUF_LINES ) { /* No room */
	text_buf_shift(); /* Make room */
	i--;
    }

    tb[i].line = (char *)strdup(line);
    tb[i].color = (color >= 0 && color < NUM_COLORS)? color : 0;
    tb[i].alignment = alignment;
    next_shift = gettime() + client.text_message_display_time;

}


void text_buf_print(void)
{
    int font_h, rows;
    int y, i;

    font_h = font_height(2);
    y = 0;

    for( i=0 ; i<TEXT_BUF_LINES; i++ ) {
	if( tb[i].line ) {
	    win_print_column(win.buf, tb[i].line, 0, y, 2,
			     colortab[tb[i].color], win.width,
			     tb[i].alignment, &rows);
	    y += rows * font_h;
	}
    }


}


void text_buf_update(void)
{
    msec_t now;

    if( tb[0].line ) { /* Non empty list */
	if( (now = gettime()) >= next_shift ) {
	    text_buf_shift();
	    next_shift = now + client.text_message_display_time;
	}

	text_buf_print();

    }

}


void text_buf_clear(void)
{

    while( tb[0].line )
	text_buf_shift();

}
