# define INCLUDE_FILE_IO
# include "dgd.h"
# include "swap.h"

# define BSET(map, bit)		(map[(bit) >> 3] |= (1 << ((bit) & 7)))
# define BCLR(map, bit)		(map[(bit) >> 3] &= ~(1 << ((bit) & 7)))
# define BTST(map, bit)		(map[(bit) >> 3] & (1 << ((bit) & 7)))

typedef struct _header_ {	/* swap slot header */
    struct _header_ *prev;	/* previous in swap slot list */
    struct _header_ *next;	/* next in swap slot list */
    sector sec;			/* the sector that uses this slot */
    sector swap;		/* the swap sector (if any) */
    bool dirty;			/* has the swap slot been written to? */
} header;

static char *swapfile;			/* swap file name */
static int swap;			/* swap file descriptor */
static int dump;			/* dump file descriptor */
static int restore;			/* restore file descriptor */
static char *mem;			/* swap slots in memory */
static sector *map, *smap;		/* sector map, swap free map */
static sector mfree, sfree;		/* free sector lists */
static char *bmap;			/* sector bitmap */
static char *cbuf;			/* sector buffer */
static sector cached;			/* sector currently cached in cbuf */
static header *first, *last;		/* first and last swap slot */
static header *lfree;			/* free swap slot list */
static long slotsize;			/* sizeof(header) + size of sector */
static unsigned int sectorsize;		/* size of sector */
static unsigned int restoresecsize;	/* size of sector in restore file */
static sector swapsize, cachesize;	/* # of sectors in swap and cache */
static sector nsectors;			/* total swap sectors */
static sector nfree;			/* # free sectors */
static sector ssectors;			/* sectors actually in swap file */
static sector dsectors, dcursec;	/* dump sectors */

/*
 * NAME:	swap->init()
 * DESCRIPTION:	initialize the swap device
 */
void sw_init(file, total, cache, secsize)
char *file;
register unsigned int total, cache;
unsigned int secsize;
{
    register header *h;
    register sector i;

    /* allocate and initialize all tables */
    swapfile = file;
    swapsize = total;
    cachesize = cache;
    sectorsize = secsize;
    slotsize = sizeof(header) + secsize;
    mem = ALLOC(char, slotsize * cache);
    map = ALLOC(sector, total);
    smap = ALLOC(sector, total);
    bmap = ALLOC(char, (total + 7) >> 3);
    cbuf = ALLOC(char, secsize);
    cached = SW_UNUSED;

    /* 0 sectors allocated */
    nsectors = 0;
    ssectors = 0;
    nfree = 0;
    dsectors = 0;

    /* init free sector maps */
    mfree = SW_UNUSED;
    sfree = SW_UNUSED;
    lfree = h = (header *) mem;
    for (i = cache - 1; i > 0; --i) {
	h->sec = SW_UNUSED;
	h->next = (header *) ((char *) h + slotsize);
	h = h->next;
    }
    h->sec = SW_UNUSED;
    h->next = (header *) NULL;

    /* no swap slots in use yet */
    first = (header *) NULL;
    last = (header *) NULL;

    swap = dump = -1;
}

/*
 * NAME:	swap->finish()
 * DESCRIPTION:	clean up swapfile
 */
void sw_finish()
{
    if (swap >= 0) {
	char buf[STRINGSZ];

	P_close(swap);
	P_unlink(path_native(buf, swapfile));
    }
    if (dump >= 0) {
	P_close(dump);
    }
}

/*
 * NAME:	create()
 * DESCRIPTION:	create the swap file
 */
static void sw_create()
{
    char buf[STRINGSZ];

    memset(cbuf, '\0', sectorsize);
    swap = P_open(path_native(buf, swapfile),
		  O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0600);
    if (swap < 0 || P_write(swap, cbuf, sectorsize) < 0) {
	fatal("cannot create swap file \"%s\"", swapfile);
    }
}

/*
 * NAME:	copy()
 * DESCRIPTION:	copy sectors from dump to swap
 */
