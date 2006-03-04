#ifndef _MW_PARSER_H
#define _MW_PARSER_H


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


/**
   @file mw_parser.h
   @since 2.0.0
   @author siege@preoccupied.net

   This file provides a simple parser utility structure. The parser
   can be fed data asynchronously. As Sametime messages are completed,
   a user-defined continuation will be triggered to perform and
   handling of the message that is necessary.
*/


#include <glib.h>


G_BEGIN_DECLS


typedef struct mw_parser MwParser;


typedef void (*MwParserCallback)(MwParser *parser,
				 const guchar *buf, gsize len,
				 gpointer user_data);


/** @struct mw_parser
    @since 2.0.0

    parser state, retains a buffer of incomplete messages and a pointer
    to its continuation */
struct mw_parser;


/** @since 2.0.0

    create a new parser which will use the given continuation

    @param cont the continuation to be called as the parser completes
    a message
    @return newly allocated parser state
 */
MwParser *MwParser_new(MwParserCallback cb, gpointer user_data);


/** @since 2.0.0

    feed a parser with data, triggering the parser's continuation as
    full messages are discovered.

    @param parser the parser state to feed
    @param buf    data to feed to the parser
    @param len    length of buf
 */
void MwParser_feed(MwParser *parser, const guchar *buf, gsize len);


/** @since 2.0.0

    destroy a parser

    @param p the parser state to clear and free
 */
void MwParser_free(MwParser *p);


G_END_DECLS


#endif /* _MW_PARSER_H */
