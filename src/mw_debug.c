
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
#include <glib/gprintf.h>

#include "mw_debug.h"


#define FRM               "%02x"
#define FRMT              "%02x%02x "
#define BUF(n)            ((unsigned char) buf[n])
#define ADVANCE(b, n, c)  {b += c; n -= c;}


#ifdef DEBUG
static char *t_pretty_print(const char *buf, gsize len) {
  char *ret, *tmp;

  ret = g_strdup("\n");

  while(len) {
    if(len >= 16) {
      char part[42]; part[41] = '\0';

      sprintf(part,
	      FRMT FRMT FRMT FRMT FRMT FRMT FRMT FRMT "\n",
	      BUF(0),  BUF(1),  BUF(2),  BUF(3),
	      BUF(4),  BUF(5),  BUF(6),  BUF(7),
	      BUF(8),  BUF(9),  BUF(10), BUF(11),
	      BUF(12), BUF(13), BUF(14), BUF(15));
      ADVANCE(buf, len, 16);
      tmp = g_strconcat(ret, part, NULL);
      
    } else if(len > 1) {
      char part[6]; part[5] = '\0';

      sprintf(part, FRMT, BUF(0), BUF(1));
      ADVANCE(buf, len, 2);
      tmp = g_strconcat(ret, part, NULL);
      
    } else {
      char part[4]; part[3] = '\0';

      sprintf(part, FRM "\n", BUF(0));
      ADVANCE(buf, len, 1);
      tmp = g_strconcat(ret, part, NULL);
    }

    g_free(ret);
    ret = tmp;
  }

  return ret;
}
#endif


void pretty_print(const char *buf, gsize len) {
#ifdef DEBUG
  char *p = t_pretty_print(buf, len);
  g_debug(p);
  g_free(p);
#endif
  ;
}


void mw_debug_mailme_v(struct mwOpaque *block,
		       const char *info, va_list args) {
  /*
    MW_MAILME_MESSAGE
    begin here
    info % args
    pretty_print
    end here
  */

#ifdef DEBUG
  char *txt, *msg;

  txt = g_strdup_vprintf(info, args);

  if(block) {
    char *dat;

    dat = t_pretty_print(block->data, block->len);
    msg = g_strconcat(MW_MAILME_MESSAGE MW_MAILME_CUT_START,
		      txt, dat, MW_MAILME_CUT_STOP, NULL);
    g_free(dat);

  } else {
    msg = g_strconcat(MW_MAILME_MESSAGE MW_MAILME_CUT_START,
		      txt, MW_MAILME_CUT_STOP, NULL);
  }

  g_debug(msg);
  g_free(txt);
  g_free(msg);
#endif
  ;
}


void mw_debug_mailme(struct mwOpaque *block,
		     const char *info, ...) {
  va_list args;
  va_start(args, info);
  mw_debug_mailme_v(block, info, args);
  va_end(args);
}