static void copy(sec, n)
register sector sec, n;
{
    dsectors -= n;

    if (swap < 0) {
	sw_create();
    }

    while (n > 0) {
	while (!BTST(bmap, dcursec)) {
	    dcursec++;
	}
	P_lseek(dump, (map[dcursec] + 1L) * sectorsize, SEEK_SET);
	if (P_read(dump, cbuf, sectorsize) <= 0) {
	    fatal("cannot read dump file");
	}
	P_lseek(swap, (sec + 1L) * sectorsize, SEEK_SET);
	if (P_write(swap, cbuf, sectorsize) < 0) {
	    fatal("cannot write swap file");
	}

	BCLR(bmap, dcursec);
	map[dcursec++] = sec++;
	--n;
    }
}

/*
 * NAME:	swap->newv()
 * DESCRIPTION:	initialize a new vector of sectors
 */
void sw_newv(vec, size)
register sector *vec;
register unsigned int size;
{
    while (mfree != SW_UNUSED) {
	/* reuse a previously deleted sector */
	if (size == 0) {
	    return;
	}
	mfree = map[*vec = mfree];
	map[*vec++] = SW_UNUSED;
	--nfree;
	--size;
    }

    while (size > 0) {
	/* allocate a new sector */
	if (nsectors == swapsize) {
	    fatal("out of sectors");
	}
	BCLR(bmap, nsectors);
	map[*vec++ = nsectors++] = SW_UNUSED;
	--size;
    }
}

/*
 * NAME:	swap->wipev()
 * DESCRIPTION:	wipe a vector of sectors
 */
void sw_wipev(vec, size)
register sector *vec;
register unsigned int size;
{
    register sector sec, i;
    register header *h;

    while (dsectors > 0) {
	if (size == 0) {
	    return;
	}
	sec = *vec++;
	i = map[sec];
	if (i < cachesize && (h=(header *) (mem + i * slotsize))->sec == sec) {
	    i = h->swap;
	    h->swap = SW_UNUSED;
	} else {
	    map[sec] = SW_UNUSED;
	}
	if (i != SW_UNUSED) {
	    if (BTST(bmap, sec)) {
		/*
		 * free sector in dump file
		 */
		BCLR(bmap, sec);
		--dsectors;
	    } else {
		/*
		 * replace by sector from dump file
		 */
		copy(i, (sector) 1);
	    }
	}
	--size;
    }

    vec += size;
    while (size > 0) {
	sec = *--vec;
	i = map[sec];
	if (i < cachesize && (h=(header *) (mem + i * slotsize))->sec == sec) {
	    i = h->swap;
	    h->swap = SW_UNUSED;
	} else {
	    map[sec] = SW_UNUSED;
	}
	if (i != SW_UNUSED) {
	    /*
	     * free sector in swap file
	     */
	    smap[i] = sfree;
	    sfree = i;
	}
	--size;
    }
}

/*
 * NAME:	swap->delv()
 * DESCRIPTION:	delete a vector of swap sectors
 */
void sw_delv(vec, size)
register sector *vec;
register unsigned int size;
{
    register sector sec, i;
    register header *h;

    /*
     * note: sectors must have been wiped before being deleted!
     */
    vec += size;
    while (size > 0) {
	sec = *--vec;
	i = map[sec];
	if (i < cachesize && (h=(header *) (mem + i * slotsize))->sec == sec) {
	    /*
	     * remove the swap slot from the first-last list
	     */
	    if (h != first) {
		h->prev->next = h->next;
	    } else {
		first = h->next;
		if (first != (header *) NULL) {
		    first->prev = (header *) NULL;
		}
	    }
	    if (h != last) {
		h->next->prev = h->prev;
	    } else {
		last = h->prev;
		if (last != (header *) NULL) {
		    last->next = (header *) NULL;
		}
	    }
	    /*
	     * put the cache slot in the free cache slot list
	     */
	    h->sec = SW_UNUSED;
	    h->next = lfree;
	    lfree = h;
	}

	/*
	 * put sec in free sector list
	 */
	map[sec] = mfree;
	mfree = sec;
	nfree++;

	--size;
    }
}

/*
 * NAME:	swap->load()
 * DESCRIPTION:	reserve a swap slot for sector sec. If fill == TRUE, load it
 *		from the swap file if appropriate.
 */
