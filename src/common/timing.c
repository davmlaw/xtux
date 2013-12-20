#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include "timing.h"

static msec_t tvtol(struct timeval t);
static struct timeval ltotv(msec_t t);


void cap_fps(int fps)
{
    static msec_t fr_st = INT32_MAX, fr_end;
    msec_t frame_len; /* Time each frame should take */
    msec_t fr_length; /* Time it took to perform last frame */

    frame_len = M_SEC / fps;
    fr_end = gettime();
    fr_length = fr_end - fr_st;
    if( fr_length < frame_len ) {
	delay( frame_len - fr_length );
    }
    fr_st = gettime();

}


/* Delay for i milliseconds */
void delay(msec_t i)
{

    if( i ) {
	struct timeval timeout;
	timeout = ltotv(i);
	select(0, NULL, NULL, NULL, &timeout);
    }

}


/* Like gettimeofday, but for our integer time representation */
msec_t gettime(void)
{
    struct timeval tv;

    gettimeofday( &tv, NULL );
    return tvtol(tv);

}


/* Converts a struct timeval to a msec_t integer representation */ 
static msec_t tvtol(struct timeval t)
{

    return (msec_t)M_SEC * t.tv_sec + t.tv_usec/M_SEC;

}

/* Converts a msec_t int to a struct timeval */
static struct timeval ltotv(msec_t t)
{
    struct timeval tv;

    tv.tv_sec  =  t / M_SEC;
    tv.tv_usec =  (t % M_SEC) * M_SEC;

    return tv;

}

