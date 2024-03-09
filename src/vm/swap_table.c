#include "swap_table.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

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
  ASSERT (pg_ofs (upage) == 0);
  
  struct swap_table_elem elem_for_find;
  elem_for_find.upage = upage;

  return hash_find (&swap_hash, &elem_for_find.hash_elem) != NULL;
}

void swap_table_insert_and_save (void *upage, void *kpage)
{
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (pg_ofs (kpage) == 0);

  struct block *swap_block = block_get_role (BLOCK_SWAP);

  // TODO: Pick the sector in a more sophisticated way,
  // taking into account which were used and then freed.
  struct swap_table_elem *elem = malloc (sizeof *elem);
  elem->upage = upage;
  elem->sector = next_sector;
  hash_insert (&swap_hash, &elem->hash_elem);

  uint8_t *buffer = kpage;
  for (int i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); ++i)
    {
      // save kpage things in the swap_block
      block_write (swap_block, next_sector, buffer);
      ++next_sector;
      buffer += BLOCK_SECTOR_SIZE;
    }

}
