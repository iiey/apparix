/*   Copyright (C) 2005 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/


/* TODO
   -  [*a*256] leads to count 0.
   -  make * stop on range and class boundaries
   cleanup/document backlog and tbl bit manipulation.
*/

/* NOTE
   -  only room for 256 classes including sentinels C_CLASSSTART C_CLASSEND
   -  during tlt phase delete and squash bits are ignored and not respected
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "tr.h"
#include "ting.h"
#include "ding.h"
#include "err.h"


#define MAX_SPEC_SIZE 511

#define HEX1(c)  hexs[(c >> 4) & 15]
#define HEX2(c)  hexs[c & 15u]
#define UC(c)    ((unsigned char) *(c))

#define DEL_SET(tbl, i)       (tbl[i] |= (1<< 16))
#define DEL_GET(tbl, i)       (tbl[i] &  (1<< 16))
#define SQS_SET(tbl, i)       (tbl[i] |= (1<< 17))
#define SQS_GET(tbl, i)       (tbl[i] &  (1<< 17))

#define TLT_SET(tbl, i, c)    (tbl[i] |= c)
#define TLT_GET(tbl, i)       (tbl[i] & 0377)


static const char* us = "tr";

static int hexs[16]  =
{ '0', '1', '2', '3'
, '4', '5', '6', '7'
, '8', '9', 'a', 'b'
, 'c', 'd', 'e', 'f'
}                    ;


enum
{  C_CLASSSTART = 256u
,  C_ALNUM
,  C_ALPHA
,  C_CNTRL
,  C_DIGIT
,  C_GRAPH
,  C_LOWER
,  C_PRINT
,  C_PUNCT
,  C_SPACE
,  C_UPPER
,  C_XDIGIT
,  C_CLASSEND

,  C_RANGESTART
,  C_RANGEEND

,  C_REPEAT       /* next token encodes repeat */
,  C_EOF
}  ;


#define ISCLASS(c) ((c) > C_CLASSSTART && (c) < C_CLASSEND)

#define ISBOUNDARY(c) \
  ( (c) == C_CLASSSTART || (c) == C_CLASSEND   \
    (c) == C_RANGESTART || (c) == C_RANGEEND ) \


int (*isit[13])(int c) =
{  NULL           /* so C_CLASSVAL - C_CLASSSTART gives an index */
,  isalnum
,  isalpha
,  iscntrl
,  isdigit
,  isgraph
,  islower
,  isprint
,  ispunct
,  isspace
,  isupper
,  isxdigit
}  ;



const char* tr_get_range
(  const char *p
,  const char *z
,  int  *offset
,  int  *range_len
)  ;



/* Sets either
 *    value to something in the range 0-255 and length * to something >= 1
 * or
 *    value to one of the constants C_ALPHA etc and length to 0.
 *
 * Returns the next parsable character or NULL for failure.
*/

const char* xtr_get_set
(  const char *p
,  const char *z
,  int  *value
,  int  *onwards
,  int  *repeat_count
)  ;


const char* xtr_get_token
(  const char *p
,  const char *z
,  int  *tokval
,  int  *repeat_count
,  mcxbool class_ok
)  ;


const char* tr_get_token
(  const char *p
,  const char *z
,  int  *tokval
)  ;


int mcxTRTranslate
(  char*    src
,  mcxTR*   tr
)
   {  int i,j
   ;  int prev =  -1          /* only relevant for squash case */
   ;  int len = strlen(src)
   ;  mcxbits flags = tr->modes
   ;  mcxbool squash = flags & MCX_TR_SQUASH
   ;  u32* tbl = tr->tlt
   ;

      for (i=0,j=0;i<len;i++)
      {  unsigned char idx = *(src+i)

         /* tbl[idx] < 0 is deletion, implies *not* squash */

      ;  if (tbl[idx] >= 0)
         {  mcxbool translate = (tbl[idx] > 0)
         ;  int val =  translate ? tbl[idx] : idx

         ;  /* val != prev: no squash */
            /* !squash: no squash */
            /* !tbl[idx]: character is not mapped, always print */

            if (val != prev || !squash || !translate)
            {  *(src+j) =  (char) val
            ;  j++
         ;  }
            if (squash)
            prev = translate ? val : -1
      ;  }
      }
      *(src+j) = '\0'
   ;  return j
;  }


