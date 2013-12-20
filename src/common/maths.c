#include <math.h>

#include "xtux.h"
#include "version.h"

float sin_lookup[DEGREES];
float cos_lookup[DEGREES];

void calc_lookup_tables(int degrees)
{
    int i;
    int semicircle, quadrant;
    float d;
    float s, c; /* Degree's (RADIANS) */
    float inc = M_PI /(degrees/2); /* Convert from Radians to our degrees */
    
    /*
      We only need to calculate 1/4 of the circle, because the ratios can
      be determined for the rest of the circle by one quadrant. We use a
      lookup table (1Kb) so we don't have to call cos() & sin() all the time
    */

    semicircle = degrees / 2;
    quadrant = degrees / 4;
    
    for( d=0.0, i=0 ; i <= quadrant ; d+=inc, i++ ) {
	s = sin(d);
	c = cos(d);
	
	/* 1st quadrant */
	sin_lookup[i] = s;
	cos_lookup[i] = c;
	/* 2nd quadrant */
	sin_lookup[semicircle - i] =  s;
	cos_lookup[semicircle - i] = -c;
	/* 3rd quadrant */
	sin_lookup[semicircle + i] = -s;
	cos_lookup[semicircle + i] = -c;
	/* 4th quadrant */
	if( i ) {
	    sin_lookup[degrees - i] = -s;
	    cos_lookup[degrees - i] =  c;
	}
    }    

    /* Correct some floating point inaccuracies */
    sin_lookup[ quadrant * 0 ] = 0.0;
    cos_lookup[ quadrant * 1 ] = 0.0;
    sin_lookup[ quadrant * 2 ] = 0.0;
    cos_lookup[ quadrant * 3 ] = 0.0;

}


int point_distance(point A, point B)
{
    register int x, y;
  
    x = B.x - A.x;
    y = B.y - A.y;
    return (int)sqrt( (double)(x * x + y * y) );

}


/* Return the angle (0.255) between U and V using dot product */
byte angle_between(vector_t U, vector_t V)
{
    float dot_product;
    float deg = DEGREES/(M_PI*2); /* to convert from radians -> our degrees*/
    float l_u, l_v; /* Vector lengths of U and V */
    byte angle;

    /*
      u . v = ||u|| ||v|| cos theta
      theta = cos-1 (u.v) / (||u|| ||v||)

      u.v = u1v1 + u2v2

    */

    l_u = sqrt( U.x * U.x + U.y * U.y );
    l_v = sqrt( V.x * V.x + V.y * V.y );

    if( l_u == 0 || l_v == 0 )
	return 0;

    dot_product = U.x * V.x + U.y + V.y;
    angle = deg * acos( dot_product / (l_u + l_v) );

    if( V.x < U.x )
	angle = DEGREES - angle;

    return angle;

}



/* Originally from Graphic Gems 2
 *
 *   This function computes whether two line segments,
 *   respectively joining the input points (x1,y1) -- (x2,y2)
 *   and the input points (x3,y3) -- (x4,y4) intersect.
 *   If the lines intersect, the output variables x, y are
 *   set to coordinates of the point of intersection.
 *
 *   All values are in integers.  The returned value is rounded
 *   to the nearest integer point.
 *
 *   If non-integral grid points are relevant, the function
 *   can easily be transformed by substituting floating point
 *   calculations instead of integer calculations.
 *
 *   Entry
 *        x1, y1,  x2, y2   Coordinates of endpoints of one segment.
 *        x3, y3,  x4, y4   Coordinates of endpoints of other segment.
 *
 *   Exit
 *        x, y              Coordinates of intersection point.
 *
 *   The value returned by the function is one of:
 *
 *        DONT_INTERSECT    0
 *        DO_INTERSECT      1
 *        COLLINEAR         2
 *
 * Error conditions:
 *
 *     Depending upon the possible ranges, and particularly on 16-bit
 *     computers, care should be taken to protect from overflow.
 *
 *     In the following code, 'long' values have been used for this
 *     purpose, instead of 'int'.
 *
 */

/**************************************************************
 *                                                            *
 *    NOTE:  The following macro to determine if two numbers  *
 *    have the same sign, is for 2's complement number        *
 *    representation.  It will need to be modified for other  *
 *    number systems.                                         *
 *                                                            *
 **************************************************************/

#define SAME_SIGNS( a, b )	\
		(((long) ((unsigned long) a ^ (unsigned long) b)) >=0 )


int intersection_test(point A, point B, point C, point D,
		      point *INTERSECTION)
{
    long a1, b1, c1;
    long a2, b2, c2;
    long r1, r2, r3, r4;
    long denom, offset, num;


    /* Compute a1, b1, c1, where line joining points 1 and 2
     * is "a1 x  +  b1 y  +  c1  =  0".
     */

    a1 = B.y - A.y;
    b1 = A.x - B.x;
    c1 = B.x * A.y - A.x * B.y;

    /* Compute r3 and r4.
     */
    r3 = a1 * C.x + b1 * C.y + c1;
    r4 = a1 * D.x + b1 * D.y + c1;

    /* Check signs of r3 and r4.  If both point 3 and point 4 lie on
     * same side of line 1, the line segments do not intersect.
     */

    if ( r3 != 0 &&
         r4 != 0 &&
         SAME_SIGNS( r3, r4 ))
        return 0;

    /* Compute a2, b2, c2 */

    a2 = D.y - C.y;
    b2 = C.x - D.x;
    c2 = D.x * C.y - C.x * D.y;

    /* Compute r1 and r2 */

    r1 = a2 * A.x + b2 * A.y + c2;
    r2 = a2 * B.x + b2 * B.y + c2;

    /* Check signs of r1 and r2.  If both point 1 and point 2 lie
     * on same side of second line segment, the line segments do
     * not intersect.
     */

    if ( r1 != 0 &&
         r2 != 0 &&
         SAME_SIGNS( r1, r2 ))
        return 0;

    /* Line segments intersect: compute intersection point.
     */

    denom = a1 * b2 - a2 * b1;
    if ( denom == 0 )
        return 0;
    offset = denom < 0 ? - denom / 2 : denom / 2;

    /* The denom/2 is to get rounding instead of truncating.  It
     * is added or subtracted to the numerator, depending upon the
     * sign of the numerator.
     */

    num = b1 * c2 - b2 * c1;
    INTERSECTION->x = ( num < 0 ? num - offset : num + offset ) / denom;

    num = a2 * c1 - a1 * c2;
    INTERSECTION->y = ( num < 0 ? num - offset : num + offset ) / denom;

    return 1;
}









