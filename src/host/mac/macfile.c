# include <Files.h>
# include <StandardFile.h>
# include <Errors.h>
# define INCLUDE_FILE_IO
# include "dgd.h"

extern Uint m2utime(long t);

typedef struct {
    short fref;		/* file ref */
    Str255 fname;	/* file name */
} fdtype;

static fdtype fdtab[20];

static long crea;	/* creator */
static long type;	/* file type */

static short vref;	/* volume refNum of current directory */
static long dirid;	/* directory ID */


/*
 * NAME:	fsinit()
 * DESCRIPTION:	initialize file functions
 */
void fsinit(long fcrea, long ftype)
{
    WDPBRec buf;
    Str255 str;

    crea = fcrea;
    type = ftype;
    buf.ioNamePtr = str;
    PBHGetVolSync(&buf);
    vref = buf.ioVRefNum;
    dirid = buf.ioWDDirID;
}


/*
 * NAME:	getpath()
 * DESCRIPTION:	get the full path of a file
 */
char *getpath(char *buf, short vref, unsigned char *fname)
{
    Str255 str;
    DirInfo dir;

    buf += STRINGSZ - 1;
    buf[0] = '\0';
    memcpy(str, fname, fname[0] + 1);
    memcpy(buf -= fname[0], fname + 1, fname[0]);

    dir.ioNamePtr = str;
    dir.ioCompletion = NULL;
    dir.ioFDirIndex = 0;
    dir.ioVRefNum = vref;
    dir.ioDrDirID = 0;
    for (;;) {
	PBGetCatInfoSync((CInfoPBPtr) &dir);
	memcpy(buf -= str[0], str + 1, str[0]);
	if (dir.ioDrDirID == 2) {
	    return buf;
	}
	*--buf = ':';
	dir.ioFDirIndex = -1;
	dir.ioDrDirID = dir.ioDrParID;
    }
}

/*
 * NAME:	getfile()
 * DESCRIPTION:	get the path of a specific file with a standard dialog
 */
char *getfile(char *buf, long type)
{
    Point where;
    SFTypeList list;
    SFReply reply;

    where.h = 82;
    where.v = 124;
    list[0] = type;
    SFGetFile(where, NULL, NULL, 1, list, NULL, &reply);
    if (reply.good) {
	return getpath(buf, reply.vRefNum, reply.fName);
    } else {
	return NULL;
    }
}


/*
 * NAME:	path_native()
 * DESCRIPTION:	deal with path that's already native
 */
char *path_native(char *to, char *from)
{
    to[0] = '/';	/* mark as native */
    strncpy(to + 1, from, STRINGSZ - 1);
    to[STRINGSZ - 1] = '\0';
    return to;
}

/*
 * NAME:	path_file()
 * DESCRIPTION:	translate a path to a pascal string
 */
static unsigned char *path_file(unsigned char *to, const char *from)
{
    char *p, *q;
    int n;

    p = (char *) to + 1;
    q = (char *) from;
    if (*q == '/') {
	/* native path: copy directly */
	q++;
	while (*q != '\0') {
	    *p++ = *q++;
	}
	to[0] = (unsigned char *) p - to - 1;
	return to;
    }

    n = 0;
    *p++ = ':';
    if (*q == '.' && q[1] == '\0') {
	*p = '\0';
	to[0] = 1;
	return to;
    }
    n++;

    while (n < 255 && *q != '\0') {
	if (*q == '/') {
	    *p = ':';
	} else if (*q == ':') {
	    *p = '/';
	} else {
	    *p = *q;
	}
	p++;
	q++;
	n++;
    }
    to[0] = n;

    return to;
}

/*
 * NAME:	path_unfile()
 * DESCRIPTION:	translate a pascal filename to a path
 */
static char *path_unfile(char *to, const unsigned char *from)
{
    char *p, *q;
    int n;

    for (p = to, q = (char *) from + 1, n = from[0]; n != 0; p++, q++, --n) {
	if (*q == '/') {
	    *p = ':';
	} else {
	    *p = *q;
	}
    }
    *p = '\0';

    return to;
}


static long sdirid;		/* scan directory ID */
static short sdiridx;		/* scan directory index */
static HFileInfo sdirbuf;	/* scan dir file info */

/*
 * NAME:	P->opendir()
 * DESCRIPTION:	open a directory
 */
bool P_opendir(char *path)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = path_file(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask) == 0) {
	return FALSE;
    }
    sdirid = buf.ioDirID;
    sdiridx = 1;
    return TRUE;
}

