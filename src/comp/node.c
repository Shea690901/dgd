# include "comp.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "lex.h"
# include "node.h"

# define NODE_CHUNK	64

typedef struct _nodelist_ {
    node n[NODE_CHUNK];			/* node array */
    struct _nodelist_ *next;		/* next in linked list */
} nodelist;

static nodelist *list;			/* linked list of all node chunks */
static nodelist *flist;			/* list of free node chunks */
static int chunksize = NODE_CHUNK;	/* part of list chunk used */

/*
 * NAME:	node->new()
 * DESCRIPTION:	create a new node
 */
static node *node_new()
{
    register node *n;

    if (chunksize == NODE_CHUNK) {
	register nodelist *l;

	if (flist != (nodelist *) NULL) {
	    l = flist;
	    flist = l->next;
	} else {
	    l = ALLOC(nodelist, 1);
	}
	l->next = list;
	list = l;
	chunksize = 0;
    }
    n = &list->n[chunksize++];
    n->line = tk_line();
    return n;
}

/*
 * NAME:	node->int()
 * DESCRIPTION:	create an integer node
 */
node *node_int(num)
Int num;
{
    register node *n;

    n = node_new();
    n->type = N_INT;
    n->mod = T_NUMBER;
    n->l.number = num;
    n->r.number = num;

    return n;
}

/*
 * NAME:	node->str()
 * DESCRIPTION:	create a string node
 */
node *node_str(str)
string *str;
{
    register node *n;

    n = node_new();
    n->type = N_STR;
    n->mod = T_STRING;
    str_ref(n->l.string = str);
    n->r.right = (node *) NULL;

    return n;
}

/*
 * NAME:	node->fcall()
 * DESCRIPTION:	create a character pointer node
 */
node *node_fcall(mod, func, call)
int mod;
char *func;
long call;
{
    register node *n;

    n = node_new();
    n->type = N_FUNC;
    n->mod = mod;
    n->l.ptr = func;
    n->r.number = call;

    return n;
}

/*
 * NAME:	node->mon()
 * DESCRIPTION:	create a node for a monadic operator
 */
node *node_mon(type, mod, left)
int type, mod;
node *left;
{
    register node *n;

    n = node_new();
    n->type = type;
    n->mod = mod;
    n->l.left = left;
    n->r.right = (node *) NULL;

    return n;
}

/*
 * NAME:	node->bin()
 * DESCRIPTION:	create a node for a binary operator
 */
node *node_bin(type, mod, left, right)
int type, mod;
node *left, *right;
{
    register node *n;

    n = node_new();
    n->type = type;
    n->mod = mod;
    n->l.left = left;
    n->r.right = right;

    return n;
}

/*
 * NAME:	node->free()
 * DESCRIPTION:	free all nodes
 */
void node_free()
{
    register nodelist *l;
    register int i;

    i = chunksize;
    for (l = list; l != (nodelist *) NULL; ) {
	register node *n;
	register nodelist *f;

	n = &l->n[i];
	do {
	    --n;
	    if (n->type == N_STR) {
		/*
		 * only strings are deleted here
		 */
		str_del(n->l.string);
	    }
	} while (--i > 0);
	i = NODE_CHUNK;
	f = l;
	l = l->next;
	f->next = flist;
	flist = f;
    }
    list = (nodelist *) NULL;
    chunksize = NODE_CHUNK;
}

/*
 * NAME:	node->clear()
 * DESCRIPTION:	cleanup after node handling
 */
void node_clear()
{
    register nodelist *l;

    node_free();
    for (l = flist; l != (nodelist *) NULL; ) {
	register nodelist *f;

	f = l;
	l = l->next;
	FREE(f);
    }
    flist = (nodelist *) NULL;
}