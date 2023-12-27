#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

struct fixed_point
  {
    int integer;  /* 17.14 fixed-point number representation */
  };

void fixed_point_init(struct fixed_point *fp, int value);
void fixed_point_copy(struct fixed_point *dst, struct fixed_point *src);
void fixed_point_add_int(struct fixed_point *fp, int value);
void fixed_point_multiply(struct fixed_point *fp1, struct fixed_point *fp2);
void fixed_point_multiply_int(struct fixed_point *fp, int value);
void fixed_point_divide(struct fixed_point *fp1, struct fixed_point *fp2);
void fixed_point_divide_int(struct fixed_point *fp, int value);
int fixed_point_to_int(struct fixed_point *fp);

#endif /* threads/fixed-point.h */
