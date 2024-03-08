#include "swap_table.h"
#include <hash.h>

static struct hash swap_hash;

static unsigned swap_table_hash_func (const struct hash_elem *e, void *aux)
{
  // TODO: implement
  return 0;
}

static bool swap_table_less_func (const struct hash_elem *a,
                           const struct hash_elem *b,
                           void *aux)
{
  // TODO: implement
  return false;
}

void swap_table_init (void)
{
  hash_init (&swap_hash, &swap_table_hash_func, &swap_table_less_func, NULL);
}
