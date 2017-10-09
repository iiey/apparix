/*   (C) Copyright 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/

#ifndef util_err_h
#define util_err_h

#include <stdio.h>


enum
{  MCX_LOG_DEBUG1  = -5    /* whatever you have */
,  MCX_LOG_DEBUG2          /* do not be shy */
,  MCX_LOG_DEBUG3          /* default */
,  MCX_LOG_DEBUG4          /* summarize */
,  MCX_LOG_DEBUG5          /* summarize all the way  */

,  MCX_LOG_INFORM1 =  0
,  MCX_LOG_INFORM2
,  MCX_LOG_INFORM3
,  MCX_LOG_INFORM4
,  MCX_LOG_INFORM5
                           /* strange parameters
                            * underflow
                            * incomplete specifications
                           */
,  MCX_LOG_WARN1   =  5
,  MCX_LOG_WARN2
,  MCX_LOG_WARN3
,  MCX_LOG_WARN4
,  MCX_LOG_WARN5

,  MCX_LOG_ERR1    = 10   /* errors, unexpected things may happen */
,  MCX_LOG_ERR2
,  MCX_LOG_ERR3
,  MCX_LOG_ERR4
,  MCX_LOG_ERR5

,  MCX_LOG_PANIC1  = 15    /* inconsistent internal state */
,  MCX_LOG_PANIC2
,  MCX_LOG_PANIC3
,  MCX_LOG_PANIC4
,  MCX_LOG_PANIC5

,  MCX_LOG_IGNORE  = 20
}  ;



void  mcxTell
(  const char  *caller
,  const char  *fmt
,  ...
)  
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
   ;


void  mcxTellf
(  FILE*       fp
,  const char  *caller
,  const char  *fmt
,  ...
)  
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
   ;

void  mcxWarn
(  const char  *caller
,  const char  *fmt
,  ...
)
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
   ;

void  mcxErr
(  const char  *caller
,  const char  *fmt
,  ...
)  
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
   ;

void  mcxErrf
(  FILE*       fp
,  const char  *caller
,  const char  *fmt
,  ...
)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
   ;

void  mcxDie
(  int status
,  const char  *caller
,  const char  *fmt
,  ...
)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
   ;

void mcxTellFile
(  FILE* fp
)  ;

void mcxWarnFile
(  FILE* fp
)  ;

void mcxErrorFile
(  FILE* fp
)  ;

void mcxFail
(  void
)  ;

void mcxExit
(  int val
)  ;

#endif

