/* (c) Copyright 2005 Stijn van Dongen
 *
 * This file is part of apparix.  You can redistribute and/or modify apparix
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with apparix, in the file COPYING.
*/

/* This file is largish, mostly because it was spaceously written.
 * NOTE
 *    we do not do hash lookup of mark, because just building the hash
 *    takes longer then a sequential search.
 * TODO
 *    j/J is a bit of a kludge. On disk is always j.
*/


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "version.h"

#include "util/io.h"
#include "util/alloc.h"
#include "util/opt.h"
#include "util/err.h"
#include "util/tok.h"
#include "util/ding.h"
#include "util/ting.h"
#include "util/types.h"

enum
{  MY_OPT_ADD_JUMP
,  MY_OPT_ADD_PORTAL
,  MY_OPT_REHASH
,  MY_OPT_GREP
,  MY_OPT_DUMP
,  MY_OPT_LIST
,  MY_OPT_UNDO
,  MY_OPT_FAVOUR
,  MY_OPT_SQUASH_MARK
,  MY_OPT_SQUASH_MARK2
,  MY_OPT_SQUASH_DEST
,  MY_OPT_PURGE
,  MY_OPT_PURGE_MARK
,  MY_OPT_BACKUP
,  MY_OPT_BACKUP_NAMED
,  MY_OPT_CWD
,  MY_OPT_SHELL
,  MY_OPT_VERSION
,  MY_OPT_HELP
,  MY_OPT_APROPOS
}  ;



static const char* us    =  "apparix";

static mcxbool use_cwd  =  FALSE;

#define MY_PATH_MAX 1024


const char* shell_examples[] =
{  "BASH-style functions\n---"
,  "function to () {"
,  "   if test \"$2\"; then"
,  "     cd \"$(apparix \"$1\" \"$2\" || echo .)\";"
,  "   else"
,  "     cd \"$(apparix \"$1\" || echo .)\";"
,  "   fi"
,  "   pwd"
,  "}"
,  "function bm () {"
,  "   if test \"$2\"; then"
,  "      apparix --add-mark \"$1\" \"$2\";"
,  "   elif test \"$1\"; then"
,  "      apparix --add-mark \"$1\";"
,  "   else"
,  "      apparix --add-mark;"
,  "   fi"
,  "}"
,  "function portal () {"
,  "   if test \"$1\"; then"
,  "      apparix --add-portal \"$1\";"
,  "   else"
,  "      apparix --add-portal;"
,  "   fi"
,  "}"
,  "# function to generate list of completions from .apparixrc"
,  "function _apparix_aliases ()"
,  "{   cur=$2"
,  "    dir=$3"
,  "    COMPREPLY=()"
,  "    if [ \"$1\" == \"$3\" ]"
,  "    then"
,  "        COMPREPLY=( $( cat $HOME/.apparix{rc,expand} | \\"
,  "                       grep \"j,.*$cur.*,\" | cut -f2 -d, ) )"
,  "    else"
,  "        dir=`apparix -favour rOl $dir 2>/dev/null` || return 0"
,  "        eval_compreply=\"COMPREPLY=( $("
,  "            cd \"$dir\""
,  "            \\ls -d *$cur* | while read r"
,  "            do"
,  "                [[ -d \"$r\" ]] &&"
,  "                [[ $r == *$cur* ]] &&"
,  "                    echo \\\"${r// /\\\\ }\\\""
,  "            done"
,  "            ) )\""
,  "        eval $eval_compreply"
,  "    fi"
,  "    return 0"
,  "}"
,  "# command to register the above to expand when the 'to' command's args are"
,  "# being expanded"
,  "complete -F _apparix_aliases to"
,  "---\nCSH-style aliases\n---"
,  "alias to    'cd `(apparix -favour rOl \\!* || echo -n .)` && pwd'"
,  "alias bm   'apparix --add-mark \\!*'"
,  "alias portal 'apparix --add-portal \\!*'"
,  "---"
,  NULL
}  ;

const char* syntax = "apparix [--add-mark|--add-portal] bookmark (...)";

mcxOptAnchor options[] =
{  {  "--apropos"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_APROPOS
   ,  NULL
   ,  "print this help"
   }
,  {  "--version"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "print version and exit"
   }
,  {  "--bu"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_BACKUP
   ,  NULL
   ,  "copy .apparixrc to .apparixrc.bu"
   }
,  {  "-l"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_LIST
   ,  NULL
   ,  "list all bookmarks"
   }
,  {  "-d"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_DUMP
   ,  NULL
   ,  "dump resource file to STDOUT"
   }
,  {  "-u"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_UNDO
   ,  "<num>"
   ,  "undo last <num> additions"
   }
,  {  "-bu"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_BACKUP_NAMED
   ,  "fname"
   ,  "copy .apparixrc to fname"
   }
,  {  "-v"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_VERSION
   ,  NULL
   ,  "print version and exit"
   }
,  {  "-h"
   ,  MCX_OPT_DEFAULT | MCX_OPT_INFO
   ,  MY_OPT_HELP
   ,  NULL
   ,  "print this help"
   }
,  {  "--add-mark"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_ADD_JUMP
   ,  NULL
   ,  "add jump bookmark, accepts trailing [mark [destination]]"
   }
,  {  "--add-portal"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_ADD_PORTAL
   ,  NULL
   ,  "add portal bookmark, accepts trailing [destination]"
   }
,  {  "--squash-mark"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SQUASH_MARK
   ,  NULL
   ,  "for repeated marks take the first occurring destination (cf -favour)"
   }
,  {  "-sm"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SQUASH_MARK2
   ,  NULL
   ,  "same as above"
   }
,  {  "--squash-dest"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SQUASH_DEST
   ,  NULL
   ,  "for repeated destinations take the first occurring mark (cf -favour)"
   }
,  {  "-purge"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_PURGE
   ,  "<pat>"
   ,  "delete bookmarks where destination matches <pat>"
   }
,  {  "-purge-mark"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_PURGE_MARK
   ,  "<pat>"
   ,  "delete bookmarks where mark matches <pat>"
   }
,  {  "-favour"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_FAVOUR
   ,  "[lroLRO]*"
   ,  "(level, order, regular) duplicate resolution"
   }
,  {  "--rehash"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_REHASH
   ,  NULL
   ,  "re-expand portal bookmarks"
   }
,  {  "--cwd"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_CWD
   ,  NULL
   ,  "use getcwd(3) rather than reading pwd(1) output"
   }
,  {  "--shell-examples"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SHELL
   ,  NULL
   ,  "output example macros for tcsh and bash"
   }
,  {  NULL
   ,  0
   ,  0
   ,  NULL
   ,  NULL
   }
}  ;


