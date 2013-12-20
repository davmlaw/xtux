/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* defined by automake */

#undef AF_LOCAL
#undef DEBUG_SOCKET
#undef HAVE_MSGHDR_MSG_CONTROL
#undef HAVE_SUN_LEN
#undef HAVE_CMSG_ALIGN
#undef HAVE_CMSG_LEN
#undef HAVE_CMSG_SPACE
#undef PF_LOCAL
#undef TMPDIR


/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */

@BOTTOM@
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

