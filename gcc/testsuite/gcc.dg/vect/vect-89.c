/* { dg-require-effective-target vect_int } */

#include <stdarg.h>
#include "tree-vect.h"

#define N 16

struct tmp_struct
{
  int x;
  int y[N];
};
	 
int main1 ()
{  
  int i, *q;
  struct tmp_struct tmp, *p;

  p = &tmp;
  q = p->y;

  for (i = 0; i < N; i++)
    {
      *q++ = 5;
    }

  /* check results: */  
  for (i = 0; i < N; i++)
    {
      if (p->y[i] != 5)
        {
          abort ();
        }
    }

  return 0;
}

int main (void)
{ 
  check_vect ();
  
  return main1 ();
} 

/* { dg-final { scan-tree-dump-times "vectorized 1 loops" 1 "vect" } } */
