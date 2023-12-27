#include "threads/fixed-point.h"
#include <stdint.h>

#define FIXED_POINT_CONVERTER (1 << 14)

void
fixed_point_init(struct fixed_point *fp, int value)
{
  fp->integer = value * FIXED_POINT_CONVERTER;
}

void
fixed_point_copy(struct fixed_point *dst, struct fixed_point *src)
{
  dst->integer = src->integer;
}

void
fixed_point_add_int(struct fixed_point *fp, int value)
{
  fp->integer += value * FIXED_POINT_CONVERTER;
}

void
fixed_point_multiply(struct fixed_point *fp1, struct fixed_point *fp2)
{
  fp1->integer = ((int64_t) fp1->integer) * fp2->integer / FIXED_POINT_CONVERTER;
}

void
fixed_point_multiply_int(struct fixed_point *fp, int value)
{
  fp->integer *= value;
}

void
fixed_point_divide(struct fixed_point *fp1, struct fixed_point *fp2)
{
  fp1->integer = ((int64_t) fp1->integer) * FIXED_POINT_CONVERTER / fp2->integer;
}

void
fixed_point_divide_int(struct fixed_point *fp, int value)
{
  fp->integer /= value;
}

int
fixed_point_to_int(struct fixed_point *fp)
{
  return fp->integer / FIXED_POINT_CONVERTER;
}