void show_version
(  void
)
   {  fprintf
      (  stdout
      ,
"apparix %s, %s\n"
"(c) Copyright 2005, 2006 Stijn van Dongen. apparix comes with NO WARRANTY\n"
"to the extent permitted by law. You may redistribute copies of apparix under\n"
"the terms of the GNU General Public License.\n"
      ,  apxNumTag
      ,  apxDateTag
      )
;  }


#define  BIT_PURGE         1

typedef struct
{  int         type              /*  'j', 'e', 'J' */
;  mcxTing*    mark
;  mcxTing*    dest
;  mcxbits     info
;  int         ord
;
}  bookmark    ;


typedef struct
{  bookmark*   bms
;  int         bm_n
;  int         bm_N
;
}  folder      ;

static mcxTing* favour  =  NULL;


int bookmark_cmp_j_over_J
(  const void* bm1
,  const void* bm2
)
   {  const bookmark* b1 = bm1
   ;  const bookmark* b2 = bm2
   ;  return   b2->type - b1->type
;  }


int bookmark_cmp_ord
(  const void* bm1
,  const void* bm2
)
   {  return ((bookmark*)bm1)->ord - ((bookmark*)bm2)->ord
;  }


int bookmark_cmp_level
(  const void* bm1
,  const void* bm2
)
   {  const bookmark* b1 = bm1
   ;  const bookmark* b2 = bm2
   ;  return
      (  mcxStrCountChar(b1->dest->str, '/', b1->dest->len)
      -  mcxStrCountChar(b2->dest->str, '/', b2->dest->len)
      )
;  }


int bookmark_cmp_mark_favour
(  const void* bm1
,  const void* bm2
)
   {  int i=0
   ;  int cmp_result = 0

   ;  int c = strcmp(((bookmark*) bm1)->mark->str, ((bookmark*) bm2)->mark->str)

   ;  if (c)
      return c

   ;  for (i=0;i<favour->len;i++)
      {  int mode = (unsigned char) favour->str[i]
      ;  switch(mode)
         {  case 'l'
         :  cmp_result = bookmark_cmp_level(bm1, bm2)
         ;  break
         ;

            case 'L'
         :  cmp_result = bookmark_cmp_level(bm2, bm1)
         ;  break
         ;

            case 'o'
         :  cmp_result = bookmark_cmp_ord(bm1, bm2)
         ;  break
         ;

            case 'O'
         :  cmp_result = bookmark_cmp_ord(bm2, bm1)
         ;  break
         ;

            case 'r'
         :  cmp_result = bookmark_cmp_j_over_J(bm1, bm2)
         ;  break
         ;

            case 'R'
         :  cmp_result = bookmark_cmp_j_over_J(bm2, bm1)
         ;  break
      ;  }
         if (cmp_result)
         break
   ;  }
      return cmp_result
;  }


int bookmark_cmp_mark
(  const void* bm1
,  const void* bm2
)
   {  return strcmp(((bookmark*)bm1)->mark->str,((bookmark*)bm2)->mark->str)
;  }


int bookmark_cmp_dest
(  const void* bm1
,  const void* bm2
)
   {  return strcmp(((bookmark*)bm1)->dest->str,((bookmark*)bm2)->dest->str)
;  }


void* bookmark_init
(  void* bm_v
)
   {  bookmark* bm = bm_v
   ;  bm->type =  0
   ;  bm->mark =  mcxTingEmpty(NULL, 12)
   ;  bm->dest =  mcxTingEmpty(NULL, 40)
   ;  bm->info =  0
   ;  bm->ord  =  0
   ;  return bm
;  }


folder* folder_new
(  int bm_N
)
   {  folder* fl        =  mcxAlloc(sizeof *fl, EXIT_ON_FAIL)
   ;  bookmark* bms     =  mcxNAlloc
                           (bm_N, sizeof *bms, bookmark_init, EXIT_ON_FAIL)
   ;  fl->bm_n          =  0
   ;  fl->bm_N          =  bm_N
   ;  fl->bms           =  bms
   ;  return fl
;  }