mcxbool mcxTRLoadTable
(  const char* src
,  const char* dst
,  mcxTR*      tr
,  mcxbits     flags
)
   {  const char* p = src
   ;  const char* z = p + strlen(p)

   ;  const char* P = dst
   ;  const char* Z = P + strlen(P)

   ;  mcxbool empty        =  P >= Z ? TRUE : FALSE
   ;  mcxbool complement   =  flags & MCX_TR_COMPLEMENT
   ;  mcxbool delete       =  flags & MCX_TR_DELETE
   ;  u32* tbl = tr->tlt
   ;  int i

   ;  tr->modes = flags

   ;  for (i=0;i<256;i++)
      tbl[i] = 0

   ;  if (complement)
      {
         if (!delete && !*P)
         return FALSE

      ;  while (p<z)
         {
            int p_offset, p_len
         ;  p = tr_get_range(p, z, &p_offset, &p_len)

         ;  if (!p)
            return FALSE

         ;  while (--p_len >= 0)
            tbl[p_offset+p_len] = 1
      ;  }
         for (i=0;i<256;i++)
         tbl[i] = tbl[i] ? 0 : delete ? -1 : *P
      ;  return TRUE
   ;  }

      while (p<z)
      {
         int p_offset, p_len, P_offset, P_len
      ;  p = tr_get_range(p, z, &p_offset, &p_len)
; /* fprintf(stderr, "___ char(%d), len %d\n", p_offset, p_len) */

      ;  if (P >= Z)    /* dst is exhausted, rest is mapped to nil */
         empty = TRUE
      ;  else if (!(P = tr_get_range(P, Z, &P_offset, &P_len)))
         return FALSE

      ;  if (!p || (!empty && (p_len != P_len)))
         return FALSE

      ;  while (--p_len >= 0)
         tbl[p_offset+p_len] = empty ? -1 : P_offset + p_len
   ;  }

      return TRUE
;  }


const char* tr_get_range
(  const char *p
,  const char *z
,  int  *lower
,  int  *range_len
)
   {  int upper = 0
   ;  const char* q = tr_get_token(p, z, lower)

   ;  if (!q)
      return NULL

   ;  if (q<z && *q == '-')
      {  q = tr_get_token(q+1, z, &upper)

      ;  if (!q)
         return NULL

      ;  if (upper < *lower)
         return NULL

      ;  *range_len = upper - *lower + 1
   ;  }
      else
      *range_len = 1

   ;  return q
;  }


/* fixme: should pbb cast everything p to unsigned char */

const char* tr_get_token
(  const char *p
,  const char *z
,  int  *tokval
)
#define MCX_HEX(c)   (  (c>='0' && c<='9') ? c-'0'\
                     :  (c>='a' && c<='f') ? c-'a'+10\
                     :  (c>='A' && c<='F') ? c-'A'+10\
                     :  0\
                     )
   {  int c = 0
   ;  if (p >= z)
      return NULL

   ;  if (*p == '\\')
      {  if (p+1 == z)
         {  *tokval = '\\'
         ;  return p + 2
      ;  }
      
         if (p+3 >= z)
         return NULL

      ;  if
         (  isdigit((unsigned char) *(p+1))
         && isdigit((unsigned char) *(p+2))
         && isdigit((unsigned char) *(p+3))
         )
         c = 64 * (*(p+1)-'0') + 8  * (*(p+2)-'0') + (*(p+3)-'0')
      ;  else if
         (  *(p+1) == 'x'
         && isxdigit((unsigned char) *(p+2))
         && isxdigit((unsigned char) *(p+3))
         )
         c = 16 * MCX_HEX(*(p+2)) +  MCX_HEX(*(p+3))
      ;  else
         return NULL

      ;  if (c < 0 || c > 255)
         return NULL

      ;  *tokval = c
      ;  return p+4
   ;  }
      else
      {  c = (unsigned char) *p
      ;  if (c < 0 || c > 255)
         return NULL
      ;  *tokval = c
      ;  return p+1
   ;  }
      return NULL
;  }


