/* (c) Copyright 2005 Stijn van Dongen
 *
 * This file is part of apparix.  You can redistribute and/or modify apparix
 * under the terms of the GNU General Public License; either version 2 of the
 * License or (at your option) any later version.  You should have received a
 * copy of the GPL along with apparix, in the file COPYING.
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
,  MY_OPT_SQUASH_MARK
,  MY_OPT_SQUASH_DEST
,  MY_OPT_UNGREP
,  MY_OPT_UNGREP_MARK
,  MY_OPT_REHASH
,  MY_OPT_SHOWALL
,  MY_OPT_SHOW
,  MY_OPT_CWD
,  MY_OPT_SHELL
,  MY_OPT_VERSION
,  MY_OPT_HELP
,  MY_OPT_APROPOS
}  ;


static const char* me    =  "apparix";

const char* shell_examples[] =
{  "BASH-style functions\n---"
,  "function to () {"
,  "   if test \"$2\"; then"
,  "     apparix \"$1\" \"$2\" && cd \"`cat \"$HOME/.apparixresult\"`\" ;"
,  "   else"
,  "     apparix \"$1\" && cd \"`cat \"$HOME/.apparixresult\"`\" ;"
,  "   fi"
,  "}"
,  "function bm () {"
,  "   if test \"$2\"; then"
,  "      apparix --add-mark \"$1\" \"$2\" && cd \"`cat \"$HOME/.apparixresult\"`\" ;"
,  "   elif test \"$1\"; then"
,  "      apparix --add-mark \"$1\" && cd \"`cat \"$HOME/.apparixresult\"`\" ;"
,  "   else"
,  "      apparix --add-mark && cd \"`cat \"$HOME/.apparixresult\"`\" ;"
,  "   fi"
,  "}"
,  "function portal () {"
,  "   if test \"$1\"; then"
,  "      apparix --add-portal \"$1\" && cd \"`cat \"$HOME/.apparixresult\"`\" ;"
,  "   else"
,  "      apparix --add-portal && cd \"`cat \"$HOME/.apparixresult\"`\" ;"
,  "   fi"
,  "}"
,  "# function to generate list of completions from .apparixrc"
,  "function _apparix_aliases () {"
,  "   cur=$2"
,  "   COMPREPLY=()"
,  "   COMPREPLY=( $( (echo $cur ; cat $HOME/.apparix{rc,expand}) | \\"
,  "   grep \"j,$cur\" | cut -f2 -d, ) )"
,  "   return 0"
,  "}"
,  "# command to register the above to expand when the 'to' command's args are"
,  "# being expanded"
,  "complete -F _apparix_aliases to"
,  "---\nCSH-style aliases\n---"
,  "alias to   'apparix \\!* && cd `cat $HOME/.apparixresult`'"
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
   ,  "for repeated marks take the last occurring destination"
   }
,  {  "--squash-dest"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SQUASH_DEST
   ,  NULL
   ,  "for repeated destinations take the last occurring mark"
   }
,  {  "-rid"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_UNGREP
   ,  "<pat>"
   ,  "delete bookmarks where destination matches <pat>"
   }
,  {  "-rid-mark"
   ,  MCX_OPT_HASARG
   ,  MY_OPT_UNGREP_MARK
   ,  "<pat>"
   ,  "delete bookmarks where mark matches <pat>"
   }
,  {  "--rehash"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_REHASH
   ,  NULL
   ,  "re-expand portal bookmarks"
   }
,  {  "--quo"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_SHOWALL
   ,  NULL
   ,  "show all jump and portal destinations"
   }
,  {  "--cwd"
   ,  MCX_OPT_DEFAULT
   ,  MY_OPT_CWD
   ,  NULL
   ,  "use getcwd(3) rather than reading pwd(1) output"
   }
,  {  "-quo"
   ,  MCX_OPT_HASARG | MCX_OPT_HIDDEN
   ,  MY_OPT_SHOW
   ,  "<j|p|e|a>"
   ,  "(not implemented) jump mark, portal mark, portal expansions, all"
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
"(c) Copyright 2005, Stijn van Dongen. apparix comes with NO WARRANTY\n"
"to the extent permitted by law. You may redistribute copies of apparix under\n"
"the terms of the GNU General Public License.\n"
      ,  apxNumTag
      ,  apxDateTag
      )
;  }



typedef struct
{  int         type              /*  'j', 'e' */
;  mcxTing*    mark
;  mcxTing*    dest
;  mcxbits     stib
;  int         ord
;
}  bookmark    ;