void folder_add
(  folder*  fl
,  int      type
,  mcxbits  info
,  const mcxTing* mark
,  const mcxTing* dest
,  int      ord
)
   {  int bm_n = fl->bm_n
   ;  int bm_N = fl->bm_N
   ;  bookmark* bm = NULL

   ;  if (bm_n >= bm_N)
      {  fl->bms
      =  mcxNRealloc
         (  fl->bms
         ,  2*bm_N
         ,  bm_N
         ,  sizeof fl->bms[0]
         ,  bookmark_init
         ,  EXIT_ON_FAIL
         )
      ;  bm_N = 2*bm_N
   ;  }

      bm       =  fl->bms+bm_n++
   ;  bm->type =  type
   ;  bm->info =  info

   ;  mcxTingWrite(bm->dest, dest->str)
   ;  if (mark)
      mcxTingWrite(bm->mark, mark->str)

   ;  bm->ord  =  ord
   ;  fl->bm_n = bm_n
   ;  fl->bm_N = bm_N
;  }



#if 0                               /* not used */
int folder_dups
(  folder* fl_any
)
   {  bookmark* bm = fl_any->bms+1
   ;  int n_dup = 0

   ;  while (bm<fl_any->bms + fl_any->bm_n)
      {  if (!strcmp(bm[0].mark->str, bm[-1].mark->str))
         n_dup++
      ;  bm++
   ;  }

      return n_dup
;  }


/* this is broken when both contain same mark multiple times */

int folder_dups2
(  folder* fl_uniq      /* assume/pretend uniq */
,  folder* fl_any
)
   {  bookmark* bmu = fl_uniq->bms
   ;  bookmark* bma = fl_any->bms
   ;  int n_dup = 0

   ;  while (bmu<fl_uniq->bms+fl_uniq->bm_n && bma<fl_any->bms+fl_any->bm_n)
      {  int c = strcmp(bmu->mark->str, bma->mark->str)
      ;  if (!c)
         {  n_dup++
         ;  bma++
      ;  }
         else if (c < 0)
         bmu++
      ;  else
         bma++
   ;  }

      return n_dup
;  }
#endif


void folder_addto
(  folder*  flall
,  folder*  fl
,  mcxbool  reuse_ord
)
   {  bookmark* bm = fl->bms
   ;  int ord = 0
   ;  while (bm<fl->bms+fl->bm_n)
      {  folder_add(flall, bm->type, bm->info, bm->mark, bm->dest, reuse_ord ? bm->ord : ord++)
      ;  bm++
   ;  }
   }


/* warning: does not merge in the sense of preserving sort. */

folder* folder_merge
(  folder* fl1
,  folder* fl2
,  mcxbool reuse_ord
)
   {  folder* fl3       =  folder_new(fl1->bm_n+fl2->bm_n)

   ;  folder_addto(fl3, fl1, reuse_ord)
   ;  folder_addto(fl3, fl2, reuse_ord)

   ;  return fl3
;  }


folder* expand_portal
(  mcxIO* xfport
,  const char* dest
)
   {  DIR * dir = opendir(dest)
   ;  struct dirent *de
   ;  mcxstatus status = STATUS_FAIL
   ;  mcxTing* fqname = mcxTingEmpty(NULL, 512)
   ;  mcxTing* dname = mcxTingEmpty(NULL, 512)
   ;  folder* fl = folder_new(20)
   ;  int ct = 0

   ;  while (1)
      {  if (!dir)
         {  mcxErr(us, "cannot open directory %s", dest)
         ;  break
      ;  }
         while ((de = readdir(dir)))
         {  struct stat fstat
         ;  mcxTingWrite(dname, de->d_name)
         ;  mcxTingPrint(fqname, "%s/%s", dest, de->d_name)
         ;  if ((unsigned char) dname->str[0] == '.')
            continue
         ;  if (!strncmp(dname->str, "CVS", 3))      /* fixme: env variable */
            continue
         ;  if (lstat(fqname->str, &fstat) < 0)
            {  mcxErr(us, "error accessing node %s", fqname->str)
            ;  break
         ;  }
            if (!S_ISDIR(fstat.st_mode))
            continue
         ;  fprintf(xfport->fp, "j,%s,%s\n", dname->str, fqname->str)
         ;  folder_add(fl, 'J', 0, dname, fqname, ct++)
      ;  }
         closedir(dir)

      ;  if (de)
         break
      ;  status = STATUS_OK
      ;  break
   ;  }
      mcxTingFree(&fqname)
   ;  mcxTingFree(&dname)
   ;  return fl
;  }



mcxstatus bookmark_resync_portal
(  folder*     flport
,  mcxIO*      xfport
)
   {  int i
   ;  const char* portname = xfport->fn->str
   ;  int ct_expansion = 0
   ;  int ct_portal = 0

   ;  mcxIOclose(xfport)
   ;  {  struct stat fstat
      ;  if (lstat(portname, &fstat) < 0)
         {  if (errno == ENOENT)
            mcxTell(us, "portal file %s will be newly created", portname)
         ;  else
            mcxErr(us, "cannot access %s, fingers crossed", portname)
      ;  }
      }

      mcxIOrenew(xfport, NULL, "w")
   ;  if (mcxIOopen(xfport, EXIT_ON_FAIL))
      {  mcxErr(us, "panic - portal file is gone, try --rehash")
      ;  return STATUS_FAIL
   ;  }

      for (i=0;i<flport->bm_n;i++)
      {  folder* fl = expand_portal(xfport, flport->bms[i].dest->str)
      ;  if (fl)
            ct_expansion += fl->bm_n
         ,  ct_portal++
   ;  }                          /* fixme fl memleak */

      mcxTell
      (us, "expanded %d portals to %d destinations", ct_portal, ct_expansion)
   ;  return STATUS_OK
;  }