mcxTing* mcxTRExpandSpec
(  const char* spec
,  mcxbits  bits
)
   {  mcxTR tr
   ;  int i = 0, n_set = 0
   ;  mcxTing* set = mcxTingEmpty(NULL, 256)
   ;  mcxbits complement = bits & MCX_TR_COMPLEMENT
   ;  char* p
   ;  u32* tbl = tr.tlt

   ;  if (!mcxTRLoadTable(spec, spec, &tr, complement))
      return NULL

   ;  for (i=0;i<256;i++)
      {  if
         (  (complement && tbl[i] <= 0)
         || (!complement && tbl[i] > 0)
         )
         set->str[n_set++] = (unsigned char) i
   ;  }
      set->str[n_set] = '\0'
   ;  set->len = n_set

   ;  if ((p = strchr(set->str, '-')))
      {  *p = set->str[0]
      ;  set->str[0] = '-'
   ;  }
      if ((p = strchr(set->str, '\\')))
      {  *p = set->str[set->len-1]
      ;  set->str[set->len-1] = '\\'
   ;  }
      return set
;  }



int mcxTingTranslate
(  mcxTing* src
,  mcxTR*   tr
)
   {  src->len  =  mcxTRTranslate(src->str, tr)
   ;  return src->len
;  }



/* fixme conceivably mcxTRTranslate could return -1 as
 * error condition
*/
int mcxTingTr
(  mcxTing*       txt
,  const char*    src
,  const char*    dst
,  mcxbits        flags
)
   {  mcxTR tr
   ;  if (!mcxTRLoadTable(src, dst, &tr, flags))
      return -1
   ;  txt->len  =  mcxTRTranslate(txt->str, &tr)
   ;  return txt->len
;  }



/* only parse a single spec. Caller has to deal with complement.
*/

mcxTing* mcxTRLoadSpec
(  const char*    src
)
   {  const char* p     =  src
   ;  const char* z     =  p + strlen(p)
   ;  const char* me    =  "mcxTRLoadSpec"
   ;  mcxTing* dst      =  mcxTingEmpty(NULL, MAX_SPEC_SIZE+1)
   ;  int val_prev      =  -1
   ;  int len           =  0
   ;  mcxstatus status  =  STATUS_OK

   ;  while (p<z)
      {  int c    =  (unsigned char) *p, d
      ;  int val  =  -1
      ;  int np   =  1
      ;  mcxbool range =  FALSE

      ;  status = STATUS_FAIL
      ;  if (!mcxTingEnsure(dst, len+2))              break

      ;  switch(c)
         {  case '\\'
         :     if (p+1 >= z)                          break
            ;  d = (unsigned char) p[1]

            ;  if (!strchr("0123\\abfnrtv", d))
                  val= d
               ,  np = 2
            ;  else if (strchr("0123", d))
               {  if (p+3 >=z)                        break

               ;  np = 4
               ;  if (isdigit(UC(p+2)) && isdigit(UC(p+3))) /* fixme 009 */
                  val = 64*d + 8*(UC(p+2)-'0') + (UC(p+3)-'0')
               ;  else                                break
               ;  if (val > 0377)                     break
            ;  }
               else
               {  np = 2
               ;  switch(d)
                  {  case 'a' : val = 07  ;  break
                  ;  case 'b' : val = 010 ;  break
                  ;  case 'f' : val = 014 ;  break
                  ;  case 'n' : val = 012 ;  break
                  ;  case 'r' : val = 015 ;  break
                  ;  case 't' : val = 011 ;  break
                  ;  case 'v' : val = 013 ;  break
                  ;  case '\\': val = 0134;  break
                  ;  default  : val = '?' ;  break
               ;  }
               }
               status = STATUS_OK
            ;  break
            ;
         case '['
            :  val = c
            ;  status = STATUS_OK
            ;  break
            ;
         case '-'
            :  val = c
            ;  range = TRUE
            ;  status = STATUS_OK
            ;  break
            ;
         default
            :  val = c
            ;  status = STATUS_OK
            ;  break
            ;
         }
         if (status)
         break
      ;  status = STATUS_FAIL

      ;  if (range)
         {  if (val_prev < 0 || val_prev > val)       break
         ;  if (!mcxTingEnsure(dst, len + 2 * (val - val_prev + 1)))
                                                      break
         ;  for (d=val_prev; d<=val; d++)
               dst->str[len++] = HEX1(d)
            ,  dst->str[len++] = HEX2(d)
      ;  }
         else
            dst->str[len++] = HEX1(val)
         ,  dst->str[len++] = HEX2(val)

;fprintf(stderr, "--> <%d|%d|%d>\n", val, HEX1(val), HEX2(val))
      ;  val_prev = val
      ;  p += np
      ;  status = STATUS_OK
   ;  }

      if (status)
      {  mcxErr(me, "parse error")     /* fixme add error codes */
      ;  mcxTingFree(&dst)
      ;  return NULL
   ;  }

      dst->len = len
   ;  dst->str[len] = '\0'
   ;  return dst
;  }


