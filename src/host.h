# ifdef MINIX_68K

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_TELNET
# include "host/telnet.h"
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

/* # define UCHAR(c)	((char) (c))		      *//* unsigned character */
# define UCHAR(c)	((int) ((c) & 0xff))		/* unsigned character */
# define SCHAR(c)	((char) (c))			/* signed character */
/* # define SCHAR(c)	((((char) (c)) - 128) ^ -128) *//* signed character */

typedef long Int;
typedef unsigned long Uint;

# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)

# define FS_BLOCK_SIZE		1024

extern int   rename		P((const char*, const char*));

# endif	/* MINIX_68K */


# ifdef ATARI_ST

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_TELNET
# include "host/telnet.h"
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# define UCHAR(c)	((int)((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

typedef int Int;
typedef unsigned int Uint;

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
						 (unsigned int) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		512

# endif	/* ATARI_ST */


# ifdef WIN32

# include <limits.h>
# include <sys\types.h>
# include <malloc.h>

# ifdef INCLUDE_FILE_IO
# include <io.h>
# include <direct.h>
# include <fcntl.h>
# include <sys\stat.h>

# define open			_open
# define close			_close
# define read			_read
# define write			_write
# define lseek			_lseek
# define unlink			_unlink
# define chdir			P_chdir
# define mkdir(dir, mode)	_mkdir(dir)
# define rmdir			_rmdir
# define access			_access
# define stat			_stat

extern int P_chdir(char*);

# define W_OK	2
# endif

# ifdef INCLUDE_TELNET
# include "host\telnet.h"
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# define STRUCT_AL	4		/* define this if align(struct) > 2 */
# define UCHAR(c)	((int)((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

typedef int Int;
typedef unsigned int Uint;

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
						 (unsigned int) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		2048

# define exit			dgd_exit

extern void dgd_exit(int);

# endif	/* WIN32 */


# ifdef SUNOS4

# define GENERIC_BSD

# include <alloca.h>
# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
						 (unsigned int) (size)))
# define AFREE(ptr)		/* on function return */

# endif	/* SUNOS4 */


# if defined(NETBSD) || defined(BSD386)

# define GENERIC_BSD

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
						 (unsigned int) (size)))
# define AFREE(ptr)		/* on function return */

# endif /* NETBSD || BSD386 */


# ifdef LINUX

# define GENERIC_SYSV

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
						 (unsigned int) (size)))
# define AFREE(ptr)		/* on function return */

# endif /* LINUX */


# ifdef GENERIC_BSD

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_TELNET
# include <arpa/telnet.h>
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# define STRUCT_AL	4		/* define this if align(struct) > 2 */
# define UCHAR(c)	((int) ((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

typedef int Int;
typedef unsigned int Uint;

# ifndef ALLOCA
# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)
# endif

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_BSD */


# ifdef GENERIC_SYSV

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# define FNDELAY	O_NDELAY
# endif

# ifdef INCLUDE_TELNET
# include <arpa/telnet.h>
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# define STRUCT_AL	4		/* define this if align(struct) > 2 */
# define UCHAR(c)	((int) ((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

typedef int Int;
typedef unsigned int Uint;

# ifndef ALLOCA
# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)
# endif

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_SYSV */


extern void  P_getevent	P((void));
extern void  P_message	P((char*));

# ifndef O_BINARY
# define O_BINARY	0
# endif

extern bool  P_opendir	P((char*));
extern char *P_readdir	P((void));
extern void  P_closedir	P((void));

extern void  P_srandom	P((long));
extern long  P_random	P((void));

extern Uint  P_time	P((void));
extern char *P_ctime	P((Uint));

extern void  P_alarm	P((unsigned int));
extern bool  P_timeout	P((void));

extern char *P_crypt	P((char*, char*));

/* these must be the same on all hosts */
# define BEL	'\007'
# define BS	'\010'
# define HT	'\011'
# define LF	'\012'
# define VT	'\013'
# define FF	'\014'
# define CR	'\015'

# define ALIGN(x, s)	(((x) + (s) - 1) & ~((s) - 1))