typedef struct
{  bookmark*   bms
;  int         bm_n
;  int         bm_N
;
}  folder      ;


int bookmark_cmp_ord
(  const void* bm1
,  const void* bm2
)
   {  return ((bookmark*)bm1)->ord - ((bookmark*)bm2)->ord
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


int expand_portal
(  mcxIO* xfport
,  const char* dest
)
   {  DIR * dir = opendir(dest)
   ;  struct dirent *de
   ;  mcxstatus status = STATUS_FAIL
   ;  mcxTing* fqname = mcxTingEmpty(NULL, 512)
   ;  int ct = 0

   ;  while (1)
      {  if (!dir)
         {  mcxErr(me, "cannot open directory %s", dest)
         ;  break
      ;  }
         while ((de = readdir(dir)))
         {  struct stat fstat
         ;  const char* dname = de->d_name
         ;  mcxTingPrint(fqname, "%s/%s", dest, dname)
         ;  if ((unsigned char) dname[0] == '.')
            continue
         ;  if (!strncmp(dname, "CVS", 3))
            continue
         ;  if (lstat(fqname->str, &fstat) < 0)
            {  mcxErr(me, "error accessing node %s", fqname->str)
            ;  break
         ;  }
            if (!S_ISDIR(fstat.st_mode))
            continue
         ;  fprintf(xfport->fp, "j,%s,%s\n", dname, fqname->str)
         ;  ct++
      ;  }
         closedir(dir)

      ;  if (de)
         break
      ;  status = STATUS_OK
      ;  break
   ;  }
      mcxTingFree(&fqname)
   ;  return status ? -1 : ct
;  }



mcxstatus bookmark_resync_portal
(  folder*     fl
,  mcxIO*      xfport
)
   {  int i, fail = 0
   ;  const char* portname = xfport->fn->str
   ;  int ct_expansion = 0
   ;  int ct_portal = 0

   ;  mcxIOclose(xfport)
   ;  {  mcxTing* cmd = mcxTingPrint(NULL, "mv %s %s.bu", portname, portname)
      ;  struct stat fstat
      ;  int mv
      ;  if (lstat(portname, &fstat) < 0)
         {  if (errno == ENOENT)
            mcxTell(me, "portal file %s will be newly created", portname)
         ;  else
            mcxErr(me, "cannot access %s, fingers crossed", portname)
      ;  }
         else if ((mv = system(cmd->str)))
         mcxDie(1, me, "panic cannot create %s.bu", portname)
   ;  }

      mcxIOrenew(xfport, NULL, "w")
   ;  if (mcxIOopen(xfport, EXIT_ON_FAIL))
      mcxDie(1, me, "panic - portal file is gone, backup in %s.bu", portname)

   ;  for (i=0;i<fl->bm_n;i++)
      {  bookmark* bm = fl->bms+i
      ;  int ct
      ;  if (bm->type == 'e')
         {  if ((ct = expand_portal(xfport, bm->dest->str)) < 0)
            fail++
         ;  else
               ct_expansion += ct
            ,  ct_portal++
      ;  }
      }

   ;  if (fail)
      mcxErr(me, "portal expansion yielded %d errors", fail)

   ;  mcxTell
      (me, "expanded %d portals to %d destinations", ct_portal, ct_expansion)
   ;  return fail ? STATUS_FAIL : STATUS_OK
;  }


mcxstatus bookmark_resync_rc
(  folder*     fl
,  mcxIO*      xfrc
)
   {  int i
   ;  const char* rcname = xfrc->fn->str
   ;  int n_purged = 0
   ;  mcxIOclose(xfrc)
   ;  {  mcxTing* cmd = mcxTingPrint(NULL, "mv %s %s.bu", rcname, rcname)
      ;  int mv = system(cmd->str)
      ;  if (mv)
         mcxDie(1, me, "panic cannot create %s.bu", xfrc->fn->str)
   ;  }

      mcxIOrenew(xfrc, NULL, "w")
   ;  if (mcxIOopen(xfrc, EXIT_ON_FAIL))
      mcxDie(1, me, "panic - rc file is gone, backup in %s.bu", rcname)

   ;  for (i=0;i<fl->bm_n;i++)
      {  bookmark* bm   =  fl->bms+i
      ;  FILE* fp       =  bm->stib & 1 ? stdout : xfrc->fp

      ;  if (bm->type == 'j')
         fprintf(fp, "%c,%s,%s\n", bm->type, bm->mark->str, bm->dest->str)
      ;  else if (bm->type == 'e')
         fprintf(fp, "%c,%s\n", bm->type, bm->dest->str)

      ;  if (bm->stib & 1)
         n_purged++
   ;  }
      mcxIOclose(xfrc)
   ;  fprintf(stdout, "purged a flock of %d\n", n_purged)
   ;  return STATUS_OK
;  }


enum
{  SQUASH_DEST = 1444
,  SQUASH_MARK
}  ;

mcxstatus bookmark_squash
(  folder*      fl
,  mcxIO*       xfrc
,  int          mode
)
   {  int j
   ;  mcxbool dest = mode == SQUASH_DEST
   ;  int (*cmp)(const void*, const void*)
      =     dest
         ?  bookmark_cmp_dest
         :  bookmark_cmp_mark

   ;  if (!fl->bm_n)
      return STATUS_OK
   ;  qsort(fl->bms, fl->bm_n, sizeof fl->bms[0], cmp)

   ;  for (j=1;j<fl->bm_n;j++)
      {  bookmark* bmj = fl->bms+j
      ;  bookmark* bmi = bmj - 1
                                       /* fixme convoluted logic */
      ;  if (!dest && bmi->type == 'e' && bmj->type == 'e')
         continue
      ;  if
         (  ( dest && !strcmp(bmi->dest->str, bmj->dest->str))
         || (!dest && !strcmp(bmi->mark->str, bmj->mark->str) )
         )
         bmi->stib |= 1
   ;  }

      qsort(fl->bms, fl->bm_n, sizeof fl->bms[0], bookmark_cmp_ord)
   ;  return bookmark_resync_rc(fl, xfrc)
;  }


mcxstatus bookmark_ungrep
(  folder*      fl
,  mcxIO*       xfrc
,  const char*  destpat
,  const char*  markpat
)
   {  int i
   ;  for (i=0;i<fl->bm_n;i++)
      {  bookmark* bm = fl->bms+i
      ;  if
         (  (destpat && strstr(bm->dest->str, destpat))
         || (markpat && strstr(bm->mark->str, markpat))
         )
         bm->stib |= 1
   ;  }
      return bookmark_resync_rc(fl, xfrc)
;  }


void* bookmark_init
(  void* bm_v
)
   {  bookmark* bm = bm_v
   ;  bm->type =  0
   ;  bm->mark =  mcxTingEmpty(NULL, 12)
   ;  bm->dest =  mcxTingEmpty(NULL, 40)
   ;  bm->stib =  0
   ;  bm->ord  =  0
   ;  return bm
;  }


mcxstatus bookmark_show
(  folder*   fl
,  const char* mode
)
   {  int i
   ;  qsort(fl->bms, fl->bm_n, sizeof fl->bms[0], bookmark_cmp_dest)
   ;  for (i=0;i<fl->bm_n;i++)
      {  bookmark* bm = fl->bms+i
      ;  if (bm->type == 'j')
         fprintf
         (  stdout
         ,  "%c %-12s %s\n"
         ,  bm->type
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


mcxstatus bookmark_add_portal
(  mcxIO* xfrc
,  mcxIO* xfport
,  const char* dest
,  folder* flrc
)
   {  int ct = expand_portal(xfport, dest)
   ;  if (ct < 0)
      mcxErr(me, "some error occurred")
   ;  else if (ct)
      {  fprintf(xfrc->fp, "e,%s\n", dest)
      ;  fprintf(stdout, "added flock of %d in portal %s\n", ct, dest)
   ;  }
      else if (!ct)
      mcxTell(me, "nothing to expand from %s", dest)
      
   ;  return STATUS_OK
;  }



mcxstatus bookmark_add_jump
(  mcxIO* xfrc
,  const char* mark
,  const char* dest
)
   {  fprintf(xfrc->fp, "j,%s,%s\n", mark, dest)
   ;  fprintf(stdout, "added: %s -> %s\n", mark, dest)
   ;  return STATUS_OK
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
         ,  bm_N
         ,  2*bm_N
         ,  sizeof fl->bms[0]
         ,  bookmark_init
         ,  EXIT_ON_FAIL
         )
      ;  bm_N = 2*bm_N
   ;  }

      bm       =  fl->bms+bm_n++
   ;  bm->type =  type

   ;  mcxTingWrite(bm->dest, dest->str)
   ;  if (mark)
      mcxTingWrite(bm->mark, mark->str)

   ;  bm->ord  =  ord
   ;  fl->bm_n = bm_n
   ;  fl->bm_N = bm_N
;  }


folder* folder_merge
(  folder* fl1
,  folder* fl2
)
   {  folder* fl        =  folder_new(100)
   ;  bookmark* bm      =  NULL
   ;  int ord           =  0

   ;  bm = fl1->bms
   ;  while (bm<fl1->bms+fl1->bm_n)
      {  folder_add(fl, bm->type, bm->mark, bm->dest, ord++)
      ;  bm++
   ;  }

      bm = fl2->bms
   ;  while (bm<fl2->bms+fl2->bm_n)
      {  folder_add(fl, bm->type, bm->mark, bm->dest, ord++)
      ;  bm++
   ;  }

      return fl
;  }


folder*  bookmark_parse
(  mcxIO* xfrc
)
   {  mcxTing* line     =  mcxTingEmpty(NULL, 512)
   ;  folder* fl        =  folder_new(100)
   ;  mcxstatus status  =  STATUS_OK
   ;  int lct           =  0
   ;  int ord           =  0

   ;  while
      (  STATUS_OK
      == (status = mcxIOreadLine(xfrc, line, MCX_READLINE_CHOMP))
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
         {  mcxErr(me, "parse error at line %d (empty line?)\n", lct)
         ;  break
      ;  }

         lk       =  root->next
      ;  typeting =  lk->val

      ;  if (typeting->len != 1)
         {  mcxErr(me, "syntax error at line %d token %s", lct, typeting->str)
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
         else
         {  mcxErr(me, "syntax error at line %d token %s", lct, typeting->str)
         ;  continue
      ;  }

         folder_add(fl, type, mark, dest, ord++)

      ;  mcxTingFree(&typeting)
      ;  root->val = NULL

      ;  mcxLinkFree(&root, mcxTingFree_v)
   ;  }

      return fl
;  }


/* -  creates if not exists
 * -  sets file pointer at EOF
 * -  returns folder on success, NULL otherwise.
*/

folder* resource_prepare
(  mcxIO* xf
,  const char* type
)
   {  folder* fl = NULL
   ;  if (STATUS_OK == mcxIOopen(xf, RETURN_ON_FAIL))
      fl = bookmark_parse(xf)    /* xf now positioned at EOF */
   ;  else
      {  mcxIOrenew(xf, NULL, "w")
      ;  if (mcxIOopen(xf, RETURN_ON_FAIL))
         mcxErr(me, "cannot create %s file %s", type, xf->fn->str)
      ;  else
            mcxTell(me, "created %s file %s", type, xf->fn->str)
         ,  fl = folder_new(0)
   ;  }
      return fl
;  }


mcxstatus attempt_jump
(  folder*  flrc
,  folder*  flport
,  mcxIO*   xfresult
,  mcxTing* mark
,  mcxTing* sub
)
   {  mcxTing* dest     =  NULL
   ;  folder* fl        =  folder_merge(flrc, flport)
   ;  bookmark* bmx     =  NULL
   ;  bookmark* bmz     =  fl->bms+fl->bm_n
   ;  bookmark* bm      =  fl->bms
   ;  mcxstatus status  =  STATUS_FAIL

   ;  qsort(fl->bms, fl->bm_n, sizeof fl->bms[0], bookmark_cmp_mark)

   ;  while (bm < bmz)
      {  if (bm->type == 'j' && !strcmp(bm->mark->str, mark->str))
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
         {  int n_dup = 1, n_chosen = 0
         ;  mcxTing* ans = mcxTingEmpty(NULL, 10)
         ;  mcxIO* xfin  =  mcxIOnew("-", "r")

         ;  mcxIOopen(xfin, EXIT_ON_FAIL)
         ;  bm =  bmx

         ;  fprintf
            (  stdout
            ,  "multiple hits for mark %s - please choose (abort with x)\n"
            ,  mark->str
            )
         ;  fprintf
            (stdout, "1. %s\n", bmx->dest->str)

         ;  while (bm<bmz-1 && !strcmp(bm->mark->str, bm[1].mark->str))
            fprintf(stdout, "%d. %s\n", ++n_dup, (++bm)->dest->str)

         ;  fprintf(stdout, "> ")
         ;  fflush(NULL)

         ;  mcxIOreadLine(xfin, ans, MCX_READLINE_CHOMP)
         ;  if (ans->len && sscanf(ans->str, "%d", &n_chosen) == 1)
            {  fprintf(stdout, "%d\n", n_chosen)
            ;  if (n_chosen > 0 && n_chosen <= n_dup)
               dest = bmx[n_chosen-1].dest
         ;  }
            else if (!strcmp(ans->str, "x"))
            {  status = STATUS_ABORT
            ;  break
         ;  }
         }
         else
         dest  =  bmx->dest

      ;  if (!dest)
         break

      ;  if (sub)
         mcxTingPrintAfter(dest, "/%s", sub->str)
      ;  fprintf(xfresult->fp, "%s\n", dest->str)
      ;  fprintf
         (  stdout
         ,  "%s\n"
         ,  dest->str
         )
      ;  mcxIOclose(xfresult)
      ;  status = STATUS_OK
      ;  break
   ;  }

      /* qsort(fl->bms, fl->bm_n, sizeof fl->bms[0], bookmark_cmp_ord) */
      /* ^ only needed if fl is ever reused */

      return status
;  }


int main
(  int   argc
,  char* argv[]
)
   {  const char* h     =  getenv("HOME")
   ;  const char* home  =  h ? h : ""
   ;  mcxTing* fnrc     =  mcxTingPrint(NULL, "%s/%s", home, ".apparixrc")
   ;  mcxTing* fnport   =  mcxTingPrint(NULL, "%s/%s", home, ".apparixexpand")
   ;  mcxTing* fngt     =  mcxTingPrint(NULL, "%s/%s", home, ".apparixresult")
   ;  mcxIO*   xfrc     =  mcxIOnew(fnrc->str, "r+")
   ;  mcxIO*   xfport   =  mcxIOnew(fnport->str, "r+")
   ;  mcxIO*   xfresult     =  mcxIOnew(fngt->str, "w")
   ;  mcxstatus status  =  STATUS_FAIL

   ;  mcxTing* mark     =  NULL
   ;  mcxTing* sub      =  NULL
   ;  folder*  flrc     =  NULL
   ;  folder*  flport   =  NULL
#define MY_PATH_MAX 1024
   ;  char buf[MY_PATH_MAX+1]

                  /* fixme warn if incompatible arguments */
   ;  const char* showarg  =  NULL
   ;  const char* ungrep   =  NULL
   ;  const char* ungrep_mark  =  NULL
   ;  mcxbool squash_mark  =  FALSE
   ;  mcxbool squash_dest  =  FALSE
   ;  mcxbool rehash       =  FALSE
   ;  mcxbool add_jump     =  FALSE
   ;  mcxbool add_portal   =  FALSE
   ;  mcxbool shell        =  FALSE

   ;  mcxbool use_cwd      =  FALSE

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
         :  mcxOptApropos(stdout, me, syntax, 20, MCX_OPT_DISPLAY_SKIP, options)
         ;  return 0
         ;

            case  MY_OPT_VERSION
         :  show_version()
         ;  exit(0)
         ;

            case  MY_OPT_ADD_JUMP
         :  add_jump = TRUE
         ;  break
         ;

            case  MY_OPT_ADD_PORTAL
         :  add_portal = TRUE
         ;  break
         ;

            case  MY_OPT_SQUASH_MARK
         :  squash_mark = TRUE
         ;  break
         ;

            case  MY_OPT_SQUASH_DEST
         :  squash_dest = TRUE
         ;  break
         ;

            case  MY_OPT_REHASH
         :  rehash = TRUE
         ;  break
         ;

            case  MY_OPT_UNGREP
         :  ungrep = opt->val
         ;  break
         ;

            case  MY_OPT_UNGREP_MARK
         :  ungrep_mark = opt->val
         ;  break
         ;

            case  MY_OPT_SHELL
         :  shell = TRUE
         ;  break
         ;

            case  MY_OPT_CWD
         :  use_cwd = TRUE
         ;  break
         ;

            case  MY_OPT_SHOW
         :  showarg = opt->val
         ;  break
         ;

            case  MY_OPT_SHOWALL
         :  showarg = "a"
         ;  break
         ;
         }
      }


      if (shell)
      {  const char** exline = shell_examples
      ;  while (*exline)
            fprintf(stdout, "%s\n", *exline)
         ,  exline++
      ;  return STATUS_OK
   ;  }

      if (!(flrc = resource_prepare(xfrc, "bookmark")))
      return STATUS_FAIL

   ;  if (rehash)
      return bookmark_resync_portal(flrc, xfport)

   ;  if (!(flport = resource_prepare(xfport, "expansion")))
      return STATUS_FAIL

   ;  n_arg_trailing = argc - n_arg_read - 1

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

   ;  if (showarg)
      {  status = bookmark_show(flport, showarg)
      ;  fprintf(stdout, "---\n");
      ;  return bookmark_show(flrc, showarg)
   ;  }

      if (ungrep)
      {  return bookmark_ungrep(flrc, xfrc, ungrep, ungrep_mark)
   ;  }

      if (ungrep_mark)
      {  return bookmark_ungrep(flrc, xfrc, NULL, ungrep_mark)
   ;  }

      if (squash_dest)
      {  return bookmark_squash(flrc, xfrc, SQUASH_DEST)
   ;  }

      if (squash_mark)
      {  return bookmark_squash(flrc, xfrc, SQUASH_MARK)
   ;  }

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */
      if (add_jump)
      {  const char* dest, *newmark
      ;  mcxTing* pwd
      
      ;  dest     =     n_arg_trailing >= 2 ? argv[argc-1] : NULL

      ;  if (n_arg_trailing > 2)
         mcxErr(me, "ignoring trailing arguments")

      ;  newmark  =     dest
                     ?  argv[argc-2]
                     :     n_arg_trailing == 1
                        ?  argv[argc-1]
                        :  NULL
      ;  pwd  =  mcxTingEmpty(NULL, 256)

      ;  if (!dest)
         {  if (use_cwd)
            {  if (!getcwd(buf, MY_PATH_MAX + 1))
               mcxDie(1, me, "cannot get current directory")
            ;  else
               dest = buf
         ;  }
            else
            {  mcxIO* xfpipe = mcxIOnew("dummy-file-name", "r")
            ;  FILE*  p =  popen("pwd", "r")
            ;  if (p)
               {  xfpipe->fp = p       /* its a hack */
               ;  mcxIOreadLine(xfpipe, pwd, MCX_READLINE_CHOMP)
                     /* xfpipe: (donot) fixme (yet) warning danger sign todo */
                     /* readline: fixme check status */
               ;  pclose(p)
               ;  xfpipe->fp = NULL
               ;  dest = pwd->str
            ;  }
               else
               mcxDie(1, me, "cannot execute pwd")
            ;  mcxIOfree(&xfpipe)
         ;  }
         }

         if (!newmark)
         {  const char* sep = strrchr(dest, '/')
         ;  if (!sep)
               mcxErr(me, "strangeî - no separator in %s", dest)
            ,  newmark = dest
         ;  else
            newmark = sep+1
      ;  }

         status = bookmark_add_jump(xfrc, newmark, dest)
      ;  mcxTingFree(&pwd)
      ;  return status
   ;  }

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */
      if (add_portal)
      {  const char* dest= n_arg_trailing == 1 ? argv[argc-1] : NULL

      ;  if (n_arg_trailing > 1)
         mcxDie(1, me, "expecting [dest] trailing argument")

      ;  if (!dest)
         {  if (!getcwd(buf, MY_PATH_MAX + 1))
            mcxDie(1, me, "cannot get current directory")
         ;  dest = buf
      ;  }
         return bookmark_add_portal(xfrc, xfport, dest, flrc)
   ;  }
/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

      if (n_arg_trailing > 0) 
      mark = mcxTingNew(argv[n_arg_read+1])

   ;  if (n_arg_trailing > 1) 
      sub = mcxTingNew(argv[n_arg_read+2])

   ;  if (!mark)
      {  status = bookmark_show(flport, showarg)
      ;  fprintf(stdout, "---\n");
      ;  return bookmark_show(flrc, showarg)
   ;  }

      mcxIOopen(xfresult, EXIT_ON_FAIL)
   ;  if
      (  STATUS_FAIL
      == (status = attempt_jump(flrc, flport, xfresult, mark, sub))
      )
      mcxErr(me, "I don't know how to go to %s", mark->str)
   ;  else if (status == STATUS_ABORT)
      ;
      /* NOTHING (documentation of status option) */

      mcxIOclose(xfresult)
   ;  mcxIOclose(xfrc)
   ;  return status
;  }