static header *sw_load(sec, fill)
sector sec;
bool fill;
{
    register header *h;
    register sector load, save;

    load = map[sec];
    if (load >= cachesize ||
	(h=(header *) (mem + load * slotsize))->sec != sec) {
	/*
	 * the sector is either unused or in the swap file
	 */
	if (lfree != (header *) NULL) {
	    /*
	     * get swap slot from the free swap slot list
	     */
	    h = lfree;
	    lfree = h->next;
	} else {
	    /*
	     * No free slot available, use the last one in the swap slot list
	     * instead.
	     */
	    h = last;
	    last = h->prev;
	    if (last != (header *) NULL) {
		last->next = (header *) NULL;
	    }
	    save = h->swap;
	    if (h->dirty) {
		/*
		 * Dump the sector to swap file
		 */

		if (save == SW_UNUSED) {
		    /*
		     * allocate new sector in swap file
		     */
		    if (sfree == SW_UNUSED) {
			save = ssectors++;
		    } else {
			save = sfree;
			sfree = smap[save];
		    }
		}

		if (swap < 0) {
		    sw_create();
		}
		P_lseek(swap, (save + 1L) * sectorsize, SEEK_SET);
		if (P_write(swap, (char *) (h + 1), sectorsize) < 0) {
		    fatal("cannot write swap file");
		}
	    }
	    map[h->sec] = save;
	}
	h->sec = sec;
	h->swap = load;
	h->dirty = FALSE;
	/*
	 * The slot has been reserved. Update map.
	 */
	map[sec] = ((long) h - (long) mem) / slotsize;

	if (load != SW_UNUSED) {
	    if (dsectors > 0 && BTST(bmap, sec)) {
		if (fill) {
		    /*
		     * load the sector from the dump file
		     */
		    P_lseek(dump, (load + 1L) * sectorsize, SEEK_SET);
		    if (P_read(dump, (char *) (h + 1), sectorsize) <= 0) {
			fatal("cannot read dump file");
		    }
		}

		BCLR(bmap, sec);
		--dsectors;

		h->swap = SW_UNUSED;
		h->dirty = TRUE;
	    } else if (fill) {
		/*
		 * load the sector from the swap file
		 */
		P_lseek(swap, (load + 1L) * sectorsize, SEEK_SET);
		if (P_read(swap, (char *) (h + 1), sectorsize) <= 0) {
		    fatal("cannot read swap file");
		}
	    }
	} else if (fill) {
	    /* zero-fill new sector */
	    memset(h + 1, '\0', sectorsize);
	}
    } else {
	/*
	 * The sector already had a slot. Remove it from the first-last list.
	 */
	if (h != first) {
	    h->prev->next = h->next;
	} else {
	    first = h->next;
	}
	if (h != last) {
	    h->next->prev = h->prev;
	} else {
	    last = h->prev;
	    if (last != (header *) NULL) {
		last->next = (header *) NULL;
	    }
	}
    }
    /*
     * put the sector at the head of the first-last list
     */
    h->prev = (header *) NULL;
    h->next = first;
    if (first != (header *) NULL) {
	first->prev = h;
    } else {
	last = h;	/* last was NULL too */
    }
    first = h;

    return h;
}

/*
 * NAME:	swap->readv()
 * DESCRIPTION:	read bytes from a vector of sectors
 */
void sw_readv(m, vec, size, idx)
register char *m;
register sector *vec;
register Uint size, idx;
{
    register unsigned int len;

    vec += idx / sectorsize;
    idx %= sectorsize;
    do {
	len = (size > sectorsize - idx) ? sectorsize - idx : size;
	memcpy(m, (char *) (sw_load(*vec++, TRUE) + 1) + idx, len);
	idx = 0;
	m += len;
    } while ((size -= len) > 0);
}

/*
 * NAME:	swap->writev()
 * DESCRIPTION:	write bytes to a vector of sectors
 */
