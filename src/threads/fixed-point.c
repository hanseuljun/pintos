#include "threads/fixed-point.h"
#include <stdint.h>
#include <stdio.h>

#define FIXED_POINT_CONVERTER (1 << 14)

void
fixed_point_init (struct fixed_point *fp, int value)
{
  fp->bits = value * FIXED_POINT_CONVERTER;
}

void
fixed_point_copy (struct fixed_point *fp1, struct fixed_point *fp2)
{
  fp1->bits = fp2->bits;
}

struct fixed_point fixed_point_create (int value)
{
  struct fixed_point fp;
  fixed_point_init (&fp, value);
  return fp;
}

struct fixed_point
fixed_point_get_copied (struct fixed_point *fp)
{
  struct fixed_point new_fp;
  fixed_point_copy (&new_fp, fp);
  return new_fp;
}

void
fixed_point_add_int (struct fixed_point *fp, int value)
{
  fp->bits += value * FIXED_POINT_CONVERTER;
}

void
fixed_point_substract (struct fixed_point *fp1, struct fixed_point *fp2)
{
  fp1->bits -= fp2->bits;
}

void
fixed_point_multiply (struct fixed_point *fp1, struct fixed_point *fp2)
{
  fp1->bits = ((int64_t) fp1->bits) * fp2->bits / FIXED_POINT_CONVERTER;
}

void
fixed_point_multiply_int (struct fixed_point *fp, int value)
{
  fp->bits *= value;
}

struct fixed_point
fixed_point_get_multiplied_int (struct fixed_point *fp, int value)
{
  struct fixed_point new_fp = fixed_point_get_copied (fp);
  fixed_point_multiply_int (&new_fp, value);
  return new_fp;
}

void
fixed_point_divide (struct fixed_point *fp1, struct fixed_point *fp2)
{
  fp1->bits = ((int64_t) fp1->bits) * FIXED_POINT_CONVERTER / fp2->bits;
}

void
fixed_point_divide_int (struct fixed_point *fp, int value)
{
  fp->bits /= value;
}

struct fixed_point
fixed_point_get_divided_int (struct fixed_point *fp, int value)
{
  struct fixed_point new_fp = fixed_point_get_copied (fp);
  fixed_point_divide_int (&new_fp, value);
  return new_fp;
}

int
fixed_point_to_int (struct fixed_point *fp)
{
  return fp->bits / FIXED_POINT_CONVERTER;
}