/*
 * NAME:	P->readdir()
 * DESCRIPTION:	read the next filename from the currently open directory
 */
char *P_readdir(void)
{
    Str255 str;
    static char path[34];

    sdirbuf.ioVRefNum = vref;
    sdirbuf.ioFDirIndex = sdiridx++;
    sdirbuf.ioNamePtr = str;
    sdirbuf.ioDirID = sdirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &sdirbuf) != noErr) {
	return NULL;
    }

    return path_unfile(path, str);
}

/*
 * NAME:	P->closedir()
 * DESCRIPTION:	close the currently open directory
 */
void P_closedir(void)
{
    sdiridx = 0;
}


/*
 * NAME:	P->open()
 * DESCRIPTION:	open a file
 */
int P_open(char *path, int flags, int mode)
{
    int fd;
    short fref;

    for (fd = 0; fdtab[fd].fref != 0; fd++) {
	if (fd == sizeof(fdtab) / sizeof(short) - 1) {
	    return -1;
	}
    }

    switch (HOpen(vref, dirid, path_file(fdtab[fd].fname, path), fsRdWrShPerm,
		  &fref)) {
    case noErr:
	if ((flags & O_EXCL) ||
	    ((flags & O_TRUNC) && SetEOF(fref, 0L) != noErr) ||
	    ((flags & O_APPEND) && SetFPos(fref, fsFromLEOF, 0) != noErr)) {
	    FSClose(fref);
	    return -1;
	}
	break;

    case fnfErr:
    case dirNFErr:
	if ((flags & O_CREAT) &&
	    HCreate(vref, dirid, fdtab[fd].fname, crea, type) == noErr &&
	    HOpen(vref, dirid, fdtab[fd].fname, fsRdWrShPerm, &fref) == noErr) {
	    break;
	}
	/* fall through */

    default:
	return -1;
    }

    fdtab[fd].fref = fref;
    return fd;
}

/*
 * NAME:	P->close()
 * DESCRIPTION:	close a file
 */
int P_close(int fd)
{
    FSClose(fdtab[fd].fref);
    fdtab[fd].fref = 0;

    return 0;
}

/*
 * NAME:	P->read()
 * DESCRIPTION:	read from a file
 */
int P_read(int fd, void *buf, int nbytes)
{
    long count;

    count = nbytes;
    switch (FSRead(fdtab[fd].fref, &count, buf)) {
    case noErr:
    case eofErr:
	return (int) count;

    default:
	return -1;
    }
}

/*
 * NAME:	P->write()
 * DESCRIPTION:	write to a file
 */
int P_write(int fd, const void *buf, int nbytes)
{
    long count;

    count = nbytes;
    switch (FSWrite(fdtab[fd].fref, &count, buf)) {
    case noErr:
    case dskFulErr:
	return (int) count;

    default:
	return -1;
    }
}

/*
 * NAME:	P->lseek()
 * DESCRIPTION:	seek on a file
 */
long P_lseek(int fd, long offset, int whence)
{
    short mode;

    switch (whence) {
    case SEEK_SET:
	mode = fsFromStart;
	break;

    case SEEK_CUR:
	mode = fsFromMark;
	break;

    case SEEK_END:
	mode = fsFromLEOF;
	break;
    }

    /*
     * note: no seek beyond the end of the file
     */
    if (SetFPos(fdtab[fd].fref, mode, offset) != noErr) {
	return -1;
    }
    if (mode != fsFromStart) {
	GetFPos(fdtab[fd].fref, &offset);
    }
    return offset;
}

/*
 * NAME:	P->stat()
 * DESCRIPTION:	get information about a file
 */
int P_stat(char *path, struct stat *sb)
{
    HFileInfo buf;
    Str255 str;

    if (sdiridx != 0) {
	buf = sdirbuf;
    } else {
	buf.ioVRefNum = vref;
	buf.ioFDirIndex = 0;
	buf.ioNamePtr = path_file(str, path);
	buf.ioDirID = dirid;
	if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr) {
	    return -1;
	}
    }

    sb->st_mode = (buf.ioFlAttrib & ioDirMask) ? S_IFDIR : S_IFREG;
    sb->st_size = buf.ioFlLgLen;
    sb->st_mtime = (long) m2utime(buf.ioFlMdDat);

    return 0;
}

/*
 * NAME:	P->fstat()
 * DESCRIPTION:	get information about an open file
 */
int P_fstat(int fd, struct stat *sb)
{
    HFileInfo buf;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = fdtab[fd].fname;
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr) {
	return -1;
    }

    sb->st_mode = (buf.ioFlAttrib & ioDirMask) ? S_IFDIR : S_IFREG;
    sb->st_size = buf.ioFlLgLen;
    sb->st_mtime = (long) m2utime(buf.ioFlMdDat);

    return 0;
}

