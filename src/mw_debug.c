

#include <stdio.h>
#include "mw_debug.h"


#if (MW_DEBUG == 1)
#include <stdarg.h>

void debug_printf(const char *c, ...) {
  va_list args;
  va_start(args, c);
  vprintf(c, args);
  va_end(args);
}

#else

void debug_printf(const char *c, ...) { ; }

#endif


#if (MW_PRETTY == 1)

#define frmt "%02x%02x "
#define buf(n) (unsigned char)buf[n] 

void pretty_print(const char *buf, unsigned int len) {
  while(len > 0) {
    if(len >= 16) {
      printf(frmt frmt frmt frmt frmt frmt frmt frmt "\n",
             buf(0), buf(1), buf(2), buf(3),
             buf(4), buf(5), buf(6), buf(7),
             buf(8), buf(9), buf(10), buf(11),
             buf(12), buf(13), buf(14), buf(15));
      buf += 16;
      len -= 16;
             
    } else if(len >= 2) {
      printf(frmt, buf(0), buf(1));
      buf += 2;
      len -= 2;
      if(!len) putchar('\n');

    } else {
      printf("%02x", buf(0));
      buf++;
      len--;
      if(!len) putchar('\n');
    }
  }
}

#else

void pretty_print(const char *buf, unsigned int len) { ; }

#endif


