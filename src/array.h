struct _array_ {
    unsigned short size;		/* number of elements */
    Uint ref;				/* number of references */
    Uint tag;				/* used in sorting */
    Uint odcount;			/* last destructed object count */
    value *elts;			/* elements */
    struct _maphash_ *hashed;		/* hashed mapping elements */
    struct _arrref_ *primary;		/* primary reference */
};

typedef struct _abchunk_ abchunk;	/* array backup chunk */

extern void		arr_init	P((unsigned int));
extern array	       *arr_alloc	P((unsigned int));
extern array	       *arr_new		P((dataspace*, long));
# define arr_ref(a)	((a)->ref++)
extern void		arr_del		P((array*));
extern void		arr_freeall	P((void));

extern uindex		arr_put		P((array*));
extern void		arr_clear	P((void));

extern void		arr_backup	P((abchunk**, array*, plane*));
extern void		arr_commit	P((abchunk**, plane*));
extern void		arr_restore	P((abchunk**));

extern array	       *arr_add		P((dataspace*, array*, array*));
extern array	       *arr_sub		P((dataspace*, array*, array*));
extern array	       *arr_intersect	P((dataspace*, array*, array*));
extern array	       *arr_setadd	P((dataspace*, array*, array*));
extern array	       *arr_setxadd	P((dataspace*, array*, array*));
extern unsigned short	arr_index	P((array*, long));
extern void		arr_ckrange	P((array*, long, long));
extern array	       *arr_range	P((dataspace*, array*, long, long));

extern array	       *map_new		P((dataspace*, long));
extern void		map_sort	P((array*));
extern void		map_compact	P((array*));
extern unsigned short	map_size	P((array*));
extern array	       *map_add		P((dataspace*, array*, array*));
extern array	       *map_sub		P((dataspace*, array*, array*));
extern array	       *map_intersect	P((dataspace*, array*, array*));
extern value	       *map_index	P((dataspace*, array*, value*, value*));
extern array	       *map_range	P((dataspace*, array*, value*, value*));
extern array	       *map_indices	P((dataspace*, array*));
extern array	       *map_values	P((dataspace*, array*));