mcxstatus bookmark_backup_rcfile
(  const char* name
,  const char* dest
)
   {  mcxTing* target  =      dest
                           ?  mcxTingPrint(NULL, "%s", dest)
                           :  mcxTingPrint(NULL, "%s.bu", name)
   ;  mcxTing* cmd
      =     !strcmp(target->str, "-")
         ?  mcxTingPrint(NULL, "cat %s", name)
         :  mcxTingPrint(NULL, "cp %s %s", name, target->str)

   ;  int cpstat = system(cmd->str)
   ;  if (cpstat)
      {  mcxErr(us, "panic cannot write file %s", target->str)
      ;  return STATUS_FAIL
   ;  }
      return STATUS_OK
;  }


void bookmark_report
(  const char* kind
,  folder* fl
,  const char* mark
)
   {  int i
   ;  for (i=0;i<fl->bm_n;i++)
      {  bookmark* bm = fl->bms+i
      ;  if (!strcmp(bm->mark->str, mark))
         fprintf(stderr, "%s %s exists -> %s\n", kind, mark, bm->dest->str)
   ;  }
   }


mcxstatus bookmark_resync_rc
(  folder*     flreg
,  folder*     flport
,  mcxIO*      xfrc
)
   {  int i
   ;  const char* rcname = xfrc->fn->str
   ;  folder* flmerge
   ;  int n_purged = 0

   ;  mcxIOclose(xfrc)

   ;  if (bookmark_backup_rcfile(xfrc->fn->str, NULL))
      return STATUS_FAIL

   ;  mcxIOrenew(xfrc, NULL, "w")

   ;  if (mcxIOopen(xfrc, EXIT_ON_FAIL))
      {  mcxErr(us, "panic - rc file is gone, backup in %s.bu", rcname)
      ;  return STATUS_FAIL
   ;  }

      flmerge =      flport
                  ?  folder_merge(flreg, flport, TRUE)
                  :  flreg

   ;  qsort(flmerge->bms, flmerge->bm_n, sizeof flmerge->bms[0], bookmark_cmp_ord)

   ;  for (i=0;i<flmerge->bm_n;i++)
      {  bookmark* bm   =  flmerge->bms+i
      ;  FILE* fp       =  bm->info & BIT_PURGE ? stdout : xfrc->fp
      ;  int type       =  bm->type

      ;  if (type == 'j' || type == 'J')
         fprintf(fp, "j,%s,%s\n", bm->mark->str, bm->dest->str)
      ;  else if (bm->type == 'e')
         fprintf(fp, "%c,%s\n", bm->type, bm->dest->str)

      ;  if (bm->info & BIT_PURGE)
         n_purged++
   ;  }
      fprintf(stderr, "purged a flock of %d\n", n_purged)
   ;  return STATUS_OK
;  }


enum
{  SQUASH_DEST = 1444
,  SQUASH_MARK
}  ;


void bookmark_squash
(  folder*      flreg
,  mcxIO*       xfrc
,  int          mode
)
   {  int j
   ;  mcxbool dest = mode == SQUASH_DEST

   ;  if (!flreg->bm_n)
      return

   ;  if (!favour)
      favour = mcxTingNew("OLr")
            /* younger entries before older entries (keep latest & greatest)
             * deeper levels over shallower levels (irrelevant due to o)
             * regular before expansions (irrelevant due to o)
            */

   ;  qsort(flreg->bms, flreg->bm_n, sizeof flreg->bms[0], bookmark_cmp_mark_favour)

   ;  for (j=1;j<flreg->bm_n;j++)
      {  bookmark* bmj = flreg->bms+j
      ;  bookmark* bmi = bmj - 1

      ;  if (bmj->type == 'e')
         mcxErr(us, "internal error e-in-rc-folder")

      ;  if
         (  ( dest && !strcmp(bmi->dest->str, bmj->dest->str))
         || (!dest && !strcmp(bmi->mark->str, bmj->mark->str) )
         )
         bmj->info |= BIT_PURGE
   ;  }

      qsort(flreg->bms, flreg->bm_n, sizeof flreg->bms[0], bookmark_cmp_ord)
;  }


folder* bookmark_undo
(  folder*      flreg
,  folder*      flport
,  int          num
)
   {  folder* flall = folder_merge(flreg, flport, TRUE)
   ;  int i = flall->bm_n
   ;  qsort(flall->bms, flall->bm_n, sizeof flall->bms[0], bookmark_cmp_ord)

   ;  while (--num >= 0 && --i >= 0)
      flall->bms[i].info |= BIT_PURGE

   ;  return flall
;  }


void bookmark_ungrep
(  folder*      flreg
,  mcxIO*       xfrc
,  const char*  destpat
,  const char*  markpat
)
   {  int i
   ;  for (i=0;i<flreg->bm_n;i++)
      {  bookmark* bm = flreg->bms+i
      ;  if
         (  (destpat && strstr(bm->dest->str, destpat))
         || (markpat && strstr(bm->mark->str, markpat))
         )
         bm->info |= BIT_PURGE
   ;  }
   }


