
#include <stdio.h>
#include "mw_debug.h"


#define MW_PRETTY 1


#if (MW_PRETTY == 1)

#define FRM               "%02x"
#define FRMT              "%02x%02x "
#define BUF(n)            ((unsigned char) buf[n])
#define ADVANCE(b, n, c)  {b += c; n -= c;}

void pretty_print(const char *buf, gsize len) {
  while(len) {
    if(len >= 16) {
      printf(FRMT FRMT FRMT FRMT FRMT FRMT FRMT FRMT "\n",
             BUF(0),  BUF(1),  BUF(2),  BUF(3),
             BUF(4),  BUF(5),  BUF(6),  BUF(7),
             BUF(8),  BUF(9),  BUF(10), BUF(11),
             BUF(12), BUF(13), BUF(14), BUF(15));
      ADVANCE(buf, len, 16);
      continue;
      
    } else if(len > 1) {
      printf(FRMT, BUF(0), BUF(1));
      ADVANCE(buf, len, 2);

    } else {
      printf(FRM, BUF(0));
      ADVANCE(buf, len, 1);
    }

    if(! len) putchar('\n');
  }
}

#else

void pretty_print(const char *buf, gsize len) { ; }

#endif


