#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static void *stack = NULL;
static int  nstack, nstack_a, item_size;

/* Token stack */
int stack_create (int n, int size)
{
    if (stack != NULL) free (stack);

    item_size = size;
    nstack = 0;
    nstack_a = n;
    stack = malloc (size * nstack_a);
    if (stack == NULL) return -1;

    return 0;
}

int stack_destroy (void)
{
    if (stack == NULL) return -1;

    nstack = 0;
    free (stack);
    stack = NULL;

    return 0;
}

int stack_push (void *item)
{
    void *tp;

    if (stack == NULL) return -1;

    if (nstack == nstack_a)
    {
        nstack_a *= 2;
        tp = realloc (stack, item_size * nstack_a);
        if (tp == NULL)
        {
            return -2;
        }
        stack = tp;
    }

    memcpy ((char *)stack+nstack*item_size, item, item_size);
    nstack++;

    return 0;
}

int stack_pop (void *item)
{
    if (stack == NULL) return -1;
    if (nstack == 0) return -3;

    memcpy (item, stack+(nstack-1)*item_size, item_size);
    nstack--;

    return 0;
}

int stack_peek (void *item)
{
    if (stack == NULL) return -1;
    if (nstack == 0) return -3;

    memcpy (item, stack+(nstack-1)*item_size, item_size);

    return 0;
}

#ifdef TEST

typedef struct
{
    int  type;
    char *text;
}
token_t;

int main (int argc, char *argv[])
{
    int     rc;
    token_t t1;

    rc = stack_create (10, sizeof(token_t));
    if (rc) error1 ("rc = %d after stack_create()\n", rc);
    printf ("Stack created\n");

    t1.type = 1;
    t1.text = strdup ("token 1");
    rc = stack_push (&t1);
    if (rc) error1 ("rc = %d after 1st stack_push()\n", rc);
    printf ("Pushed 'token 1'\n");

    t1.type = 2;
    t1.text = strdup ("token 2");
    rc = stack_push (&t1);
    if (rc) error1 ("rc = %d after 2nd stack_push()\n", rc);
    printf ("Pushed 'token 2'\n");

    t1.type = 0;
    t1.text = NULL;
    rc = stack_peek (&t1);
    if (rc) error1 ("rc = %d after stack_peek()\n", rc);
    printf ("Peeked '%d,%s'\n", t1.type, t1.text);

    t1.type = 0;
    t1.text = NULL;
    rc = stack_pop (&t1);
    if (rc) error1 ("rc = %d after stack_pop()\n", rc);
    printf ("Popped '%d,%s'\n", t1.type, t1.text);

    t1.type = 0;
    t1.text = NULL;
    rc = stack_pop (&t1);
    if (rc) error1 ("rc = %d after stack_pop()\n", rc);
    printf ("Popped '%d,%s'\n", t1.type, t1.text);

    rc = stack_destroy ();
    if (rc) error1 ("rc = %d after stack_destroy()\n", rc);
    printf ("Stack destroy\n");

    return 0;
}
#endif