void bookmark_list
(  const char* what
,  folder*   fl
)
   {  int i
   ;  int n_mark, n_char

   ;  fprintf(stdout, "--- %s\n", what)
   ;  if (!fl->bm_n)
      return

   ;  fputs(fl->bms[0].mark->str, stdout)
   ;  n_mark = 1
   ;  n_char = fl->bms[0].mark->len

   ;  for (i=1;i<fl->bm_n;i++)
      {  bookmark* bm = fl->bms+i
      ;  int len = strlen(bm->mark->str)

      ;  if (n_char + 1 + len <= 70)
            fputc(' ', stdout)
         ,  n_char += 1 + len
      ;  else
            fputc('\n', stdout)
         ,  n_char = 0

      ;  fputs(bm->mark->str, stdout)
   ;  }
      fputc('\n', stdout)
;  }


mcxstatus bookmark_show
(  folder*   fl
)
   {  int i
   ;  for (i=0;i<fl->bm_n;i++)
      {  bookmark* bm = fl->bms+i
      ;  if (bm->type == 'j' || bm->type == 'J')
         fprintf
         (  stdout
         ,  "j %-12s %s\n"
         ,  bm->mark->str
         ,  bm->dest->str
         )
      ;  else if (bm->type == 'e')
         fprintf
         (  stdout
         ,  "%c %-12s %s\n"
         ,  bm->type
         ,  ""
         ,  bm->dest->str
         )  
   ;  }
      return STATUS_OK
;  }



mcxstatus bookmark_write_jump
(  mcxIO* xfrc
,  const char* mark
,  const char* dest
)
   {  fprintf(xfrc->fp, "j,%s,%s\n", mark, dest)
   ;  fprintf(stderr, "added: %s -> %s\n", mark, dest)
   ;  return STATUS_OK
;  }



folder*  bookmark_parse
(  mcxIO* xf
,  mcxbool expansion
,  folder** flportpp
)
   {  mcxTing* line     =  mcxTingEmpty(NULL, 512)
   ;  folder* flreg     =  folder_new(100)
   ;  folder* flport    =  NULL
   ;  mcxstatus status  =  STATUS_OK
   ;  int lct           =  0
   ;  int ord           =  0

   ;  if (flportpp)
      flport = folder_new(10)

   ;  while
      (  STATUS_OK
      == (status = mcxIOreadLine(xf, line, MCX_READLINE_CHOMP))
      )
      {  mcxTing* typeting = NULL, *mark = NULL, *dest = NULL
      ;  int n_args = 0
      ;  char* sth  = NULL
      ;  mcxLink* lk = NULL, *root = NULL
      ;  unsigned char type = 0

      ;  lct++
      ;  if
         (  (sth = mcxStrChrAint(line->str, isspace, line->len))
         && (unsigned char) sth[0] == '#'
         )
         continue

      ;  if
         (!(root = mcxTokArgs(line->str, line->len, &n_args, MCX_TOK_DEL_WS)))
         {  mcxErr(us, "parse error at line %d (empty line?)\n", lct)
         ;  break
      ;  }

         lk       =  root->next
      ;  typeting =  lk->val

      ;  if (typeting->len != 1)
         {  mcxErr(us, "syntax error at line %d token %s", lct, typeting->str)
         ;  continue
      ;  }

         type = typeting->str[0]

      ;  lk = lk->next

      ;  if (type == 'j' && n_args == 3)
         {  mark  =  lk->val
         ;  dest  =  lk->next->val
      ;  }
         else if (type == 'e' && n_args == 2)
         {  mark  =  NULL
         ;  dest  =  lk->val
      ;  }
         else if (n_args > 3)
         {  mcxErr(us, "comma in destination at line %d - skipping", lct)
         ;  continue
      ;  }
         else
         {  mcxErr(us, "syntax error at line %d - skipping", lct)
         ;  continue
      ;  }

         if (type == 'j' && expansion)
         type = 'J'

      ;  if (type == 'e' && flport)
         folder_add(flport, type, 0, mark, dest, ord++)
      ;  else
         folder_add(flreg, type, 0, mark, dest, ord++)

      ;  mcxTingFree(&typeting)
      ;  root->val = NULL

      ;  mcxLinkFree(&root, mcxTingFree_v)
   ;  }

      if (flportpp)
      *flportpp = flport

   ;  return flreg
;  }


/* -  creates if not exists
 * -  sets file pointer at EOF
 * -  returns folder on success, NULL otherwise.
*/

folder* resource_prepare
(  mcxIO* xf
,  folder** flportpp
)
   {  mcxbool expansion =  flportpp ? FALSE : TRUE
   ;  const char* type  =  expansion ? "expansion" : "bookmark"
   ;  folder* fl = NULL

   ;  if (STATUS_OK == mcxIOopen(xf, RETURN_ON_FAIL))
      fl = bookmark_parse(xf, expansion, flportpp)    /* xf now positioned at EOF */
   ;  else
      {  mcxIOrenew(xf, NULL, "w")
      ;  if (mcxIOopen(xf, RETURN_ON_FAIL))
         mcxErr(us, "cannot create %s file %s", type, xf->fn->str)
      ;  else
         {  mcxTell(us, "created %s file %s", type, xf->fn->str)
         ;  fl = folder_new(0)
         ;  if (flportpp)
            *flportpp = folder_new(0)
      ;  }
      }
      return fl
;  }


