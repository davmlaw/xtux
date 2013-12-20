/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* #undef AF_LOCAL */
/* #undef DEBUG_SOCKET */
#define HAVE_MSGHDR_MSG_CONTROL 1
#define HAVE_SUN_LEN 1
#define HAVE_CMSG_ALIGN 1
#define HAVE_CMSG_LEN 1
#define HAVE_CMSG_SPACE 1
/* #undef PF_LOCAL */
#define TMPDIR "/tmp/ggzd"

/* Name of package */
#define PACKAGE "easysock"

/* Version number of package */
#define VERSION "0.2.0"

/* keep these at the end of the generated config.h */

#ifndef HAVE_SUN_LEN
#define SUN_LEN(ptr) ((size_t)(((struct sockaddr_un *) 0)->sun_path) + strlen ((ptr)->sun_path))
#endif

#ifdef HAVE_MSGHDR_MSG_CONTROL
#ifndef HAVE_CMSG_ALIGN
#define CMSG_ALIGN(len) (((len) + sizeof (size_t) - 1) & ~(sizeof (size_t) - 1))
#endif
#ifndef HAVE_CMSG_SPACE
#define CMSG_SPACE(len) (CMSG_ALIGN (len) + CMSG_ALIGN (sizeof (struct cmsghdr)))
#endif
#ifndef HAVE_CMSG_LEN
#define CMSG_LEN(len)   (CMSG_ALIGN (sizeof (struct cmsghdr)) + (len))
#endif
#endif

