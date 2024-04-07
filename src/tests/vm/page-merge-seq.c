/* Generates about 1 MB of random data that is then divided into
   16 chunks.  A separate subprocess sorts each chunk in
   sequence.  Then we merge the chunks and verify that the result
   is what it should be. */

#include <syscall.h>
#include "tests/arc4.h"
#include "tests/lib.h"
#include "tests/main.h"
#include <stdio.h>

/* This is the max file size for an older version of the Pintos
   file system that had 126 direct blocks each pointing to a
   single disk sector.  We could raise it now. */
#define CHUNK_SIZE (126 * 512)
#define CHUNK_CNT 16                            /* Number of chunks. */
#define DATA_SIZE (CHUNK_CNT * CHUNK_SIZE)      /* Buffer size. */

unsigned char buf1[DATA_SIZE], buf2[DATA_SIZE];
size_t histogram[256];

/* Initialize buf1 with random data,
   then count the number of instances of each value within it. */
static void
init (void) 
{
  struct arc4 arc4;
  size_t i;

  msg ("init");

  arc4_init (&arc4, "foobar", 6);
  arc4_crypt (&arc4, buf1, sizeof buf1);
  for (i = 0; i < sizeof buf1; i++)
    histogram[buf1[i]]++;
}

/* Sort each chunk of buf1 using a subprocess. */
static void
sort_chunks (void)
{
  size_t i;

  create ("buffer", CHUNK_SIZE);
  for (i = 0; i < CHUNK_CNT; i++) 
    {
      pid_t child;
      int handle;

      msg ("sort chunk %zu", i);

      /* Write this chunk to a file. */
      quiet = true;
      CHECK ((handle = open ("buffer")) > 1, "open \"buffer\"");
      write (handle, buf1 + CHUNK_SIZE * i, CHUNK_SIZE);
      close (handle);

      /* Sort with subprocess. */
      CHECK ((child = exec ("child-sort buffer")) != -1,
             "exec \"child-sort buffer\"");
      CHECK (wait (child) == 123, "wait for child-sort");

      /* Read chunk back from file. */
      CHECK ((handle = open ("buffer")) > 1, "open \"buffer\"");
      read (handle, buf1 + CHUNK_SIZE * i, CHUNK_SIZE);
      close (handle);

      unsigned char prev = 0;
      for (size_t j = 0; j < CHUNK_SIZE; ++j) {
        unsigned char val = buf1[CHUNK_SIZE * i + j];
        if (val < prev) {
          printf ("sort_chunks failed, chunk: %ld, index: %ld, prev: %d, val: %dn", i, j, prev, val);
        }
        prev = val;
      }

      quiet = false;
    }
}

/* Merge the sorted chunks in buf1 into a fully sorted buf2. */
static void
merge (void) 
{
  unsigned char *mp[CHUNK_CNT];
  size_t mp_left;
  unsigned char *op;
  size_t i;

  msg ("merge");

  for (i = 0; i < CHUNK_CNT; i++) {
    printf ("check chunk %ld\n", i);
    unsigned char prev = 0;
    for (size_t j = 0; j < CHUNK_SIZE; ++j) {
      unsigned char val = buf1[CHUNK_SIZE * i + j];
      if (val < prev) {
        printf ("merge chunk failed, chunk: %ld, index: %ld, val: %d, prev: %d\n", val, prev, i, j);
      } else if (val > prev) {
        // printf ("bump found: %d\n", val);
      }
      prev = val;
    }
  }

  /* Initialize merge pointers. */
  mp_left = CHUNK_CNT;
  for (i = 0; i < CHUNK_CNT; i++)
    mp[i] = buf1 + CHUNK_SIZE * i;

  /* Merge. */
  op = buf2;
  while (mp_left > 0) 
    {
      /* Find smallest value. */
      size_t min = 0;
      for (i = 1; i < mp_left; i++)
        if (*mp[i] < *mp[min])
          min = i;

      /* Append value to buf2. */
      *op++ = *mp[min];

      /* Advance merge pointer.
         Delete this chunk from the set if it's emptied. */ 
      if ((++mp[min] - buf1) % CHUNK_SIZE == 0)
        mp[min] = mp[--mp_left]; 
    }


  unsigned char prev = 0;
  printf ("check buf2\n");
  printf ("buf2[0]: %d\n", buf2[0]);
  for (size_t j = 0; j < DATA_SIZE; ++j) {
    unsigned char val = buf2[j];
    if (val < prev) {
      printf ("sort failed, val: %d, prev: %d, j: %d\n", val, prev, j);
    } else if (val > prev) {
      printf ("bump found: %d\n", val);
    }
    prev = val;
  }
}

static void
verify (void) 
{
  size_t buf_idx;
  size_t hist_idx;

  msg ("verify");

  buf_idx = 0;
  for (hist_idx = 0; hist_idx < sizeof histogram / sizeof *histogram;
       hist_idx++)
    {
      printf ("hist_idx: %d\n", hist_idx);
      printf ("histogram[hist_idx]: %d\n", histogram[hist_idx]);
      printf ("sizeof histogram / sizeof *histogram: %d\n", sizeof histogram / sizeof *histogram);
      while (histogram[hist_idx]-- > 0) 
        {
          // printf ("buf_idx: %d\n", buf_idx);
          if (buf2[buf_idx] != hist_idx)
            fail ("bad value %d in byte %zu", buf2[buf_idx], buf_idx);
          buf_idx++;
        } 
    }

  msg ("success, buf_idx=%'zu", buf_idx);
}

void
test_main (void)
{
  init ();
  sort_chunks ();
  merge ();
  verify ();
}