int resolve_user
(  bookmark* bmx
,  int n_same
,  bookmark* bmz
)
   {  mcxTing* ans = mcxTingEmpty(NULL, 10)
   ;  mcxIO* xfin  =  mcxIOnew("-", "r")
   ;  int n_chosen = 0
   ;  int i
   ;  FILE* fask = stderr

   ;  mcxIOopen(xfin, EXIT_ON_FAIL)

   ;  fprintf
      (  fask
      ,  "multiple hits for mark %s - please choose (abort with x)\n"
      ,  bmx->mark->str
      )
   ;  for (i=0;i<n_same;i++)
      fprintf(fask, "%d. %s\n", i+1, bmx[i].dest->str)

   ;  fprintf(fask, "> ")
   ;  fflush(NULL)

   ;  mcxIOreadLine(xfin, ans, MCX_READLINE_CHOMP)
   ;  if (ans->len && sscanf(ans->str, "%d", &n_chosen) == 1)
      {  if (n_chosen <= 0 || n_chosen > n_same)
         n_chosen = -2
      ;  else
         n_chosen--
   ;  }
      else if (!strcmp(ans->str, "x"))
      n_chosen = -1
   ;  else
      n_chosen = -2

   ;  return n_chosen
;  }



mcxstatus attempt_jump
(  folder*  flreg
,  folder*  flexp
,  mcxTing* mark
,  mcxTing* sub
)
   {  mcxTing* dest     =  NULL
   ;  folder* flall     =  folder_merge(flreg, flexp, FALSE)
   ;  bookmark* bmx     =  NULL
   ;  bookmark* bmz     =  flall->bms+flall->bm_n
   ;  bookmark* bm      =  flall->bms
   ;  mcxstatus status  =  STATUS_FAIL

   ;  qsort(flall->bms, flall->bm_n, sizeof flall->bms[0], bookmark_cmp_mark)

   ;  while (bm < bmz)
      {  if
         (  (bm->type == 'j' || bm->type == 'J')
         && !strcmp(bm->mark->str, mark->str)
         )
         {  bmx = bm
         ;  break
      ;  }
         bm++
   ;  }

      while (1)
      {  if (!bmx)
         break

      ;  if
         (  bmx < bmz-1
         && !strcmp(bmx->mark->str, bmx[1].mark->str)
         )
         {  int n_same = 2
         ;  bm = bmx+1
         ;  while (bm<bmz-1 && !strcmp(bm->mark->str, bm[1].mark->str))
               n_same++
           ,   bm++

         ;  if (!favour)
            {  int ans = resolve_user(bmx, n_same, bmz)
            ;  if (ans < 0)
               {  if (ans == -1)
                  status = STATUS_ABORT
               ;  break
            ;  }
               else
               dest = bmx[ans].dest
         ;  }
            else     /* WARNING changed ordering in this segment */
            {  qsort(bmx, n_same, sizeof bmx[0], bookmark_cmp_mark_favour)
            ;  dest = bmx->dest
         ;  }
         }
         else
         dest  =  bmx->dest

      ;  if (!dest)
         break

      ;  if (sub)
         mcxTingPrintAfter(dest, "/%s", sub->str)
      ;  fprintf(stdout, "%s\n", dest->str)
      ;  status = STATUS_OK
      ;  break
   ;  }

   /* noteme: flall memleak */
      return status
;  }
      

mcxTing* get_cwd
(  void
)
   {  char buf[MY_PATH_MAX+1]
   ;  mcxTing* ret = NULL
   ;  if (use_cwd)
      {  if (!getcwd(buf, MY_PATH_MAX + 1))
         mcxErr(us, "cannot get current directory")
      ;  else
         ret = mcxTingNew(buf)
   ;  }
      else
      {  mcxIO* xfpipe = mcxIOnew("dummy-file-name", "r")
      ;  FILE*  p =  popen("pwd", "r")
      ;  if (p)
         {  mcxTing *pwd = mcxTingEmpty(NULL, 256)
         ;  xfpipe->fp = p       /* its a hack */
         ;  mcxIOreadLine(xfpipe, pwd, MCX_READLINE_CHOMP)
               /* xfpipe: (donot) fixme (yet) warning danger sign todo */
               /* readline: fixme check status */
         ;  if (!ferror(p))
            {  pclose(p)
            ;  xfpipe->fp = NULL
            ;  mcxIOfree(&xfpipe)
            ;  ret = pwd
         ;  }
            else
            mcxErr(us, "pwd does not want to play")
      ;  }
         else
         mcxErr(us, "cannot execute pwd")
   ;  }
      return ret
;  }


