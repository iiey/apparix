/*   (C) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef tingea_inttypes_h
#define tingea_inttypes_h

#include <limits.h>
#include <sys/types.h>


#if UINT_MAX >= 4294967295
#  define MCX_UINT32 unsigned int
#  define MCX_INT32  int
#else
#  define MCX_UINT32 unsigned long
#  define MCX_INT32  long
#endif

typedef  MCX_UINT32     u32 ;       /* at least 32 bits */
typedef  MCX_INT32      i32 ;       /* at least 32 bits */
typedef  unsigned char  u8  ;       /* at least  8 bits */

#ifndef ulong
#  define ulong unsigned long
#endif

#if 0
#  define  dim          size_t
#  define  ofs         ssize_t
#else
   typedef  size_t         dim;
   typedef  ssize_t        ofs;
#endif

                     /* annotate 'unsigned due to prototype'
                      * and related messages
                     */
#define different_sign
#define different_width


#endif