/*
 * NAME:	P->unlink()
 * DESCRIPTION:	remove a file (but not a directory)
 */
int P_unlink(char *path)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = path_file(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask)) {
	return -1;
    }
    return (HDelete(vref, dirid, str) == noErr) ? 0 : -1;
}

/*
 * NAME:	P->rename()
 * DESCRIPTION:	rename a file
 */
int P_rename(char *from, char *to)
{
    char *p, *q;
    Str255 dir1, dir2, file1, file2;
    HFileInfo buf;
    long xdirid;

    p = strrchr(from, ':');
    q = strrchr(to, ':');
    if (p == NULL || q == NULL) {
	return -1;
    }
    memcpy(dir1 + 1, from, dir1[0] = p - from);
    if (dir1[0] == 0) {
	dir1[++(dir1[0])] = ':';
    }
    path_file(file1, p);
    memcpy(dir2 + 1, to, dir2[0] = q - to);
    if (dir2[0] == 0) {
	dir2[++(dir2[0])] = ':';
    }
    path_file(file2, q);

    /* source directory must exist */
    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = dir1;
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask) == 0) {
	return -1;
    }
    xdirid = buf.ioDirID;

    /* source file must exist */
    buf.ioNamePtr = file1;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr) {
	return -1;
    }
    if (buf.ioFlAttrib & ioDirMask) {
	file1[++(file1[0])] = ':';
	file2[++(file2[0])] = ':';
    }

    if (p - from != q - to || memcmp(from, to, p - from) != 0) {
	CMovePBRec move;

	/*
	 * move to different directory
	 */

	/* destination directory must exist */
	buf.ioNamePtr = dir2;
	buf.ioDirID = dirid;
	if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	    (buf.ioFlAttrib & ioDirMask) == 0) {
	    return -1;
	}
	move.ioNewDirID = buf.ioDirID;

	/* destination must not already exist */
	buf.ioNamePtr = file2;
	if (PBGetCatInfoSync((CInfoPBPtr) &buf) == noErr) {
	    return -1;
	}

	/* rename source */
	memcpy(dir1, file1, file1[0] + 1);
	memcpy(file1, "\p:_tmp0000", 6);
	do {
	    static short count;

	    if (count == 9999) {
		count = 0;
	    }
	    sprintf((char *) file1 + 6, "%04d", ++count);
	    buf.ioNamePtr = file1;
	    buf.ioDirID = xdirid;
	} while (PBGetCatInfoSync((CInfoPBPtr) &buf) == noErr);
	if (dir1[dir1[0]] == ':') {
	    file1[++(file1[0])] = ':';
	}
	if (HRename(vref, xdirid, dir1, file1) != noErr) {
	    return -1;
	}

	/* move source to new directory */
	move.ioNamePtr = file1;
	move.ioVRefNum = vref;
	move.ioNewName = NULL;
	move.ioDirID = xdirid;
	if (PBCatMoveSync(&move) != noErr) {
	    /* back to old name */
	    HRename(vref, xdirid, file1, dir1);
	    return -1;
	}

	xdirid = move.ioNewDirID;
    }

    return (HRename(vref, xdirid, file1, file2) == noErr) ? 0 : -1;
}

/*
 * NAME:	P->access()
 * DESCRIPTION:	check access on a file
 */
int P_access(char *path, int mode)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = path_file(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr) {
	return -1;
    }

    if (mode == W_OK) {
	return (buf.ioFlAttrib & 0x01) ? -1 : 0;
    }
    return 0;
}

/*
 * NAME:	P->mkdir()
 * DESCRIPTION:	create a directory
 */
int P_mkdir(char *path, int mode)
{
    Str255 str;
    long newdir;

    if (DirCreate(vref, dirid, path_file(str, path), &newdir) == noErr) {
	return 0;
    } else {
	return -1;
    }
}

/*
 * NAME:	P->rmdir()
 * DESCRIPTION:	remove an empty directory
 */
int P_rmdir(char *path)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = path_file(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask) == 0) {
	return -1;
    }
    return (HDelete(vref, dirid, str) == noErr) ? 0  : -1;
}

/*
 * NAME:	P->chdir()
 * DESCRIPTION:	change the current directory
 */
int P_chdir(char *path)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = path_file(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask) == 0) {
	return -1;
    }
    dirid = buf.ioDirID;
    return 0;
}
