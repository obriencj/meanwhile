
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


/** define MW_PRETTY as 1 to have buf printed in hex pairs to stdout */
void pretty_print(const char *buf, gsize len);

#define pretty_opaque(opaque) \
  pretty_print(opaque->data, opaque->len)


#endif