const char* xtr_get_set
(  const char *p
,  const char *z
,  int  *value
,  int  *onwards
,  int  *repeat_count
)
   {  const char* q  =  xtr_get_token(p, z, value, repeat_count, TRUE)
   ;  *onwards =  0

   ;  if (!q)
      return NULL

   ;  if (q<z && *value <= 255 && *q == '-')
      {  int value_top = -1
      ;  q = xtr_get_token(q+1, z, &value_top, repeat_count, FALSE)

      ;  if (!q)
         return NULL

      ;  if (value_top < *value)
         {  mcxErr("xtr_get_set", "illegal range %c-%c", *value, value_top)
         ;  return NULL
      ;  }
         *onwards = value_top - *value + 1
   ;  }
      else              /* also ok if repeat_count >= 0 */
      *onwards = 1

   ;  return q
;  }



const char* xtr_get_token
(  const char *p
,  const char *z
,  int  *tokval
,  int  *repeat_count
,  mcxbool class_ok
)
#define MCX_HEX(c)   (  (c>='0' && c<='9') ? c-'0'\
                     :  (c>='a' && c<='f') ? c-'a'+10\
                     :  (c>='A' && c<='F') ? c-'A'+10\
                     :  0\
                     )
   {  int c = UC(p), d = 0, np = 0, val = -1
   ;  mcxstatus status = STATUS_FAIL

   ;  *tokval = -1
   ;  if (repeat_count)
      *repeat_count = -1

   ;  while (1)
      {  if (p >= z)
         break

   /*************************************************************************/

      ;  if (c == '\\')
         {  if (p+1 >= z)                          break
         ;  d = UC(p+1)

         ;  if (strchr("456789", d))               break

         ;  if (strchr("0123", d))
            {  if (p+3 >=z)                        break
            ;  if (isdigit(UC(p+2)) && isdigit(UC(p+3))) /* fixme 009 */
               val = 64*(d-'0') + 8*(UC(p+2)-'0') + (UC(p+3)-'0')
            ;  else                                break
            ;  if (val > 0377)                     break
            ;  np = 4
         ;  }
            else if (d == 'x')
            {  if (p+3 >= z)                       break
            ;  if
               ( !isxdigit(UC(p+2))
               ||!isxdigit(UC(p+3))
               )                                   break

            ;  val = 16 * MCX_HEX(UC(p+2)) +  MCX_HEX(UC(p+3))
            ;  np = 4
         ;  }
            else if (strchr("\\abfnrtv", d))
            {  np = 2
            ;  switch(d)
               {  case 'a' : val = 07  ;  break
               ;  case 'b' : val = 010 ;  break
               ;  case 'f' : val = 014 ;  break
               ;  case 'n' : val = 012 ;  break
               ;  case 'r' : val = 015 ;  break
               ;  case 't' : val = 011 ;  break
               ;  case 'v' : val = 013 ;  break
               ;  case '\\': val = 0134;  break
               ;  default  : val = '?' ;  break    /* impossible */
            ;  }
            }
            else
            {  val= d
            ;  np = 2
         ;  }
         }  /* case '\\' */

   /*************************************************************************/

         else if (c == '[')
         {  if (!class_ok)                            break

         /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

         ;  else if (p+1 < z && UC(p+1) == ':')
            {  if (!strncmp(p+2, "alpha:]", 7))    val = C_ALPHA,  np = 4+ 5
      ;   else if (!strncmp(p+2, "alnum:]", 7))    val = C_ALNUM,  np = 4+ 5
      ;   else if (!strncmp(p+2, "digit:]", 7))    val = C_DIGIT,  np = 4+ 5
      ;   else if (!strncmp(p+2, "cntrl:]", 7))    val = C_CNTRL,  np = 4+ 5
      ;   else if (!strncmp(p+2, "graph:]", 7))    val = C_GRAPH,  np = 4+ 5
      ;   else if (!strncmp(p+2, "lower:]", 7))    val = C_LOWER,  np = 4+ 5
      ;   else if (!strncmp(p+2, "print:]", 7))    val = C_PRINT,  np = 4+ 5
      ;   else if (!strncmp(p+2, "punct:]", 7))    val = C_PUNCT,  np = 4+ 5
      ;   else if (!strncmp(p+2, "space:]", 7))    val = C_SPACE,  np = 4+ 5
      ;   else if (!strncmp(p+2, "upper:]", 7))    val = C_UPPER,  np = 4+ 5
      ;   else if (!strncmp(p+2, "xdigit:]", 8))   val = C_XDIGIT, np = 4+ 6
      ;        else                                   break
         ;  }

         /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

            else if (p+1 < z && UC(p+1) == '*')
            {  int num = 0, tokval2 = -1
            ;  const char* q = NULL
            ;  if (!repeat_count || p+2 >= z)         break

            ;  if (!(q = xtr_get_token(p+2, z, &tokval2, repeat_count, FALSE)))
                                                      break

            ;  if (UC(q) != '*')                      break 

            ;  if (UC(q+1) == ']')
               {  *repeat_count = 0
               ;  q++
            ;  }
               else
               {  if (sscanf(q+1, "%d]", &num) != 1)     break
               ;  if (!(q = strchr(q, ']')))             break
               ;  if (num < 0 || num > 256)              break
               ;  *repeat_count  =  num
            ;  }
                  /* q[0] should be ']' now */

               val            =  tokval2
            ;  np             =  (q+1) - p
         ;  }

         /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

            else                                      break
      ;  }

   /*************************************************************************/

         else
         {  val = c
         ;  np  = 1
      ;  }

         status = STATUS_OK
      ;  break
   ;  }

      if (status)
      {  mcxErr("xtr_get_token", "error at char (%c)", UC(p))
      ;  return NULL
   ;  }
      
fprintf(stderr, "char (%d)\n", val)
;
      *tokval = val
   ;  return p + np
;  }