mcxstatus bookmark_add_jump
(  folder* flreg
,  folder* flexp
,  mcxIO* xfrc
,  const char* argv[]
,  int argc
,  int n_arg_trailing
)
   {  mcxTing* pwd
   ;  const char* newmark
   ;  const char* dest
   ;  mcxstatus status = STATUS_FAIL
   
   ;  dest = n_arg_trailing >= 2 ? argv[argc-1] : NULL

   ;  if (n_arg_trailing > 2)
      mcxErr(us, "ignoring trailing arguments")

   ;  newmark =   dest
                  ?  argv[argc-2]
                  :     n_arg_trailing == 1
                     ?  argv[argc-1]
                     :  NULL

   ;  if (!dest)
      {  if (!(pwd = get_cwd()))
         {  mcxErr(us, "cannot get current directory")
         ;  return STATUS_FAIL
      ;  }
         dest = pwd->str
   ;  }

      if (!newmark)
      {  const char* sep = strrchr(dest, '/')
      ;  if (!sep)
            mcxErr(us, "strange - no separator in %s", dest)
         ,  newmark = dest
      ;  else
         newmark = sep+1
   ;  }

      if (strchr(newmark, ','))
      {  mcxErr(us, "I cannot handle commas in mark")
      ;  status = STATUS_FAIL
   ;  }

      if (!status)
      status = bookmark_write_jump(xfrc, newmark, dest)

   ;  if (!status)
      {  bookmark_report("bookmark", flreg, newmark)
      ;  bookmark_report("expansion", flexp, newmark)
   ;  }

      mcxTingFree(&pwd)
   ;  return status
;  }


mcxstatus bookmark_add_portal
(  mcxIO* xfrc
,  mcxIO* xfport
,  const char* argv[]
,  int argc
,  int n_arg_trailing
)
   {  const char* dest= n_arg_trailing == 1 ? argv[argc-1] : NULL
   ;  folder* fl = NULL
   ;  mcxTing* pwd = NULL

   ;  if (n_arg_trailing > 1)
      {  mcxErr(us, "expecting [dest] trailing argument")
      ;  return STATUS_FAIL
   ;  }

      if (!dest)
      {  if (!(pwd = get_cwd()))
         {  mcxErr(us, "cannot get current directory")
         ;  return STATUS_FAIL
      ;  }
         dest = pwd->str
   ;  }

      if ((fl = expand_portal(xfport, dest)))
      {  fprintf(xfrc->fp, "e,%s\n", dest)
      ;  fprintf(stderr, "added flock of %d in portal %s\n", fl->bm_n, dest)
   ;  }
      else
      return STATUS_FAIL

   ;  return STATUS_OK
;  }



mcxstatus show_or_jump
(  folder* flreg
,  folder* flport
,  folder* flexp
,  const char* argv[]
,  int argc
,  int n_arg_trailing
)
   {  int arg_ofs = argc - n_arg_trailing
   ;  mcxTing* mark = n_arg_trailing > 0 ? mcxTingNew(argv[arg_ofs]) : NULL
   ;  mcxTing* sub  = n_arg_trailing > 1 ? mcxTingNew(argv[arg_ofs+1]) : NULL
   ;  mcxstatus status = STATUS_FAIL

   ;  if (!mark)
      {  qsort(flport->bms, flport->bm_n, sizeof flport->bms[0], bookmark_cmp_dest)
      ;  fprintf(stdout, "--- portals\n");
      ;  bookmark_show(flport)

      ;  qsort(flexp->bms, flexp->bm_n, sizeof flexp->bms[0], bookmark_cmp_dest)
      ;  fprintf(stdout, "--- expansions\n");
      ;  bookmark_show(flexp)

      ;  qsort(flreg->bms, flreg->bm_n, sizeof flreg->bms[0], bookmark_cmp_dest)
      ;  fprintf(stdout, "--- bookmarks\n");
      ;  return bookmark_show(flreg)
   ;  }

      if
      (  STATUS_FAIL
      == (status = attempt_jump(flreg, flexp, mark, sub))
      )
      mcxErr(us, "I don't know how to go to %s", mark->str)
   ;  return status
;  }


