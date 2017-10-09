/*   (C) Copyright 2001, 2002, 2003, 2004, 2005 Stijn van Dongen
 *   (C) Copyright 2006 Stijn van Dongen
 *
 * This file is part of tingea.  You can redistribute and/or modify tingea
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with tingea, in the file COPYING.
*/


#ifndef tingea_heap_h
#define tingea_heap_h


enum
{  MCX_MIN_HEAP = 10000           /*    find large elements                 */
,  MCX_MAX_HEAP                   /*    find small elements                 */
}  ;


typedef struct
{  void     *base
;  dim      heapSize
;  dim      elemSize
;  int      (*cmp)(const void* lft, const void* rgt)
;  mcxenum  type
;  dim      n_inserted
;
}  mcxHeap  ;


mcxHeap* mcxHeapInit
(  void*    heap
)  ;


mcxHeap* mcxHeapNew
(  mcxHeap* heap
,  dim      heapSize
,  dim      elemSize
,  int      (*cmp)(const void* lft, const void* rgt)
,  mcxenum  HEAPTYPE          /* MCX_MIN_HEAP or MCX_MAX_HEAP */
)  ;


void mcxHeapFree
(  mcxHeap**   heap
)  ;

void mcxHeapRelease
(  void* heapv
)  ;  

void mcxHeapClean
(  mcxHeap*   heap
)  ;  

void mcxHeapInsert
(  mcxHeap* heap
,  void*    elem
)  ;


#endif