void sw_writev(m, vec, size, idx)
register char *m;
register sector *vec;
register Uint size, idx;
{
    register header *h;
    register unsigned int len;

    vec += idx / sectorsize;
    idx %= sectorsize;
    do {
	len = (size > sectorsize - idx) ? sectorsize - idx : size;
	h = sw_load(*vec++, (len != sectorsize));
	h->dirty = TRUE;
	memcpy((char *) (h + 1) + idx, m, len);
	idx = 0;
	m += len;
    } while ((size -= len) > 0);
}

/*
 * NAME:	swap->dreadv()
 * DESCRIPTION:	restore bytes from a vector of sectors in dump file
 */
void sw_dreadv(m, vec, size, idx)
register char *m;
register sector *vec;
register Uint size, idx;
{
    register unsigned int len;

    vec += idx / restoresecsize;
    idx %= restoresecsize;
    do {
	len = (size > restoresecsize - idx) ? restoresecsize - idx : size;
	if (*vec != cached) {
	    P_lseek(restore, (smap[*vec] + 1L) * restoresecsize, SEEK_SET);
	    if (P_read(restore, cbuf, restoresecsize) <= 0) {
		fatal("cannot read dump file");
	    }
	    cached = *vec;
	}
	vec++;
	memcpy(m, cbuf + idx, len);
	idx = 0;
	m += len;
    } while ((size -= len) > 0);
}

/*
 * NAME:	swap->mapsize()
 * DESCRIPTION:	count the number of sectors required for size bytes + a map
 */
sector sw_mapsize(size)
unsigned int size;
{
    register sector i, n;

    /* calculate the number of sectors required */
    n = 0;
    for (;;) {
	i = (size + n * sizeof(sector) + sectorsize - 1) / sectorsize;
	if (n == i) {
	    return n;
	}
	n = i;
    }
}

/*
 * NAME:	swap->count()
 * DESCRIPTION:	return the number of sectors presently in use
 */
sector sw_count()
{
    return nsectors - nfree;
}


typedef struct {
    Uint secsize;		/* size of swap sector */
    sector nsectors;		/* # sectors */
    sector ssectors;		/* # swap sectors */
    sector nfree;		/* # free sectors */
    sector mfree;		/* free sector list */
} dump_header;

static char dh_layout[] = "idddd";

# define CHUNKSZ	32

/*
 * NAME:	swap->copy()
 * DESCRIPTION:	copy sectors from dumpfile to swapfile
 */
void sw_copy()
{
    if (dump >= 0) {
	if (dsectors > 0) {
	    register sector n;

	    n = CHUNKSZ;
	    if (n > dsectors) {
		n = dsectors;
	    }
	    copy(ssectors, n);
	    ssectors += n;
	}
	if (dsectors == 0) {
	    P_close(dump);
	    dump = -1;
	}
    }
}

/*
 * NAME:	swap->dump()
 * DESCRIPTION:	dump swap file
 */