int main
(  int   argc
,  const char* argv[]
)
   {  const char* h     =  getenv("HOME")
   ;  const char* home  =  h ? h : ""
   ;  mcxTing* fnrc     =  mcxTingPrint(NULL, "%s/%s", home, ".apparixrc")
   ;  mcxTing* fnport   =  mcxTingPrint(NULL, "%s/%s", home, ".apparixexpand")
   ;  mcxIO*   xfrc     =  mcxIOnew(fnrc->str, "r+")
   ;  mcxIO*   xfport   =  mcxIOnew(fnport->str, "r+")
   ;  mcxstatus status  =  STATUS_FAIL

   ;  folder*  flreg     =  NULL
   ;  folder*  flexp    =  NULL
   ;  folder*  flport   =  NULL

                  /* only allow one of these modes */
   ;  enum
      {  MODE_DEFAULT
      ,  MODE_LIST
      ,  MODE_SHOWARG
      ,  MODE_PURGE
      ,  MODE_UNDO
      ,  MODE_PURGE_MARK
      ,  MODE_SQUASH_MARK
      ,  MODE_SQUASH_DEST
      ,  MODE_REHASH
      ,  MODE_BACKUP
      ,  MODE_ADD_JUMP
      ,  MODE_ADD_PORTAL
      ,  MODE_SHELL
      }

   ;  int n_mode        =  0
   ;  int mode          =  MODE_DEFAULT
   ;  const char* mode_arg =  NULL
   ;  int num           =  0

   ;  mcxstatus parseStatus = STATUS_OK
   ;  mcxOption* opts, *opt
   ;  int n_arg_read = 0
   ;  int n_arg_trailing = 0

   ;  mcxOptAnchorSortById(options, sizeof(options)/sizeof(mcxOptAnchor) -1)

   ;  if ( !(
      opts =
      mcxOptExhaust
      (options, (char**) argv, argc, 1, &n_arg_read, &parseStatus)
         )  )
      exit(0)

   ;  for (opt=opts;opt->anch;opt++)
      {  mcxOptAnchor* anch = opt->anch

      ;  switch(anch->id)
         {  case MY_OPT_HELP
         :  case MY_OPT_APROPOS
         :  mcxOptApropos(stdout, us, syntax, 20, MCX_OPT_DISPLAY_SKIP, options)
         ;  return 0
         ;

            case  MY_OPT_VERSION
         :  show_version()
         ;  exit(0)
         ;

            case  MY_OPT_ADD_JUMP
         :  mode = MODE_ADD_JUMP; n_mode++
         ;  break
         ;

            case  MY_OPT_ADD_PORTAL
         :  mode = MODE_ADD_PORTAL; n_mode++
         ;  break
         ;

            case  MY_OPT_SQUASH_MARK2
         :  mode = MODE_SQUASH_MARK; n_mode++
         ;  break
         ;

            case  MY_OPT_SQUASH_MARK
         :  mode = MODE_SQUASH_MARK; n_mode++
         ;  break
         ;

            case  MY_OPT_SQUASH_DEST
         :  mode = MODE_SQUASH_DEST; n_mode++
         ;  break
         ;

            case  MY_OPT_BACKUP_NAMED
         :  mode = MODE_BACKUP; n_mode++
         ;  mode_arg = opt->val
         ;  break
         ;

            case  MY_OPT_LIST
         :  mode = MODE_LIST; n_mode++
         ;  break
         ;

            case  MY_OPT_DUMP
         :  mode = MODE_BACKUP; n_mode++
         ;  mode_arg = "-"
         ;  break
         ;

            case  MY_OPT_BACKUP
         :  mode = MODE_BACKUP; n_mode++
         ;  break
         ;

            case  MY_OPT_REHASH
         :  mode = MODE_REHASH; n_mode++
         ;  break
         ;

            case  MY_OPT_UNDO
         :  mode = MODE_UNDO; n_mode++
         ;  num = atoi(opt->val)
         ;  break
         ;

            case  MY_OPT_PURGE
         :  mode = MODE_PURGE; n_mode++
         ;  mode_arg = opt->val
         ;  break
         ;

            case  MY_OPT_PURGE_MARK
         :  mode = MODE_PURGE_MARK; n_mode++
         ;  mode_arg = opt->val
         ;  break
         ;

            case  MY_OPT_FAVOUR
         :  favour = mcxTingNew(opt->val)
         ;  break
         ;

            case  MY_OPT_SHELL
         :  mode = MODE_SHELL; n_mode++
         ;  break
         ;

            case  MY_OPT_CWD
         :  use_cwd = TRUE
         ;  break
         ;
         }
      }

      if (n_mode > 1)
      mcxDie(1, us, "arguments specify multiple modes")

   ;  if (mode == MODE_SHELL)
      {  const char** exline = shell_examples
      ;  while (*exline)
            fprintf(stdout, "%s\n", *exline)
         ,  exline++
      ;  return STATUS_OK
   ;  }
      else if (mode == MODE_BACKUP)
      return bookmark_backup_rcfile(xfrc->fn->str, mode_arg)

   ;  if (!(flreg = resource_prepare(xfrc, &flport)))
      return STATUS_FAIL

   ;  if (mode == MODE_REHASH)
      return bookmark_resync_portal(flport, xfport)

   ;  if (!(flexp = resource_prepare(xfport, NULL)))
      return STATUS_FAIL

   ;  n_arg_trailing = argc - n_arg_read - 1

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

   ;  switch(mode)
      {  case MODE_PURGE
      :  bookmark_ungrep(flreg, xfrc, mode_arg, NULL)
      ;  status = bookmark_resync_rc(flreg, flport, xfrc)
      ;  break
      ;
         case MODE_UNDO
      :  {  folder* flall = bookmark_undo(flreg, flport, num)
         ;  status = bookmark_resync_rc(flall, NULL, xfrc)
      ;  }
         break
      ;
         case MODE_LIST
      :  bookmark_list("expansions", flexp)
      ;  bookmark_list("bookmarks", flreg)
      ;  break
      ;
         case MODE_PURGE_MARK
      :  bookmark_ungrep(flreg, xfrc, NULL, mode_arg)
      ;  status = bookmark_resync_rc(flreg, flport, xfrc)
      ;  break
      ;
         case MODE_SQUASH_DEST
      :  bookmark_squash(flreg, xfrc, SQUASH_DEST)
      ;  status = bookmark_resync_rc(flreg, flport, xfrc)
      ;  break
      ;
         case MODE_SQUASH_MARK
      :  bookmark_squash(flreg, xfrc, SQUASH_MARK)
      ;  status = bookmark_resync_rc(flreg, flport, xfrc)
      ;  break
      ;
         case MODE_ADD_JUMP
      :  status = bookmark_add_jump(flreg, flexp, xfrc, argv, argc, n_arg_trailing)
      ;  break
      ;
         case MODE_ADD_PORTAL
      :  status = bookmark_add_portal(xfrc, xfport, argv, argc, n_arg_trailing)
      ;  break
      ;
         case MODE_DEFAULT
      :  status = show_or_jump(flreg, flport, flexp, argv, argc, n_arg_trailing)
      ;  break
   ;  }

      mcxIOclose(xfrc)
   ;  mcxIOclose(xfport)
   ;  return status
;  }



