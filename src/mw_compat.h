

#ifndef _MW_COMPAT_H
#define _MW_COMPAT_H

/** @file compat.h */

/** @todo: I may add more compatability stuff for legacy GLib 2.0 as I
    begin to indroduce more 2.2 deps */

#if GLIB_COMPAT
# include <glib/gprintf.h>

#else
# include <stdio.h>
# define  g_sprintf sprintf
#endif


#endif
