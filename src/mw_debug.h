
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

#ifndef _MW_DEBUG_H
#define _MW_DEBUG_H


#include <stdarg.h>
#include <glib.h>

#include "mw_common.h"


/** replaces NULL strings with "(null)". useful for printf where
    you're unsure that the %s will be non-NULL. Note that while the
    linux printf will do this automatically, not all will. The others
    will instead segfault */
#define NSTR(str) ((str)? (str): "(null)")


#ifndef g_debug
#define g_debug(format...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format)
#endif


#ifndef g_info
#define g_info(format...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, format)
#endif


/** define DEBUG to have buf printed in hex pairs to stdout */
void pretty_print(const char *buf, gsize len);


/** define DEBUG to have struct mwOpaque *opaque printed in hex pairs
    to stdout */
#define pretty_opaque(opaque) \
  pretty_print((opaque)->data, (opaque)->len)


#ifndef MW_MAILME_ADDRESS
#define MW_MAILME_ADDRESS  "meanwhile-devel@lists.sourceforge.net"
#endif


#ifndef MW_MAILME_CUT_START
#define MW_MAILME_CUT_START  "-------- begin copy --------\n"
#endif


#ifndef MW_MAILME_CUT_STOP
#define MW_MAILME_CUT_STOP   "--------- end copy ---------\n"
#endif


#ifndef MW_MAILME_MESSAGE
#define MW_MAILME_MESSAGE "\n" \
 "  Greetings! It seems that you've run across protocol data that the\n" \
 "Meanwhile library does not yet know about. As such, there may be\n"    \
 "some unexpected behaviour in this session. If you'd like to help\n"    \
 "resolve this issue, please copy and paste the following block into\n"  \
 "an email to the address listed below with a brief explanation of\n"    \
 "what you were doing at the time of this message. Thanks a lot!\n"      \
 "  Please send mail to: " MW_MAILME_ADDRESS
#endif


void mw_debug_mailme_v(struct mwOpaque *block, const char *info, va_list args);


void mw_debug_mailme(struct mwOpaque *block, const char *info, ...);


#endif