int sw_dump(dumpfile)
char *dumpfile;
{
    register header *h;
    register sector sec;
    char buffer[STRINGSZ + 4], buf1[STRINGSZ], buf2[STRINGSZ], *p, *q;
    register sector n;
    dump_header dh;

    if (dump >= 0) {
	if (dsectors > 0) {
	    /* copy remaining dump sectors */
	    n = dsectors;
	    copy(ssectors, n);
	    ssectors += n;
	}
	P_close(dump);
    }
    p = path_native(buf1, dumpfile);
    sprintf(buffer, "%s.old", dumpfile);
    q = path_native(buf2, buffer);
    P_unlink(q);
    P_rename(p, q);
    if (swap < 0) {
	sw_create();
    }

    /* flush the cache and adjust sector map */
    for (h = last; h != (header *) NULL; h = h->prev) {
	sec = h->swap;
	if (h->dirty) {
	    /*
	     * Dump the sector to swap file
	     */
	    if (sec == SW_UNUSED) {
		/*
		 * allocate new sector in swap file
		 */
		if (sfree == SW_UNUSED) {
		    sec = ssectors++;
		} else {
		    sec = sfree;
		    sfree = smap[sec];
		}
		h->swap = sec;
	    }
	    P_lseek(swap, (sec + 1L) * sectorsize, SEEK_SET);
	    if (P_write(swap, (char *) (h + 1), sectorsize) < 0) {
		fatal("cannot write swap file");
	    }
	}
	map[h->sec] = sec;
    }

    /* move to dumpfile */
    P_close(swap);
    q = path_native(buf2, swapfile);
    if (P_rename(q, p) < 0) {
	/*
	 * The rename failed.  Attempt to copy the dumpfile instead.
	 * This will take a long, long while, so keep the swapfile and
	 * dumpfile on the same file system if at all possible.
	 */
	swap = P_open(q, O_RDWR | O_BINARY, 0);
	dump = P_open(p, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0600);
	if (swap < 0 || dump < 0) {
	    fatal("cannot move swap file");
	}
	/* copy initial sector */
	if (P_read(swap, cbuf, sectorsize) <= 0) {
	    fatal("cannot read swap file");
	}
	if (P_write(dump, cbuf, sectorsize) < 0) {
	    fatal("cannot write dump file");
	}
	/* copy swap sectors */
	for (n = ssectors; n > 0; --n) {
	    if (P_read(swap, cbuf, sectorsize) <= 0) {
		fatal("cannot read swap file");
	    }
	    if (P_write(dump, cbuf, sectorsize) < 0) {
		fatal("cannot write dump file");
	    }
	}
    } else {
	/*
	 * The rename succeeded; reopen the new dumpfile.
	 */
	dump = P_open(p, O_RDWR | O_BINARY, 0);
	if (dump < 0) {
	    fatal("cannot reopen dump file");
	}
	swap = -1;
    }

    /* write header */
    dh.secsize = sectorsize;
    dh.nsectors = nsectors;
    dh.ssectors = ssectors;
    dh.nfree = nfree;
    dh.mfree = mfree;
    P_lseek(dump, sectorsize - (long) sizeof(dump_header), SEEK_SET);
    if (P_write(dump, (char *) &dh, sizeof(dump_header)) < 0) {
	fatal("cannot write swap header to dump file");
    }

    /* write map */
    P_lseek(dump, (ssectors + 1L) * sectorsize, SEEK_SET);
    if (P_write(dump, (char *) map, nsectors * sizeof(sector)) < 0) {
	fatal("cannot write sector map to dump file");
    }


    if (swap < 0) {
	/*
	 * the swapfile was moved
	 */

	/* create bitmap */
	dsectors = nsectors;
	dcursec = 0;
	memset(bmap, -1, (dsectors + 7) >> 3);
	for (sec = mfree; sec != SW_UNUSED; sec = map[sec]) {
	    BCLR(bmap, sec);
	    --dsectors;
	}

	/* fix the sector map and bitmap */
	for (h = last; h != (header *) NULL; h = h->prev) {
	    map[h->sec] = ((long) h - (long) mem) / slotsize;
	    h->swap = SW_UNUSED;
	    h->dirty = TRUE;
	    BCLR(bmap, h->sec);
	    --dsectors;
	}

	ssectors = 0;
	sfree = SW_UNUSED;
    } else {
	/*
	 * the swapfile was copied
	 */

	/* fix the sector map */
	for (h = last; h != (header *) NULL; h = h->prev) {
	    map[h->sec] = ((long) h - (long) mem) / slotsize;
	    h->dirty = FALSE;
	}
    }

    return dump;
}

/*
 * NAME:	swap->restore()
 * DESCRIPTION:	restore dump file
 */
void sw_restore(fd, secsize)
int fd;
unsigned int secsize;
{
    dump_header dh;

    /* restore swap header */
    P_lseek(fd, (long) secsize - (conf_dsize(dh_layout) & 0xff), SEEK_SET);
    conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);
    if (dh.secsize != secsize || dh.nsectors > swapsize) {
	error("Wrong sector size or too many sectors in restore file");
    }
    restoresecsize = secsize;

    /* seek beyond swap sectors */
    P_lseek(fd, (dh.ssectors + 1L) * secsize, SEEK_SET);

    /* restore swap map */
    conf_dread(fd, (char *) smap, "d", (Uint) dh.nsectors);

    restore = fd;
}
