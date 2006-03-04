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


#include <glib.h>
#include <string.h>
#include "mw_config.h"
#include "mw_debug.h"
#include "mw_parser.h"


struct mw_parser {

  /** current state of the parser */
  enum {
    parser_TRIM = 0,
    parser_LENGTH,
    parser_DATA,
  } state;

  guchar *buf;  /**< buffer of incomplete message */
  gsize len;    /**< length allocated to buffer */
  gsize msg;    /**< length of message when complete */
  gsize use;    /**< length of message received so far */
  
  /** continuation to be triggered on message completion */
  MwParserCallback cb;
  gpointer data;
};


static guint32 uint32_read(guchar *b) {
  guint32 val;
  
  val = (*(b)++ & 0xff) << 0x18;
  val = val | (*(b)++ & 0xff) << 0x10;
  val = val | (*(b)++ & 0xff) << 0x08;
  val = val | (*(b)++ & 0xff);

  return val;
}


static void parser_cont_call(MwParser *p) {
  p->cb(p, p->buf, p->msg, p->data);
}


/** skip past leading counter and keepalive bytes. once complete, sets
    the state to parser_LENGTH */
static void parser_trim(MwParser *p, guchar **buf, gsize *len) {
  guchar *b = *buf;
  gsize l = *len;

  while(l && *b & 0x80) {
    b++; l--;
  }

  if(l) {
    p->state = parser_LENGTH;
    p->msg = 4;
    p->use = 0;
  }

  *buf = b;
  *len = l;
}


/** fill the parser's buffer with data until full. Used by
    parser_length and parser_data */
static void parser_fill(MwParser *p, guchar **buf, gsize *len) {
  gsize needed, l = *len;

  needed = p->msg - p->use;
  if(needed < l) l = needed;

  memcpy(p->buf + p->use, *buf, l);
  p->use += l;

  *buf += l;
  *len -= l;
}


/** reset the parser's state */
static void parser_reset(MwParser *p) {
  p->state = parser_TRIM;
  p->msg = 0;
  p->use = 0;
}


/** read the length bytes. Once complete, sets the state to
    parser_DATA */
static void parser_length(MwParser *p, guchar **buf, gsize *len) {
  parser_fill(p, buf, len);
  
  if(p->use == p->msg) {
    p->state = parser_DATA;
    p->msg = uint32_read(p->buf);
    p->use = 0;

    /* ensure there's enough buffer. Don't bother with a realloc,
       since we don't care about what's currently in the buffer. */
    if(p->len <= p->msg) {
      g_free(p->buf);
      while(p->len <= p->msg) {
	p->len += MW_DEFAULT_BUFLEN;
      }
      p->buf = g_malloc(p->len);
    }
  }
}


/** read the message body. Once complete, triggers the parser's
    continuation, then sets the state to parser_TRIM to prepare for
    the next message */
static void parser_data(MwParser *p, guchar **buf, gsize *len) {
  parser_fill(p, buf, len);
  
  if(p->use == p->msg) {
    parser_cont_call(p);
    parser_reset(p);
  }
}


MwParser *MwParser_new(MwParserCallback cb, gpointer data) {
  MwParser *p;

  g_return_val_if_fail(cb != NULL, NULL);

  p = g_new0(MwParser, 1);
  p->cb = cb;
  p->data = data;

  p->buf = g_malloc(MW_DEFAULT_BUFLEN);
  p->len = MW_DEFAULT_BUFLEN;

  return p;
}


void MwParser_feed(MwParser *parser, const guchar *buf, gsize len) {
  g_return_if_fail(parser != NULL);
  if(len > 0) g_return_if_fail(buf != NULL);
  
  while(len > 0) {
    switch(parser->state) {
    case parser_TRIM:
      parser_trim(parser, (guchar **) &buf, &len);
      break;
    case parser_LENGTH:
      parser_length(parser, (guchar **) &buf, &len);
      break;
    case parser_DATA:
      parser_data(parser, (guchar **) &buf, &len);
      break;
    }
  }
}


void MwParser_free(MwParser *p) {
  if(! p) return;
  parser_reset(p);
  g_free(p->buf);
  g_free(p);
}


/* The end. */
