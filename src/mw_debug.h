
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


#include <glib.h>


/** replaces NULL strings with "(null)". useful for printf where
    you're unsure that the %s will be non-NULL. Note that while the
    linux printf will do this automatically, not all will. The others
    will instead segfault */
#define NSTR(str) ((str)? (str): "(null)")


#define g_debug(format...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format)
#define g_info(format...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, format)


/** define DEBUG to have buf printed in hex pairs to stdout */
void pretty_print(const char *buf, gsize len);

#define pretty_opaque(opaque) \
  pretty_print((opaque)->data, (opaque)->len)


#endif

