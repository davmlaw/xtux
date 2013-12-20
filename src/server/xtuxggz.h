/**********************************************************************
 Functions needed to support the GGZ mode
***********************************************************************/

#ifndef __XTUXS_XTUXGGZ_H
#define __XTUXS_XTUXGGZ_H

int xtuxggz_main_loop();
int xtuxggz_init();
int xtuxggz_find_ggzname( int fd, char *n, int len );
int xtuxggz_is_sock_set(int *newfd);
int xtuxggz_exit();
int xtuxggz_avoid_perdig_effect(int new_sock);
int xtuxggz_giveme_sock(void);

#endif /* __XTUXS_XTUXGGZ_H */