void mcx_dump_parse
(  u32* enc
)
   {  int i = 0
   ;  while (i<MAX_SPEC_SIZE+1)
      {  int meta = enc[i] >> 8
      ;  int val  = enc[i] & 0377
      ;  fprintf(stdout, "%6d%6d%6d\n", i, meta, val)
      ;  if (meta == C_EOF)
         {  mcxTell(us, "end token as expected")
         ;  break
      ;  }
         i++
   ;  }
   }


mcxstatus mcx_tr_load_spec
(  const char* spec
,  mcxTR*      tr
,  u32*        tbl         /* size MAX_SPEC_SIZE+1 */
,  mcxbool     repeat_ok
)
   {  const char* p  =  spec
   ;  const char* z  =  spec + strlen(spec)
   ;  int ofs        =  0
   ;  int specval    =  -1
   ;  int speclen    =  -1
   ;  int specrep    =  -1
   ;  int curclass   =  0
   ;  int i          =  0
   ;  int* repeat    =  repeat_ok ? &specrep : NULL
   ;  mcxstatus status = STATUS_FAIL

   ;  while (1)
      {  status = STATUS_OK

      ;  if (p >= z)
         break
      ;  status = STATUS_FAIL

      ;  if (ofs >= MAX_SPEC_SIZE)     /* space for 1 token; check anything longer */
         break
      ;  if (!(p = xtr_get_set(p, z, &specval, &speclen, repeat)))
         break

      ;  if (repeat && specrep >= 0)
         {  tbl[ofs++] = C_REPEAT << 8 | specval
         ;  if (ofs >= MAX_SPEC_SIZE)
            break
         ;  tbl[ofs++] = specrep & 0377
      ;  }
         else if (ISCLASS(specval))
         {  int (*innit)(int)    =  isit[specval - C_CLASSSTART]

         ;  curclass             =  specval - C_CLASSSTART
         ;  tbl[ofs++]           =  (C_CLASSSTART << 8 ) | curclass

         ;  for (i=0;i<256;i++)
            {  if (innit(i))
               {  if (ofs >= MAX_SPEC_SIZE)
                  break
               ;  tbl[ofs++] = i
            ;  }
            }
            if (i != 256)
            break
         ;  tbl[ofs++] = (C_CLASSEND << 8) | curclass
      ;  }
         else if (speclen > 1)
         {  tbl[ofs++] = C_RANGESTART << 8
         ;  for (i=specval;i<specval+speclen;i++)
            {  if (ofs >= MAX_SPEC_SIZE)
               break
            ;  tbl[ofs++] = i
         ;  }
            if (i != specval+speclen)
            break
         ;  tbl[ofs++] = C_RANGEEND << 8
      ;  }
         else
         tbl[ofs++] = specval

      ;  status = STATUS_OK
   ;  }


      tbl[ofs++] = C_EOF << 8

   ;  if (status || p!=z)
      {  mcxErr(us, "error loading spec")
      ;  return STATUS_FAIL
   ;  }
      return STATUS_OK
;  }



