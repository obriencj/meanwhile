
#ifndef _MW_DEBUG_H_
#define _MW_DEBUG_H_



/**
  debug_printf is a printf that happens only when debugging is compiled on.
*/
void debug_printf(const char *v, ...);


/**
  pretty_print is like debug_printf in that it only does something when
  debugging is compiled on, 
*/
void pretty_print(const char *buf, unsigned int len);


#endif
