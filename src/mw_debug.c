
/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004  Christopher (siege) O'Brien
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>

#include "mw_debug.h"


#ifdef DEBUG

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