mcxstatus mcx_tr_encode_tlt
(  mcxTR* tr
,  u32*   srcenc
,  u32*   dstenc
)
   {  mcxstatus status = STATUS_FAIL
   ;  while (1)
      {
      ;  status = STATUS_OK
      ;  break
   ;  }
      return status
;  }



/*
 *    the log arrays use stop and end tokens for ranges and classes.
 *    both are required to have more than 1 element.
 *    No more than MAX_SPEC_SIZE+1 positions are allocated for either specification,
 *    which also cater for start and end and EOF tokens - the +1 is
 *    for the EOF token.
*/

mcxstatus mcx_tr_load_tlt
(  const char* src
,  const char* dst
,  mcxTR* tr
)
   {  u32 srcenc[MAX_SPEC_SIZE+1]
   ;  u32 dstenc[MAX_SPEC_SIZE+1]

   ;  mcxstatus status = mcx_tr_load_spec(src, tr, srcenc, FALSE)

   ;  if (status)
      return STATUS_FAIL
   ;  mcx_dump_parse(srcenc)

   ;  if (mcx_tr_load_spec(dst, tr, dstenc, TRUE))
      return STATUS_FAIL
   ;  mcx_dump_parse(dstenc)

   ;  return mcx_tr_encode_tlt(tr, srcenc, dstenc)
;  }



mcxstatus mcx_tr_load
(  const char* src
,  const char* dst
,  const char* delete
,  const char* squash
,  mcxTR*      tr
,  mcxbits     modes
)
   {  const char* me =  "mcx_tr_load"
   ;  int i

;  fprintf(stderr, "%s %s\n", src, dst)

   ;  if (src && UC(src) == '^')
         src++
      ,  modes |= MCX_TR_SRC_C

   ;  if (dst && UC(dst) == '^')
         dst++
      ,  modes |= MCX_TR_DST_C

   ;  if (delete && UC(delete) == '^')
         delete++
      ,  modes |= MCX_TR_DELETE_C

   ;  if (squash && UC(squash) == '^')
         squash++
      ,  modes |= MCX_TR_SQUASH_C

   ;  tr->modes = modes

   ;  for (i=0;i<256;i++)
      tr->tlt[i] = 0

   ;  if (src)
      {  if (!dst)
         {  mcxErr(me, "src requires dst")
         ;  return STATUS_FAIL
      ;  }
         return mcx_tr_load_tlt(src, dst, tr)
   ;  }
      return STATUS_FAIL
;  }






