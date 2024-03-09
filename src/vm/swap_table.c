#include "swap_table.h"
#include <hash.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

struct swap_table_elem
  {
    void *upage;
    block_sector_t sector;
    struct hash_elem hash_elem;
  };

static struct hash swap_hash;
static block_sector_t next_sector;

static unsigned swap_table_hash_func (const struct hash_elem *e, void *aux)
{
  struct swap_table_elem *elem = hash_entry (e, struct swap_table_elem, hash_elem);
  return (unsigned) elem->upage;
}

static bool swap_table_less_func (const struct hash_elem *a,
                                  const struct hash_elem *b,
                                  void *aux)
{
  struct swap_table_elem *elem_a = hash_entry (a, struct swap_table_elem, hash_elem);
  struct swap_table_elem *elem_b = hash_entry (b, struct swap_table_elem, hash_elem);
  return elem_a->upage < elem_b->upage;
}

void swap_table_init (void)
{
  hash_init (&swap_hash, &swap_table_hash_func, &swap_table_less_func, NULL);
  next_sector = 0;
}

bool swap_table_contains (void *upage)
{
  // TODO: implement 
}

void swap_table_push (void *upage, void *kpage)
{
  struct block *swap_block = block_get_role (BLOCK_SWAP);

  struct swap_table_elem *elem = malloc (sizeof *elem);
  elem->upage = upage;
  elem->sector = next_sector;

  uint8_t *buffer = kpage;
  for (int i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); ++i)
    {
      // save kpage things in the swap_block
      block_write (swap_block, next_sector, buffer);
      ++next_sector;
      buffer += BLOCK_SECTOR_SIZE;
    }

  hash_insert (&swap_hash, &elem->hash_elem);
}
