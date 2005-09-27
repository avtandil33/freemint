/*
 * $Id$
 * 
 * XaAES - XaAES Ain't the AES (c) 1992 - 1998 C.Graham
 *                                 1999 - 2003 H.Robbers
 *                                        2004 F.Naumann & O.Skancke
 *
 * A multitasking AES replacement for FreeMiNT
 *
 * This file is part of XaAES.
 *
 * XaAES is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * XaAES is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XaAES; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * FILE SELECTOR IMPLEMENTATION
 * 
 * This give a non-modal windowed file selector for internal
 * and exported use, with slightly extended pattern matching
 * from the basic GEM spec.
 */

#include RSCHNAME

#include "xa_fsel.h"
#include "xa_global.h"

#include "c_window.h"
#include "form.h"
#include "k_main.h"
#include "matchpat.h"
#include "menuwidg.h"
#include "nkcc.h"
#include "obtree.h"
#include "rectlist.h"
#include "scrlobjc.h"
#include "util.h"
#include "widgets.h"

#include "xa_graf.h"
#include "xa_rsrc.h"

#include "mint/pathconf.h"
#include "mint/stat.h"


#define FLAG_FILLED	0x00000001
#define FLAG_DIR	0x00000002
#define FLAG_EXE	0x00000004
#define FLAG_LINK	0x00000008
#define FLAG_SDIR	0x00000010

#define FSLIDX_NAME	0
#define FSLIDX_SIZE	1
#define FSLIDX_TIME	2
#define FSLIDX_DATE	3
#define FSLIDX_FLAG	4
#define FSLIDX_RIGHT	5

static char fs_paths[DRV_MAX][NAME_MAX+2];
static char fs_patterns[23][16];


static void
add_pattern(char *pattern)
{
	int i;
	if (pattern && *pattern)
	{
		for (i = 0; i < 23; i++)
		{
			if (fs_patterns[i][0])
			{
				if (!stricmp(pattern, fs_patterns[i]))
				{
					break;
				}
			}
			else
			{
				strncpy(fs_patterns[i], pattern, 16);
				fs_patterns[i][15] = '\0';
				break;
			}
		}
	}
}

void
init_fsel(void)
{
	int i;
	for (i = 0; i < DRV_MAX; i++)
		fs_paths[i][0] = 0;

	for (i = 0; i < 23; i++)
		fs_patterns[i][0] = '\0';

	add_pattern("*");

	for (i = 0; i < 23; i++)
	{
		if (cfg.Filters[i])
			add_pattern(cfg.Filters[i]);
	}
}

static char *
getsuf(char *f)
{
	char *p;

	if ((p = strrchr(f, '.')) != 0L)
	{
		if (strchr(p, '/') == 0L
		    && strchr(p, '\\') == 0L)
			/* didnt ran over slash? */
			return p+1;
	}
	/* no suffix  */
	return 0L;
}
static void
strins(char *d, const char *s, long here)
{
	long slen = strlen(s);
	long dlen = strlen(d);
	char t[512];
	
	if (here > dlen)
		here = dlen;

	strncpy(t, d + here, sizeof(t));
	strncpy(d + here, s, slen);
	strcpy(d + here + slen, t);
}

static struct scroll_entry *
fs_cwd(struct scroll_info *list, char *cwd, short incsel)
{
	struct fsel_data *fs = list->data;
	struct scroll_entry *dir_ent = NULL;
	struct sesetget_params p; //seget_entrybyarg p;
	
	if (cwd)
		strcpy(cwd, fs->root);

	if (fs->selected_dir)
	{
		struct scroll_entry *this = fs->selected_dir;

		switch (incsel)
		{
			/*
			 * Include fs->selected_dir only when not in treeview
			 */
			case 0:
			{
				if (this && !fs->treeview)
					this = this->up;
				break;
			}
			/*
			 * 
			 */
			case 1:
			{
				if (this)
				{
					short xstate;
					list->get(list, this, SEGET_XSTATE, &xstate);
					if (!(xstate & OS_OPENED))
						this = this->up;
				}
				break;
			}
			case 2:
			default:;
		}
			
		dir_ent = this;

		if (cwd)
		{
			long len = strlen(cwd);
			p.idx = -1;		
			while (this)
			{
				if (list->get(list, this, SEGET_TEXTPTR, &p))
				{
					strins(cwd, "\\", len);
					strins(cwd, p.ret.ptr, len);
				}
				this = this->up;
			}
		}
	}
	return dir_ent;
}

static void
set_dir(struct scroll_info *list)
{
	struct fsel_data *fs = list->data;
	fs_cwd(list, fs->path, 1);
	display_widget(list->lock, list->wi, get_widget(list->wi, XAW_TITLE), list->pw ? list->pw->rect_list.start : NULL);
}

static struct scroll_entry *
get_selorboxed(struct scroll_info *list)
{
	struct sesetget_params p; //seget_entrybyarg p;

	p.idx = 0;
	p.arg.state.method = ANYBITS;
	p.arg.state.bits = OS_SELECTED | OS_BOXED;
	p.arg.state.mask = OS_SELECTED | OS_BOXED;
	p.level.flags = 0;
	p.level.maxlevel = -1;
	p.level.curlevel = 0;

	list->get(list, NULL, SEGET_ENTRYBYSTATE, &p);
	return p.e;
}

static void
set_selected(struct scroll_info *list, struct scroll_entry *this)
{
	list->set(list, this, SESET_SELECTED, 0, NORMREDRAW);
}
static struct scroll_entry *
get_selected(struct scroll_info *list)
{
	struct sesetget_params p; //seget_entrybyarg p;

	p.idx = 0;
	p.arg.state.method = EXACTBITS;
	p.arg.state.bits = OS_SELECTED;
	p.arg.state.mask = OS_SELECTED;
	p.level.flags = 0;
	p.level.maxlevel = -1;
	p.level.curlevel = 0;

	list->get(list, NULL, SEGET_ENTRYBYSTATE, &p);
	return p.e;
}

static bool
fs_prompt(SCROLL_INFO *list, char *file)
{
	struct fsel_data *fs = list->data;
	SCROLL_ENTRY *parent, *s;
	bool ret = false;
	struct sesetget_params seget, p; //seget_entrybyarg seget, p;

	if (!(parent = fs_cwd(list, NULL, 1)))
	{
		parent = NULL;
		s = list->start;
	}
	else
		s = parent->down;

// 	display("selected %lx, s %lx, parent %lx", fs->selected_dir, s, parent);

	/* Not if filename empty or list empty */
	if (*file && s)
	{
		int match = 0;

		seget.e = s;
		seget.level.flags = ENT_VISIBLE; //|ENT_ISROOT;
		seget.level.maxlevel = -1; //-1;
		seget.level.curlevel = 0;
		p.idx = -1;
// 		display("file '%s'", file);
		while (seget.e)
		{
			list->get(list, seget.e, SEGET_TEXTPTR, &p);
// 			display(" and '%s'", p.ret.ptr ? p.ret.ptr : "Heh!?");
			if (p.ret.ptr && (match = match_pattern(p.ret.ptr, file, true)))
				break;
			list->get(list, seget.e, SEGET_NEXTENT, &seget);
		}
		s = seget.e;
// 		display("matched %d agains '%s' (%lx)", match, (long)s ? p.ret.ptr : "none", list->cur);
		if (s)
		{
			long uf;

			list->get(list, s, SEGET_USRFLAGS, &uf);

			if ((uf & FLAG_DIR))
			{
				fs->selected_dir = s;
				fs->selected_file = NULL;
			}
			else
			{
				fs->selected_dir = s->up;
				fs->selected_file = s;
			}

			list->set(list, s, SESET_CURRENT, 0, NOREDRAW);
			list->set(list, NULL, SESET_MULTISEL, 0, NOREDRAW);

			list->set(list, NULL, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16), NORMREDRAW);
			
			if (match == 1)
				list->set(list, s, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_BOXED, NORMREDRAW);
			else if (match == 2)
				list->set(list, s, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_SELECTED, NORMREDRAW);

			list->vis(list, s, NORMREDRAW);
			set_dir(list);
			ret = true;
		}
		else
		{
			fs->selected_file = NULL;
			if (parent)
			{
				list->set(list, parent, SESET_CURRENT, 0, NOREDRAW);
				fs->selected_dir = parent;
				set_dir(list);
				list->set(list, parent, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_SELECTED, NORMREDRAW);
				list->vis(list, parent, NORMREDRAW);
			}
			else
			{
				if ((s = get_selorboxed(list)))
				{
					fs->selected_dir = s->up;
					set_dir(list);
					list->set(list, s, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16), NORMREDRAW);
					ret = true;
				}
				else if (fs->kbdnav)
				{
					s = list->start;
					list->set(list, s, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_SELECTED, NORMREDRAW);
					list->set(list, s, SESET_CURRENT, 0, NOREDRAW);
					ret = true;
				}
				else
					list->set(list, NULL, SESET_CURRENT, 0, NOREDRAW);
			}
		}
	}
	else
	{
		if (!*file)
			fs->tfile = false;

		if (!fs->tfile)
		{
			fs->selected_file = NULL;
			if (parent)
			{
				list->set(list, parent, SESET_CURRENT, 0, NOREDRAW); //list->cur = parent;
				fs->selected_dir = parent;
				set_dir(list);
				list->set(list, NULL, SESET_STATE, ((long)OS_BOXED << 16), NORMREDRAW);
				list->set(list, parent, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_SELECTED, NORMREDRAW);
				list->vis(list, parent, NORMREDRAW);
			}
			else
			{
				if ((s = get_selorboxed(list)))
				{
					fs->selected_dir = s->up;
					set_dir(list);
					list->set(list, s, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16), NORMREDRAW);
					ret = true;
				}
				else if (fs->kbdnav)
				{
					s = list->start;
					list->set(list, s, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_SELECTED, NORMREDRAW);
					list->set(list, s, SESET_CURRENT, 0, NOREDRAW);
					ret = true;
				}
				else
					list->set(list, NULL, SESET_CURRENT, 0, NOREDRAW);
			}
		}
	}
	return ret;
}

static short
sort_by_date(struct xattr *x1, struct xattr *x2)
{
	short tmp;
	/*
	 * Sorting by date.
	 * return -1 if x1 is greater (newer, after) than x2
	 * return  1 if x1 is less (older, before) than x2
	 * return  0 if x1 and x2 has exact same date.
	 */	
	if ((tmp = ((x1->mdate >> 9) & 127) - ((x2->mdate >> 9) & 127) ))	/* Year */
		return (tmp > 0) ? 1 : -1;
	if ((tmp = ((x1->mdate >> 5) & 15) - ((x2->mdate >> 5) & 15) ))		/* Month */
		return tmp > 0 ? 1 : -1;
	if ((tmp = (x1->mdate & 31) - (x2->mdate & 31)))			/* day */
		return tmp > 0 ? 1 : -1;
	if ((tmp = ((x1->mtime >> 11) & 31) - ((x2->mtime >> 11) & 31)))	/* hour */
		return tmp > 0 ? 1 : -1;
	if ((tmp = ((x1->mtime >> 5) & 63) - ((x2->mtime >> 5) & 63)))		/* min */
		return tmp > 0 ? 1 : -1;
	if ((tmp = (x1->mtime & 63) - (x2->mtime & 63)))			/* sec */
		return tmp > 0 ? 1 : -1;

	return 0;
}

static short
sort_by_ext(char *n1, char *n2)
{
	short ret = 0;

	/*
	 * return -1 if n1 is greater (newer, after) than n2
	 * return  1 if n1 is less (older, before) than n2
	 * return  0 if n1 and n2 is equal.
	 */
	n1 = getsuf(n1);
	n2 = getsuf(n2);
	
	if (n1 && n2)
		ret = stricmp(n1, n2);
	else if (!n1 && !n2)
		ret = 0;
	else if (!n1)
		ret = -1;
	else
		ret = 1;

	return ret;
}
static short
sort_by_name(char *n1, char *n2)
{	
	return stricmp(n1, n2);
}

// typedef bool sort_compare(SCROLL_ENTRY *s1, SCROLL_ENTRY *s2);
/*
 * This is called by the slist functions
 */
static bool
sortbydate(struct scroll_info *list, struct scroll_entry *e1, struct scroll_entry *e2)
{
	long uf1, uf2;
	short tmp = 0;
	struct xattr *x1, *x2;
	struct sesetget_params p1, p2; //seget_entrybyarg p1, p2;

	list->get(list, e1, SEGET_USRFLAGS, &uf1);
	list->get(list, e2, SEGET_USRFLAGS, &uf2);

	p1.idx = p2.idx = -1;
	list->get(list, e1, SEGET_TEXTPTR, &p1);
	list->get(list, e2, SEGET_TEXTPTR, &p2);
	
	if ((uf1 & FLAG_SDIR) || (uf2 & FLAG_SDIR))
	{
		if ((uf1 & FLAG_SDIR) && (uf2 & FLAG_SDIR))
		{
			return sort_by_name(p1.ret.ptr, p2.ret.ptr) > 0 ? true : false;
		}
		else if (!(uf2 & FLAG_SDIR))
			return false;
		else
			return true;
	}

	uf1 &= FLAG_DIR;
	uf2 &= FLAG_DIR;
	if (uf1 < uf2)
	{
		/* folders in front */
		return true;
	}
	
	if (uf1 == uf2)
	{
		list->get(list, e1, SEGET_USRDATA, &x1);
		list->get(list, e2, SEGET_USRDATA, &x2);
		
		if ((tmp = sort_by_date(x1, x2)))
			return tmp > 0 ? false : true;
		
		if ((tmp = sort_by_ext(p1.ret.ptr, p2.ret.ptr)))
			return tmp > 0 ? true : false;

		if ((tmp = sort_by_name(p1.ret.ptr, p2.ret.ptr)))
			return tmp > 0 ? true : false;

		/*
		 * If same date, suffix and name, we let size decide...
		 */
		if (!S_ISDIR(x1->mode) && !S_ISDIR(x2->mode))
		{
			if (x1->size > x2->size)
				return true;
		}
	}
	return false;
}
static bool
sortbyname(struct scroll_info *list, struct scroll_entry *e1, struct scroll_entry *e2)
{
	long uf1, uf2;
	short tmp = 0;
	struct xattr *x1, *x2;
	struct sesetget_params p1, p2; //seget_entrybyarg p1, p2;

	list->get(list, e1, SEGET_USRFLAGS, &uf1);
	list->get(list, e2, SEGET_USRFLAGS, &uf2);

	p1.idx = p2.idx = -1;
	list->get(list, e1, SEGET_TEXTPTR, &p1);
	list->get(list, e2, SEGET_TEXTPTR, &p2);
	
	if ((uf1 & FLAG_SDIR) || (uf2 & FLAG_SDIR))
	{
		if ((uf1 & FLAG_SDIR) && (uf2 & FLAG_SDIR))
		{
			return sort_by_name(p1.ret.ptr, p2.ret.ptr) > 0 ? true : false;
		}
		else if (!(uf2 & FLAG_SDIR))
			return false;
		else
			return true;
	}

	uf1 &= FLAG_DIR;
	uf2 &= FLAG_DIR;
	if (uf1 < uf2)
	{
		/* folders in front */
		return true;
	}
	
	if (uf1 == uf2)
	{
		list->get(list, e1, SEGET_USRDATA, &x1);
		list->get(list, e2, SEGET_USRDATA, &x2);
		
		if ((tmp = sort_by_name(p1.ret.ptr, p2.ret.ptr)))
			return tmp > 0 ? true : false;
		
		if ((tmp = sort_by_ext(p1.ret.ptr, p2.ret.ptr)))
			return tmp > 0 ? true : false;

		if ((tmp = sort_by_date(x1, x2)))
			return tmp > 0 ? false : true;

		/*
		 * If same date, suffix and name, we let size decide...
		 */
		if (!S_ISDIR(x1->mode) && !S_ISDIR(x2->mode))
		{
			if (x1->size > x2->size)
				return true;
		}
	}
	return false;
}
static bool
sortbyext(struct scroll_info *list, struct scroll_entry *e1, struct scroll_entry *e2)
{
	long uf1, uf2;
	short tmp = 0;
	struct xattr *x1, *x2;
	struct sesetget_params p1, p2; //seget_entrybyarg p1, p2;

	list->get(list, e1, SEGET_USRFLAGS, &uf1);
	list->get(list, e2, SEGET_USRFLAGS, &uf2);

	p1.idx = p2.idx = -1;
	list->get(list, e1, SEGET_TEXTPTR, &p1);
	list->get(list, e2, SEGET_TEXTPTR, &p2);
	
	if ((uf1 & FLAG_SDIR) || (uf2 & FLAG_SDIR))
	{
		if ((uf1 & FLAG_SDIR) && (uf2 & FLAG_SDIR))
		{
			return sort_by_name(p1.ret.ptr, p2.ret.ptr) > 0 ? true : false;
		}
		else if (!(uf2 & FLAG_SDIR))
			return false;
		else
			return true;
	}

	uf1 &= FLAG_DIR;
	uf2 &= FLAG_DIR;
	if (uf1 < uf2)
	{
		/* folders in front */
		return true;
	}
	
	if (uf1 == uf2)
	{
		list->get(list, e1, SEGET_USRDATA, &x1);
		list->get(list, e2, SEGET_USRDATA, &x2);
		
		if ((tmp = sort_by_ext(p1.ret.ptr, p2.ret.ptr)))
			return tmp > 0 ? true : false;
		
		if ((tmp = sort_by_name(p1.ret.ptr, p2.ret.ptr)))
			return tmp > 0 ? true : false;

		if ((tmp = sort_by_date(x1, x2)))
			return tmp > 0 ? false : true;

		/*
		 * If same date, suffix and name, we let size decide...
		 */
		if (!S_ISDIR(x1->mode) && !S_ISDIR(x2->mode))
		{
			if (x1->size > x2->size)
				return true;
		}
	}
	return false;
}
static bool
sortbysize(struct scroll_info *list, struct scroll_entry *e1, struct scroll_entry *e2)
{
	long uf1, uf2;
	short tmp = 0;
	struct xattr *x1, *x2;
	struct sesetget_params p1, p2; //seget_entrybyarg p1, p2;

	list->get(list, e1, SEGET_USRFLAGS, &uf1);
	list->get(list, e2, SEGET_USRFLAGS, &uf2);

	p1.idx = p2.idx = -1;
	list->get(list, e1, SEGET_TEXTPTR, &p1);
	list->get(list, e2, SEGET_TEXTPTR, &p2);
	
	if ((uf1 & FLAG_SDIR) || (uf2 & FLAG_SDIR))
	{
		if ((uf1 & FLAG_SDIR) && (uf2 & FLAG_SDIR))
		{
			return sort_by_name(p1.ret.ptr, p2.ret.ptr) > 0 ? true : false;
		}
		else if (!(uf2 & FLAG_SDIR))
			return false;
		else
			return true;
	}

	uf1 &= FLAG_DIR;
	uf2 &= FLAG_DIR;
	if (uf1 < uf2)
	{
		/* folders in front */
		return true;
	}
	
	if (uf1 == uf2)
	{
		list->get(list, e1, SEGET_USRDATA, &x1);
		list->get(list, e2, SEGET_USRDATA, &x2);
		
		if (!S_ISDIR(x1->mode) && !S_ISDIR(x2->mode))
		{
			if (x1->size > x2->size)
				return true;
			else if (x1->size < x2->size)
				return false;
		}

		if ((tmp = sort_by_name(p1.ret.ptr, p2.ret.ptr)))
			return tmp > 0 ? true : false;
		if ((tmp = sort_by_ext(p1.ret.ptr, p2.ret.ptr)))
			return tmp > 0 ? true : false;
		if ((tmp = sort_by_date(x1, x2)))
			return tmp > 0 ? false : true;
	}
	return false;
}
static bool
sortbynothing(struct scroll_info *list, struct scroll_entry *e1, struct scroll_entry *e2)
{
	long uf1, uf2;
	struct sesetget_params p1, p2; //seget_entrybyarg p1, p2;

	list->get(list, e1, SEGET_USRFLAGS, &uf1);
	list->get(list, e2, SEGET_USRFLAGS, &uf2);

	p1.idx = p2.idx = -1;
	list->get(list, e1, SEGET_TEXTPTR, &p1);
	list->get(list, e2, SEGET_TEXTPTR, &p2);
	
	if ((uf1 & FLAG_SDIR) || (uf2 & FLAG_SDIR))
	{
		if ((uf1 & FLAG_SDIR) && (uf2 & FLAG_SDIR))
		{
			return sort_by_name(p1.ret.ptr, p2.ret.ptr) > 0 ? true : false;
		}
		else if (!(uf2 & FLAG_SDIR))
			return false;
		else
			return true;
	}
	return true;
}

static scrl_compare *sorters[] =
{
	sortbyname,
	sortbydate,
	sortbysize,
	sortbyext,
	sortbynothing,
};

/* yields true if case sensitive */
static bool
inq_xfs(struct fsel_data *fs, char *p, char *dsep)
{	
	long c, t;

	c = fs->fcase = d_pathconf(p, DP_CASE);
	t = fs->trunc = d_pathconf(p, DP_TRUNC);

	DIAG((D_fsel, NULL, "inq_xfs '%s': case = %ld, trunc = %ld", p, c, t));

	if (dsep)
	{
		* dsep    = '\\';
		*(dsep+1) = 0;
	}

	return !(c == 1 && t == 2);
}

static void
add_slash(char *p, char *to)
{
	char *pl = p + strlen(p) - 1;

	if (*pl != '/' && *pl != '\\')
		strcat(p, to);
}

static bool
executable(char *nam)
{
	char *ext = getsuf(nam);

	return (ext
		/* The mintlib does almost the same :-) */
		&& (   !stricmp(ext, "ttp") || !stricmp(ext, "prg")
		    || !stricmp(ext, "tos") || !stricmp(ext, "g")
		    || !stricmp(ext, "sh")  || !stricmp(ext, "bat")
		    || !stricmp(ext, "gtp") || !stricmp(ext, "app")
		    || !stricmp(ext, "acc")));
}
static bool
disabled_exe(char *nam)
{
	char *ext = getsuf(nam);

	return (ext
		/* The mintlib does almost the same :-) */
		&& (   !stricmp(ext, "prx")
		    || !stricmp(ext, "gtx") || !stricmp(ext, "apx")
		    || !stricmp(ext, "acx")));
}
static void
set_file(struct fsel_data *fs, const char *fn)
{
	XA_TREE *wt = get_widget(fs->wind, XAW_TOOLBAR)->stuff;

	DIAG((D_fsel, NULL, "set_file: fs.file='%s', wh=%d", fn ? fn : fs->file, fs->wind->handle));

	/* fixup the cursor edit position */
#if 0
	/* FIXME: the edit_object() doesn't work in case the client
	 * didn't call the form_do().
	 *
	 * We should call something like the below instead
	 * of direct e.pos adjustments.
	 */
	{
		short newpos;
		edit_object(lock, wt->owner, ED_END, wt, wt->tree, FS_FILE, 0, &newpos);
		strcpy(fs.file, fn); /* set the edit field text */
		edit_object(lock, wt->owner, ED_INIT, wt, wt->tree, FS_FILE, 0, &newpos);
	}
#else
	if (fn)
		strcpy(fs->file, fn); /* set the edit field text */
	wt->e.pos = strlen(fs->file);
#endif
	/* redraw the toolbar file object */
	redraw_toolbar(0, fs->wind, FS_FILE);
}
#define FSIZE_MAX 20

static char *months[] =
{
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Okt",
	"Nov",
	"Dec",
};
static char *faccess[] =
{
	"---",
	"--x",
	"-w-",
	"-wx",
	"r--",
	"r-x",
	"rw-",
	"rwx",
};					

static struct xa_wtxt_inf norm_txt =
{
 WTXT_NOCLIP,
/* id  pnts efx   fgc      bgc */
 {  -1,  -1,   MD_TRANS, 0, G_BLACK, G_WHITE },	/* Normal */
 {  -1,  -1,   MD_TRANS, 0, G_WHITE, G_BLACK },/* Selected */
 {  -1,  -1,   MD_TRANS, 0, G_BLACK, G_WHITE },	/* Highlighted */

};

static struct xa_wtxt_inf exe_txt =
{
 WTXT_NOCLIP,
/* id  pnts efx   fgc      bgc */
 {  -1,  -1,   MD_TRANS, 0, G_RED, G_WHITE },	/* Normal */
 {  -1,  -1,   MD_TRANS, 0, G_YELLOW, G_WHITE },/* Selected */
 {  -1,  -1,   MD_TRANS, 0, G_BLACK, G_WHITE },	/* Highlighted */

};
static struct xa_wtxt_inf dexe_txt =
{
 WTXT_NOCLIP,
/* id  pnts efx   fgc      bgc */
 {  -1,  -1,   MD_TRANS, 0, G_LRED, G_WHITE },	/* Normal */
 {  -1,  -1,   MD_TRANS, 0, G_RED, G_WHITE },	/* Selected */
 {  -1,  -1,   MD_TRANS, 0, G_BLACK, G_WHITE },	/* Highlighted */

};

static struct xa_wtxt_inf dir_txt =
{
 WTXT_NOCLIP,
/* id  pnts efx   fgc      bgc */
 {  -1,  -1,   MD_TRANS, 0, G_LCYAN, G_WHITE },/* Normal */
 {  -1,  -1,   MD_TRANS, 0, G_CYAN, G_WHITE },	/* Selected */
 {  -1,  -1,   MD_TRANS, 0, G_BLACK, G_WHITE },	/* Highlighted */

};

static void
destroy_xattr(void *data)
{
	kfree(data);
}

static void
read_directory(struct fsel_data *fs, SCROLL_INFO *list, SCROLL_ENTRY *dir_ent)
{
	bool csens;
	OBJECT *obtree = fs->form->tree;
	char nm[NAME_MAX+2 + FSIZE_MAX+2  + 40 + 12];
	short xstate;
	long i, rep;

	if (dir_ent)
		list->get(list, dir_ent, SEGET_XSTATE, &xstate);

	if (dir_ent && (xstate & OS_OPENED))
	{
		struct sesetget_params p;

		p.e = dir_ent;
		p.level.flags = ENT_ISROOT;
		p.level.maxlevel = -1;
		p.level.curlevel = 0;
		p.arg.state.mask = ~(OS_SELECTED|OS_BOXED);
		p.arg.state.bits = 0;
		p.arg.state.method = ANYBITS;
		list->set(list, dir_ent, SESET_M_STATE, (long)&p, NORMREDRAW);

		/*
		 * If this directory is already opened, just close it
		 */
		list->set(list, dir_ent, SESET_OPEN, 0, NORMREDRAW);
		fs->selected_file = NULL;
		set_dir(list);
		dir_ent = NULL;
		return;
	} 
	else
	{
		if (dir_ent)
		{
			long here;
			SCROLL_ENTRY *this;
			struct sesetget_params p; //seget_entrybyarg p;
			
			strcpy(fs->path, fs->root);
			here = strlen(fs->path);
			this = dir_ent;
			p.idx = -1;
			while (this)
			{
				list->get(list, this, SEGET_TEXTPTR, &p);
				strins(fs->path, "\\", here);
				strins(fs->path, p.ret.ptr, here);
				
				this = this->up;
			}
			/* If realtime build, open entry here */
			if (fs->rtbuild)
				list->set(list, dir_ent, SESET_OPEN, 1, true);

			list->get(list, dir_ent, SEGET_USRFLAGS, &here);
			if (here & 1)
			{
				list->set(list, dir_ent, SESET_OPEN, 1, NORMREDRAW);
				return;
			}
		}
		else
			strcpy(fs->path, fs->root);

		csens = inq_xfs(fs, fs->path, fs->fslash);
		i = d_opendir(fs->path, 0);
		
		DIAG((D_fsel, NULL, "Dopendir -> %lx", i));
	
		if (i > 0)
		{
			struct xattr xat, *x;

			while (d_xreaddir(NAME_MAX, i, nm, &xat, &rep) == 0)
			{
				struct scroll_content sc = {{0}};
				char *nam = nm+4;
				bool dir = S_ISDIR(xat.mode);
				bool sln = S_ISLNK(xat.mode);
				bool match = true;

				if (!(x = kmalloc(sizeof(*x))))
					continue;

				if (!csens)
					strupr(nam);

				if (sln) 
				{
					char fulln[NAME_MAX+2];

					strcpy(fulln, fs->path);
					strcat(fulln, nam);

					DIAG((D_fsel, NULL, "Symbolic link: Fxattr on '%s'", fulln));
					f_xattr(0, fulln, &xat);
					dir = (xat.mode & 0xf000) == S_IFDIR;

					DIAG((D_fsel, NULL, "After Fxattr dir:%d", dir));
				}
				*x = xat;

				if (!dir)
					match = match_pattern(nam, fs->fs_pattern, false);
				else
				{
					if (!strcmp(nam, ".") || !strcmp(nam, ".."))
					{
						if (dir_ent)
							continue;
						sc.usr_flags |= FLAG_SDIR;
					}
					if (fs->treeview)
						sc.xstate |= OS_NESTICON;
					sc.fnt = &dir_txt;
				}

				if (match)
				{
					OBJECT *icon = NULL;
					char *s = nam + strlen(nam) + 1;

					sc.t.text = nam;
					
					if (dir)
					{
						sc.xflags = OF_OPENABLE;
						sc.usr_flags |= FLAG_DIR;
						icon = obtree + FS_ICN_DIR;
					}
					else
					{
						sc.xflags = 0;
						
						if (/*executable(nam) ||*/ (xat.mode & S_IXUGO))
						{
							if (sln)
								icon = obtree + FS_ICN_SYMLINK;
							else
								icon = obtree + FS_ICN_PRG;
							sc.fnt = &exe_txt;
						}
						else if (disabled_exe(nam))
						{
							icon = obtree + FS_ICN_PRG;
							sc.fnt = &dexe_txt;
						}
						else
						{
							icon = obtree + FS_ICN_FILE;
							sc.fnt = &norm_txt;
						}
					}
					if (sln)
					{
						sc.fnt->n.e |= ITALIC;
						sc.fnt->s.e |= ITALIC;
					}
					else
					{
						sc.fnt->n.e &= ~ITALIC;
						sc.fnt->s.e &= ~ITALIC;
					}
					
					if (!dir)
					{
						if (xat.size < (1024UL * 1024) )
							sprintf(s, 20, "%lu", xat.size);
						else if (xat.size < (1024UL * 1024 * 10))
							sprintf(s, 20, "%lu KB", xat.size >> 10);
						else
							sprintf(s, 20, "%lu MB", xat.size >> 20);
					}
					else
						sprintf(s, 20, "<dir>");
					
					s += strlen(s) + 1;
		
					sprintf(s, 20, "%02d:%02d:%02d", (xat.mtime >> 11) & 31, (xat.mtime >> 5) & 63, xat.mtime & 63);
					s += strlen(s) + 1;
					{
						unsigned short year, month, day;

						year = ((xat.mdate >> 9) & 127) + 1980;
			
			
						month = ((xat.mdate >> 5) & 15) - 1;
						if (month < 1 || month > 12)
							month = 1;
						
						day = xat.mdate & 31;
						
						sprintf(s, 20, "%02d %s %04d", day, months[month], year);
					}
					s += strlen(s) + 1;
					sprintf(s, 12, "%s%s%s", faccess[(xat.mode >> 6) & 7], faccess[(xat.mode >> 3) & 7], faccess[xat.mode & 7]);
					
					if (xat.mode & S_ISUID)
					{
						if (s[2] == 'x')
							s[2] = 's';
						else
							s[2] = 'S';
					}
					if (xat.mode & S_ISGID)
					{
						if (s[5] == 'x')
							s[5] = 's';
						else
							s[5] = 'S';
					}
					if (xat.mode & S_ISVTX)
					{
						if (s[8] == 'x')
							s[8] = 't';
						else
							s[8] = 'T';
					}
// 					display("%s", s);

					sc.icon = icon;
					sc.t.strings = 5;
					sc.data = x;
					sc.data_destruct = destroy_xattr;
					list->add(list, dir_ent, sorters[fs->sort], &sc, dir_ent ? SEADD_PRIOR|SEADD_CHILD : SEADD_PRIOR, SETYP_AMAL, fs->rtbuild ? NORMREDRAW : NOREDRAW);
				}
			}
			d_closedir(i);
		}
		if (dir_ent)
		{
			/*
			 * Set bit 0 in user flags to indicate that this directory is read
			 */
			long uf;
			list->get(list, dir_ent, SEGET_USRFLAGS, &uf);
			uf |= FLAG_FILLED;
			list->set(list, dir_ent, SESET_USRFLAGS, uf, 0);
		}
	}
	
	if (!fs->rtbuild)
	{
		if (dir_ent)
			list->set(list, dir_ent, SESET_OPEN, 1, NORMREDRAW);
		else
		{
		#if 0
			if (!fs->tfile && fs->kbdnav && list->start)
			{
				list->set(list, list->start, SESET_CURRENT, 0, NOREDRAW); //list->cur = list->start;
				list->set(list, list->cur, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_SELECTED, NOREDRAW);
			}
		#endif
			list->slider(list, NORMREDRAW);
			list->redraw(list, NULL);
		}
	}
	else if (dir_ent)
	{
		list->set(list, dir_ent, SESET_OPEN, 1, true);
	}
}
/*
 * Re-load a file_selector listbox
 * HR: version without the big overhead of separate dir/file lists
       and 2 quicksorts + the moving around of all that.
       Also no need for the Mintlib. (Mintbind only).
 */

static void
refresh_filelist(enum locks lock, struct fsel_data *fs, SCROLL_ENTRY *dir_ent)
{
	OBJECT *form = fs->form->tree;
	OBJECT *sl;
	SCROLL_INFO *list;

	sl = form + FS_LIST;
	DIAG((D_fsel, NULL, "refresh_filelist: fs = %lx, obtree = %lx, sl = %lx",
		fs, fs->form->tree, sl));
	list = object_get_slist(sl);
	add_slash(fs->root, fs->fslash);

#ifdef FS_DBAR
	{
		TEDINFO *tx;

		tx = ob_spec(form + FS_CASE);
		sprintf(tx->te_ptext, 32, "%ld", fs->fcase);

		tx = ob_spec(form + FS_TRUNC);
		sprintf(tx->te_ptext, 32, "%ld", fs->trunc);

		redraw_toolbar(lock, fs->wind, FS_DBAR);
	}
#endif
	DIAG((D_fsel, NULL, "refresh_filelist: fs.path='%s',fs_pattern='%s'",
		fs->root, fs->fs_pattern));

	/* Clear out current file list contents */
	if (!dir_ent)
	{
		fs->selected_file = NULL;
		while (list->start)
		{
			list->start = list->del(list, list->start, false);
		}
		if (fs->rtbuild)
			list->redraw(list, NULL);
	}
	
	graf_mouse(HOURGLASS, NULL, NULL, false);
	read_directory(fs, list, dir_ent);
	graf_mouse(ARROW, NULL, NULL, false);
}

static void
CE_refresh_filelist(enum locks lock, struct c_event *ce, bool cancel)
{
	if (!cancel)
	{
		struct fsel_data *fs = ce->ptr1;
		struct scroll_info *list = ce->ptr2;
		refresh_filelist(lock, fs, NULL);
		fs_prompt(list, fs->file);
	}
}

#define DRIVELETTER(i)	(i + ((i < 26) ? 'A' : '1' - 26))
static int
fsel_drives(OBJECT *m, int drive)
{
	int drv = 0, drvs = 0;
	int d = FSEL_DRVA;
	unsigned long dmap = b_drvmap();

	while (dmap)
	{
		if (dmap & 1)
		{
			m[d].ob_state &= ~OS_CHECKED;

			if (drv == drive)
				m[d].ob_state |= OS_CHECKED;

			sprintf(m[d++].ob_spec.free_string, 32, "  %c:", DRIVELETTER(drv));
			drvs++;
		}
		dmap >>= 1;
		drv++;
	}

	if (drvs & 1)
		m[d-1].ob_width = m[FSEL_DRVBOX].ob_width;

	do {
		m[d].ob_flags |= OF_HIDETREE;
		/* prevent finding those. */
		*(m[d].ob_spec.free_string + 2) = '~';
	}
	while (m[d++].ob_next != FSEL_DRVBOX);

	m[FSEL_DRVBOX].ob_height = ((drvs + 1) / 2) * screen.c_max_h;

	sprintf(m[FSEL_DRV].ob_spec.free_string, 32, " %c:", DRIVELETTER(drive));
	return drvs;
}
			
static void
fsel_filters(OBJECT *m, char *pattern)
{
	char p[16];
	int d = FSEL_PATA;
	int i = 0;

	add_pattern(pattern);

	if (pattern && *pattern && fs_patterns[0][0])
	{
		while (i < 23 && fs_patterns[i][0])
		{
			//fs_patterns[i][15] = 0;
			m[d].ob_state &= ~OS_CHECKED;
			if (stricmp(pattern, fs_patterns[i]) == 0)
			{
				m[d].ob_state |= OS_CHECKED;
			}
			sprintf(m[d++].ob_spec.free_string, 128, "  %s", fs_patterns[i++]);
		}
	
		do
		{
			m[d].ob_flags |= OF_HIDETREE;
		}
		while (m[d++].ob_next != FSEL_PATBOX);

		m[FSEL_PATBOX].ob_height = i * screen.c_max_h;
	}
	strncpy(p, pattern, 15);
	p[15] = 0;
	sprintf(m[FSEL_FILTER].ob_spec.free_string, 128, " %s", p);
}

/* HR: a little bit more exact. */
/* Dont need form as long as everything is global 8-} */
static void
fs_updir(struct scroll_info *list)
{
	struct fsel_data *fs = list->data;
	int drv;
	bool atroot = false;
	char outof[128];

	outof[0] = '\0';

	if (*fs->root)
	{
		int s = strlen(fs->root) - 1;

		if (fs->root[s] == '/' || fs->root[s] == '\\')
			fs->root[s--] = '\0';

		DIAG((D_fsel, NULL, "fs_updir '%s'", fs->root));	

		while (  s
		      && fs->root[s] != ':'
		      && fs->root[s] != '/'
		      && fs->root[s] != '\\')
			s--;

		if (fs->root[s] == ':')
		{
			if (fs->root[s + 1] == '\0')
				atroot = true;
		
			fs->root[++s] = *fs->fslash;
		}
		if (!atroot)
			strcpy(outof, &fs->root[s + 1]);

		fs->root[++s] = 0;
		DIAG((D_fsel,NULL,"   -->   '%s'", fs->root));
	}

	if (!atroot)
	{
		if ((drv = get_drv(fs->root)) >= 0)
			strcpy(fs_paths[drv], fs->root);

		/*
		 * Moving root, need to reset relatives..
		 */
		fs->selected_file = fs->selected_dir = NULL;
		set_dir(list);
		refresh_filelist(fsel, fs, NULL);
		fs_prompt(list, atroot ? fs->file : outof);
	}
}

static void
fs_closer(struct scroll_info *list, bool rdrw)
{
	struct fsel_data *fs = list->data;

// 	if ( /*fs->clear_on_folder_change &&*/ !fs->tfile)
// 		set_file(fs, "");

	set_file(fs, fs->ofile);
	fs_updir(list);
}

static void
fs_enter_dir(struct fsel_data *fs, struct scroll_info *list, struct scroll_entry *dir_ent)
{
	struct sesetget_params p; //seget_entrybyarg p;
	int drv;

	add_slash(fs->root, fs->fslash);
	
	if (!fs->treeview)
	{
		p.idx = -1;
		list->get(list, dir_ent, SEGET_TEXTPTR, &p);
		strcat(fs->root, p.ret.ptr);
		fs->selected_dir = NULL;
	}
	
	if ((drv = get_drv(fs->root)) >= 0)
		strcpy(fs_paths[drv], fs->root);

	set_file(fs, fs->ofile);
	
	fs->selected_file = NULL;
	refresh_filelist(fsel, fs, fs->treeview ? dir_ent : NULL);
	if (fs->treeview && dir_ent)
	{
		short xs;
		list->get(list, dir_ent, SEGET_XSTATE, &xs);
		if (xs & OS_OPENED)
			fs_prompt(list, fs->file);
	}
	else
		fs_prompt(list, fs->file);
}

static int
fs_item_action(struct scroll_info *list, struct scroll_entry *this, const struct moose_data *md, short flags)
{
	bool fs_done = this ? false : true;
	struct fsel_data *fs = list->data;
	struct sesetget_params p; //seget_entrybyarg p;
	long uf;
	short state, xstate;

	DIAG((D_fsel, NULL, "fs_item_action %lx, fs=%lx", list->cur, fs));
	
	if (this)
	{
		list->get(list, this, SEGET_USRFLAGS, &uf);
		list->get(list, this, SEGET_STATE, &state);
		list->get(list, this, SEGET_XSTATE, &xstate);

		if (!(uf & FLAG_DIR))
		{
			DIAG((D_fsel, NULL, " --- nodir '%s'", this->content->c.text.text));
				
			/*
			 * If not already selected, or if double-clicked, make selection
			*/
			if (!(state & OS_SELECTED) || !md || md->clicks > 1)
			{
				if (md)
				{
					p.idx = -1;
					p.e = this;
					p.arg.txt = fs->file;
					list->get(list, this, SEGET_TEXTCPY, &p);
					fs->selected_file = this;
					fs->selected_dir = this->up;
					list->set(list, this, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_SELECTED, NORMREDRAW);
					set_dir(list);
					set_file(fs, NULL);
					if (md->clicks > 1)
						fs_done = true;
					else
						fs->tfile = true;
					fs->kbdnav = false;
				}
				else if (!flags)
					fs_done = true;
			}
			/*
			 * else unselect
			 */
			else if (state & OS_SELECTED)
			{
				if (md)
					fs->kbdnav = false;

				if (md && md->clicks < 2)
				{
					//list->set(list, this, SESET_UNSELECTED, UNSELECT_ONE, NORMREDRAW);
					list->set(list, this, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16), NORMREDRAW);
					fs->selected_file = NULL;
					fs->tfile = false;
					list->set(list, NULL, SESET_CURRENT, 0, NOREDRAW); //list->cur = NULL;
// 					fs->kbdnav = false;
					set_file(fs, fs->ofile); //set_file(fs, "");
				}				
				else if (!flags && (!md || md->clicks > 1))
				{
					fs_done = true;
				}
			}
		}
		else
		{
			/* folder entry action */
			DIAG((D_fsel, NULL, " --- folder '%s'", this->content->c.text.text));
			p.idx = -1;
			p.arg.txt = "..";
			
			if (list->get(list, this, SEGET_TEXTCMP, &p) && !p.ret.ret)
			{
			//	if (!md)
			//		fs->tfile = false;
			#if 0
				if ( /*fs->clear_on_folder_change &&*/ !fs->tfile)
					set_file(fs, "");

				/* cur on double dot folder line */
			#endif
				if (md)
					fs->kbdnav = false;

				set_file(fs, fs->ofile);
				fs_updir(list);
				if (!md && !fs->kbdnav)
				{
					fs->tfile = false;
// 					set_file(fs, "");
				}
			}
			else
			{
				p.arg.txt = ".";
				if (list->get(list, this, SEGET_TEXTCMP, &p) && p.ret.ret)
				{
					if (!md || (md->state & MBS_RIGHT) || md->clicks > 1)
					{
						fs->selected_dir = this;
						if (!md)
						{
							/*
							 * If return pressed and there is something in input-field
							 * we return, else we close the directory.
							 */
							if ((xstate & OS_OPENED))
							{
								if (flags)
								{
									if (!(flags & 2))
										list->set(list, this, SESET_OPEN, 0, NORMREDRAW);
								}
								else if (*fs->file)
									fs_done = true;
							}
						#if 0

							if ((xstate & OS_OPENED) && *fs->file)
							{
								fs_done = true;
							}
						#endif
							else
							{
								if (flags)
								{
									if (!(flags & 2))
									{
										if (!this || !this->up)
											fs_updir(list);
									}
									else
									{
										fs_enter_dir(fs, list, this);
										set_dir(list);
									}
								}
								else
								{
									if (!fs->kbdnav)
										fs->tfile = false;
									fs_enter_dir(fs, list, this);
									set_dir(list);
								}
							}
						}
						else
						{
							fs->kbdnav = false;
							
							if (xstate & OS_OPENED)
							{
								if ((md->state & MBS_LEFT) && fs->tfile)
									fs_done = true;
							}
							
							if (!fs_done)
							{
								fs_enter_dir(fs, list, this);
								set_dir(list);
							}
						}
					}
					else
					{
						if (md)
							fs->kbdnav = false;

						if ((state & OS_SELECTED))
						{
							fs->selected_dir = this->up;
							list->set(list, this, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16), NORMREDRAW);
						}
						else
						{
							list->set(list, this, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_SELECTED, NORMREDRAW);
							fs->selected_dir = this;
						}
						set_dir(list);
						
						set_file(fs, fs->ofile); //if (!fs->tfile) set_file(fs, "");
					}
				}
			}
		}
	}
		
	DIAG((D_fsel, NULL, "fs_item_action: %s%s", fs->root, fs->file));

	if (fs_done)
	{
	//	if (!fs->selected_file && !fs->tfile)
	//		fs->file[0] = '\0';
		
		fs_cwd(list, fs->path, 2);

		if (fs->selected)
		{
			fs->selected(list->lock, fs, fs->path, fs->file);
		}
// 		display("select path '%s', file '%s'", fs->path, fs->file);
	}

	return true;
}
static int
fs_click(struct scroll_info *list, struct scroll_entry *this, const struct moose_data *md)
{
	//struct fsel_data *fs = list->data;

	/* since mouse click we should _not_ clear the file field */
	/*fs->clear_on_folder_change = 0;*/
	//fs->tfile = true;
	if (this)
		fs_item_action(list, this, md, 0);
	return true;
}
static int
fs_click_nesticon(struct scroll_info *list, struct scroll_entry *this, const struct moose_data *md)
{
	struct fsel_data *fs = list->data;
	long uf;

	if (this)
	{
		list->get(list, this, SEGET_USRFLAGS, &uf);

		if (uf & FLAG_DIR)
		{
			fs_enter_dir(fs, list, this);
		}
	}
	return true;
}

/*
 * FormExit()
 */
static void
fileselector_form_exit(struct xa_client *client,
		       struct xa_window *wind,
		       XA_TREE *wt,
		       struct fmd_result *fr)
{
	enum locks lock = 0;
	OBJECT *obtree = wt->tree;
	struct scroll_info *list = object_get_slist(obtree + FS_LIST);
	struct fsel_data *fs = list->data;
#ifdef FS_FILTER
	TEDINFO *filter = object_get_tedinfo(obtree + FS_FILTER);
#endif
	
	wt->which = 0;
	wt->current = fr->obj;
	
	switch (fr->obj)
	{
	/*
	 * 
	 */
	case FS_LIST:
	{
		short obj = fr->obj;

		DIAGS(("filesel_form_exit: Moved the shit out of form_do() to here!"));
		if ( fr->md && ((obtree[obj].ob_type & 0xff) == G_SLIST))
		{
			if (fr->md->clicks > 1)
				dclick_scroll_list(lock, obtree, obj, fr->md);
			else
				click_scroll_list(lock, obtree, obj, fr->md);
		}
		break;
	}
#ifdef FS_UPDIR
	/* Go up a level in the filesystem */
	case FS_UPDIR:
	{
		fs_updir(list);
		break;
	}
#endif
	/* Accept current selection - do the same as a double click */
	case FS_OK:
	{
		object_deselect(wt->tree + FS_OK);
		redraw_toolbar(lock, wind, FS_OK);
#ifdef FS_FILTER
		if (strcmp(filter->te_ptext, fs->fs_pattern) != 0)
		{
			/* changed filter */
			strcpy(fs->fs_pattern, filter->te_ptext);
			refresh_filelist(fsel, fs, NULL);
			fs_prompt(list, fs->file);
		}
		else
#endif
		{
			struct scroll_entry *sel = get_selected(list);
			
			/* Just return with current selection */
			if (fs->kbdnav && sel) //list->cur)
				fs_item_action(list, sel/*list->cur*/, NULL, 0);
			else if (!fs->kbdnav)
				fs_item_action(list, fs->selected_dir, NULL, 0);
			else
				fs_item_action(list, fs->selected_dir, NULL, 0);
	
		}
		break;
	}
	/* Cancel this selector */
	case FS_CANCEL:
	{
		object_deselect(wt->tree + FS_CANCEL);
		redraw_toolbar(lock, wind, FS_CANCEL);
		fs->selected_file = NULL;
		if (fs->canceled)
	 		fs->canceled(lock, fs, fs->root, "");
		else
			close_fileselector(lock, fs);
		break;
	}
	}
}


static int
find_drive(int a, struct fsel_data *fs)
{
	XA_TREE *wt = get_widget(fs->wind, XAW_MENU)->stuff;
	OBJECT *m = wt->tree;
	int d = FSEL_DRVA;

	do {
		int x = tolower(*(m[d].ob_spec.free_string + 2));
		if (x == '~')
			break;
		if (x == a)
			return d;
	} while (m[d++].ob_next != FSEL_DRVA - 1);

	ping();

	return -1;
}

static void
fs_change(enum locks lock, struct fsel_data *fs, OBJECT *m, int p, int title, int d, char *t)
{
	XA_WIDGET *widg = get_widget(fs->wind, XAW_MENU);
	int bx = d - 1;
	
	do
		m[d].ob_state &= ~OS_CHECKED;
	while (m[d++].ob_next != bx);

	m[p].ob_state |= OS_CHECKED;
	sprintf(m[title].ob_spec.free_string, 128, " %s", m[p].ob_spec.free_string + 2);
	widg->start = 0;
	m[title].ob_state &= ~OS_SELECTED;
	display_widget(lock, fs->wind, widg, NULL);
	strcpy(t, m[p].ob_spec.free_string + 2);
}

static struct scroll_entry *
filename_completion(struct scroll_info *list)
{			
	struct sesetget_params p; //seget_entrybyarg p;
	struct scroll_entry *this;
	struct fsel_data *fs = list->data;
	long uf;
	
	p.idx = 0;
	p.arg.state.method = ANYBITS;
	p.arg.state.mask = p.arg.state.bits = OS_BOXED;
	p.level.flags = 0;
	p.level.maxlevel = -1;
	p.level.curlevel = 0;
	
	list->get(list, NULL, SEGET_ENTRYBYSTATE, &p);
	
	if ((this = p.e))
	{
		list->get(list, this, SEGET_USRFLAGS, &uf);
		list->set(list, this, SESET_STATE, ((long)(OS_BOXED|OS_SELECTED) << 16) | OS_SELECTED, NORMREDRAW);
		p.idx = -1;
		p.arg.txt = fs->file;
		list->get(list, this, SEGET_TEXTCPY, &p);
		if (!(uf & FLAG_DIR))
		{
			fs->selected_file = this;
			fs->selected_dir = this->up;
		}
		else
		{
			fs->selected_dir = this;
			fs->selected_file = NULL;
		}
		set_dir(list);
		set_file(fs, NULL);
		fs->tfile = false;
	}
	return this;
}

/*
 * FormKeyInput()
 */
static bool
fs_key_form_do(enum locks lock,
	       struct xa_client *client,
	       struct xa_window *wind,
	       XA_TREE *wt,
	       const struct rawkey *key)
{
	unsigned short keycode = key->aes, kk;
	unsigned short nkcode = key->norm, nk;
	struct scroll_info *list = object_get_slist(((XA_TREE *)get_widget(wind, XAW_TOOLBAR)->stuff)->tree + FS_LIST);
	struct fsel_data *fs = list->data;
	//SCROLL_ENTRY *old_entry = list->cur;

	if (nkcode == 0)
		nkcode = nkc_tconv(key->raw.bcon);
	nk = tolower(nkcode & 0xff);

	/* HR 310501: ctrl|alt + letter :: select drive */
	if ((key->raw.conin.state & K_ALT) && ((nk >= 'a' && nk <= 'z') || (nk >= '0' && nk <= '9')))
	{
		int drive_object_index = find_drive(nk, fs);
		if (drive_object_index >= FSEL_DRVA)
			wind->send_message(lock, wind, NULL, AMQ_NORM, QMF_CHKDUP,
					   MN_SELECTED, 0, 0, FSEL_DRV,
					   drive_object_index, 0, 0, 0);
	}

	/* ctrl + backspace :: fs_updir() */
	else if ((key->raw.conin.state & K_CTRL) && (nkcode & NKF_CTRL) && nk == NK_BS)
	{
		set_file(fs, fs->ofile);
		fs_updir(list);
	}
	else if ((key->raw.conin.state & K_CTRL) && (nkcode & NKF_CTRL) && nk == '*')
	{
		/* change the filter to '*'
		 * - this should always be the FSEL_PATA entry IIRC
		 */
		fs_change(lock, fs, fs->menu->tree,
				FSEL_PATA, FSEL_FILTER, FSEL_PATA, fs->fs_pattern);
		fs->selected_dir = fs->selected_file = NULL;
		/* apply the change to the filelist */
		refresh_filelist(fsel, fs, NULL);
		fs_prompt(list, fs->file);
	}
	else
	{
		SCROLL_ENTRY *was, *this; // = get_selected(list), *this; //list->cur;
		long uf;

		//list->set(list, was, SESET_CURRENT, 0, NOREDRAW);

		switch (keycode)
		{
			case 0x4800:	/* arrow up */
			case 0x5000:	/* arrow down */
			case 0x4b00:	/* arrow left */
			case 0x4d00:	/* arrow right */
			case 0x5200:	/* insert */
			{
				short xsb, xsa;
			
				fs->kbdnav = true;

				if (!(was = filename_completion(list)))
					was = get_selected(list);
				
				if (was)
					list->get(list, was, SEGET_XSTATE, &xsb);
	
				list->set(list, was, SESET_CURRENT, 0, NOREDRAW);

				if ((keycode == 0x4b00 || keycode == 0x4d00) && scrl_cursor(list, keycode) != -1)
				{

					list->get(list, NULL, SEGET_CURRENT, &this);
					
					if (keycode == 0x4d00)
					{
						/* If right arrow and selection dont move, the
						 * selection have children.
						 */
						if (was && was == this)
							fs_item_action(list, was, NULL, 3);
						else
							set_selected(list, this);
					}
					else
					{
						if (was && was == this)
						{
							/* If current didtn change, we check if current was opened
							 * before, in which case we dont do anything, because it was
							 * automatically closed.
							 * If current was closed already, we're at root, and this
							 * arrow left should go one dir up
							 */
							list->get(list, this, SEGET_XSTATE, &xsa);
							if (!(xsb & OS_OPENED) && !(xsa & OS_OPENED))
							{
								set_file(fs, fs->ofile);
								fs_updir(list);
								if (!get_selected(list))
									scrl_cursor(list, 0x5200);
							}
						}
					}
				}
				else
				{
					kk = scrl_cursor(list, ((!was && (keycode == 0x5000 || keycode == 0x4800)) ? 0x5200 : keycode));
			
					list->get(list, NULL, SEGET_CURRENT, &this);
			
					if (this)
					{
						fs->kbdnav = true;
						{
							struct sesetget_params p; //seget_entrybyarg p;
							p.idx = 0;
							p.arg.state.method = ANYBITS;
							p.arg.state.mask = p.arg.state.bits = OS_BOXED;
							p.level.flags = 0;
							p.level.maxlevel = -1;
							p.level.curlevel = 0;
							list->get(list, NULL, SEGET_ENTRYBYSTATE, &p);
							if (p.e)
							{
								list->set(list, p.e, SESET_STATE, ((long)OS_BOXED << 16), NORMREDRAW);
							}
						}
				
						list->get(list, this, SEGET_USRFLAGS, &uf);
				
						if (!(uf & FLAG_DIR))
						{
							struct sesetget_params p; //seget_entrybyarg p;
				
							if (this->up != fs->selected_dir)
							{
								fs->selected_dir = this->up;
								fs->selected_file = this;
								set_dir(list);
							}
							p.idx = -1;
							list->get(list, this, SEGET_TEXTPTR, &p);
							set_file(fs, p.ret.ptr);
							fs->tfile = false;
						}
						else
						{
							if (this != fs->selected_dir)
							{
								fs->selected_dir = this;
								fs->selected_file = NULL;
								set_dir(list);
							}
							set_file(fs, fs->ofile);
						}
						if (kk == 0x5200)
							fs->kbdnav = true;
					}
					else if (kk == 0x5200)
					{
						if (was)
						{
							if (was == fs->selected_dir)
							{
								fs->selected_dir = was->up;
								set_dir(list);
							}
							else if (was == fs->selected_file)
							{
								fs->selected_file = NULL;
							}
							set_file(fs, fs->ofile); //if (!fs->tfile) set_file(fs, "");
						}
						fs->kbdnav = false;
					}
				}			
				break;
			}
			case 0x0f09:
			{
				fs->kbdnav = true;
				filename_completion(list);
				break;
			}
			default:
			{
				
				if (scrl_cursor(list, keycode) == -1)
				{
					char old[NAME_MAX+2];
					strcpy(old, fs->file);
					/* HR 290501: if !discontinue */
					if (Key_form_do(lock, client, wind, wt, key))
					{
						if (fs->wind)
						{
							/* something typed in there? */
							if (strcmp(old, fs->file) != 0)
							{
								strcpy(fs->ofile, fs->file);
								fs->tfile = true;
								fs->kbdnav = true;
								fs_prompt(list, fs->file);
							}
						}
					}
				}
				break;
			}
		}
	}
	return true;
}

/* HR: make a start */
/* dest_pid, msg, source_pid, mp3, mp4,  ....    */
static void
fs_msg_handler(
	struct xa_window *wind,
	struct xa_client *to,
	short amq, short qmf,
	short *msg)
{
	enum locks lock = 0;
	struct scroll_info *list = object_get_slist(((XA_TREE *)get_widget(wind, XAW_TOOLBAR)->stuff)->tree + FS_LIST);
	struct fsel_data *fs = list->data;

	switch (msg[0])
	{
	case MN_SELECTED:
	{
		if (msg[3] == FSEL_OPTS)
		{
			OBJECT *obtree = fs->menu->tree;
			short obj = msg[4], sort;
			if (obj == FSM_RTBUILD)
			{
				if (obtree[FSM_RTBUILD].ob_state & OS_CHECKED)
					fs->rtbuild = false;
				else
					fs->rtbuild = true;
				obtree[FSM_RTBUILD].ob_state ^= OS_CHECKED;
			}
			else if (obj == FSM_TREEVIEW)
			{
				if (obtree[FSM_TREEVIEW].ob_state & OS_CHECKED)
				{
					list->set(list, NULL, SESET_TREEVIEW, 0, NOREDRAW);
					fs->treeview = false;
				}
				else
				{
					list->set(list, NULL, SESET_TREEVIEW, 1, NOREDRAW);
					fs->treeview = true;
				}
				obtree[FSM_TREEVIEW].ob_state ^= OS_CHECKED;
			}
			else
			{
				sort = obj - FSM_SORTBYNAME;
				if (sort >= 0 && sort < 5)
				{
					obtree[FSM_SORTBYNAME + fs->sort].ob_state &= ~OS_CHECKED;
					obtree[FSM_SORTBYNAME + sort    ].ob_state |= OS_CHECKED;
					fs->sort = sort;
				}
			}
			
			obtree[FSEL_OPTS].ob_state &= ~OS_SELECTED;
			display_widget(lock, fs->wind, get_widget(fs->wind, XAW_MENU), NULL);
			fs->selected_dir = NULL;
			fs->selected_file = NULL;
			set_file(fs, fs->ofile);
// 			if (!fs->tfile)
// 				set_file(fs, "");
		}
		else if (msg[3] == FSEL_FILTER)
		{
			fs_change(lock, fs, fs->menu->tree, msg[4], FSEL_FILTER, FSEL_PATA, fs->fs_pattern);
			fs->selected_dir = fs->selected_file = NULL;
			set_file(fs, fs->ofile);
// 			if (!fs->tfile)
// 				set_file(fs, "");
		}
		else if (msg[3] == FSEL_DRV)
		{
			int drv;
			fs_change(lock, fs, fs->menu->tree, msg[4], FSEL_DRV, FSEL_DRVA, fs->root);
			inq_xfs(fs, fs->root, fs->fslash);
			add_slash(fs->root, fs->fslash);
			drv = get_drv(fs->root);
			if (fs_paths[drv][0])
				strcpy(fs->root, fs_paths[drv]);
			else
				strcpy(fs_paths[drv], fs->root);
			/* remove the name from the edit field on drive change */
// 			if ( !fs->tfile ) //fs->clear_on_folder_change )
// 				set_file(fs, "");
			set_file(fs, fs->ofile);
			fs->selected_dir = fs->selected_file = NULL;
		}
// 		display("cur0 %lx", list->cur);
		set_dir(list);
// 		display("cur1 %lx", list->cur);
		refresh_filelist(lock, fs, NULL);
// 		display("cur2 %lx", list->cur);
		fs_prompt(list, fs->file);
// 		display("cur3 %lx", list->cur);
		break;
	}
	case WM_MOVED:
	{
		msg[6] = fs->wind->r.w, msg[7] = fs->wind->r.h;
		do_formwind_msg(wind, to, amq, qmf, msg);
		break;
		/* fall through */
	}
// 	case WM_REPOSED:
	case WM_SIZED:
	{
		short dh, dw;
		OBJECT *obtree = ((struct widget_tree *)get_widget(wind, XAW_TOOLBAR)->stuff)->tree;
		struct xa_window *lwind;

		dw = msg[6] - wind->r.w; //root_window->wa.h - 7 * screen.c_max_h - form->ob_height;
		dh = msg[7] - wind->r.h; //root_window->wa.w - (form->ob_width + (screen.c_max_w * 4));
// 		display("resize dw %d, dh %d", dw, dh);
		obtree->ob_height += dh;
		obtree->ob_width += dw;
		obtree[FS_LIST ].ob_height += dh;
		obtree[FS_LIST ].ob_width += dw;
		obtree[FS_UNDER].ob_y += dh;
		obtree[FS_UNDER].ob_x += dw;
		
		if ((lwind = list->wi) && lwind->send_message)
		{
			lwind->send_message(0, lwind, NULL, amq, QMF_CHKDUP,
				WM_SIZED, 0,0, lwind->handle,
				lwind->r.x, lwind->r.y, lwind->r.w + dw, lwind->r.h + dh);
		}
// 		display("do fsel wind resize");
		move_window(0, wind, false, -1, msg[4], msg[5], msg[6], msg[7]);
		set_toolbar_coords(wind, NULL);
		break;
	}
	default:
	{
		do_formwind_msg(wind, to, amq, qmf, msg);
	}
	}
}

static int
fs_destructor(enum locks lock, struct xa_window *wind)
{
	DIAG((D_fsel,NULL,"fsel destructed"));
	return true;
}

static bool treeview = false;
static bool rtbuild = false;

static void
fs_init_menu(struct fsel_data *fs)
{
	OBJECT *obtree = fs->menu->tree;

	if (treeview)
		obtree[FSM_TREEVIEW].ob_state |= OS_CHECKED;
	else
		obtree[FSM_TREEVIEW].ob_state &= ~OS_CHECKED;
	fs->treeview = treeview;

	if (rtbuild)
		obtree[FSM_RTBUILD].ob_state |= OS_CHECKED;
	else
		obtree[FSM_RTBUILD].ob_state &= ~OS_CHECKED;

	obtree[FSM_SORTBYNAME + fs->sort].ob_state |= OS_CHECKED;

	fs->rtbuild = rtbuild;
}


static bool
open_fileselector1(enum locks lock, struct xa_client *client, struct fsel_data *fs,
		   const char *path, const char *file, const char *title,
		   fsel_handler *s, fsel_handler *c)
{
	bool nolist;
	XA_WIND_ATTR kind;
	OBJECT *form = NULL, *menu = NULL;
	struct xa_window *dialog_window = NULL;
	XA_TREE *wt;
	char *pat,*pbt;
	struct scroll_info *list;
	RECT remember = {0,0,0,0}, or;

	DIAG((D_fsel,NULL,"open_fileselector for %s on '%s', fn '%s', '%s', %lx,%lx)",
			c_owner(client), path, file, title, s, c));

	if (fs)
	{
		bzero(fs, sizeof(*fs));

		fs->owner = client;
		
		*fs->fslash = '\\';
		
		form = duplicate_obtree(C.Aes, ResourceTree(C.Aes_rsc, FILE_SELECT), 0);
		if (!form)
			goto memerr;
		fs->form = new_widget_tree(client, form);
		if (!fs->form)
			goto memerr;
		fs->form->flags |= WTF_TREE_ALLOC | WTF_AUTOFREE;

		menu = duplicate_obtree(C.Aes, ResourceTree(C.Aes_rsc, FSEL_MENU), 0);
		if (!menu)
			goto memerr;
		fs->menu = new_widget_tree(client, menu);
		if (!fs->menu)
			goto memerr;
		fs->menu->flags |= WTF_TREE_ALLOC | WTF_AUTOFREE;
				
		fs->selected = s;
		fs->canceled = c;
		if (file && *file)
		{
			strncpy(fs->file, file, sizeof(fs->file) - 1);
			*(fs->file + sizeof(fs->file) - 1) = 0;
		}
		else
		{
			fs->file[0] = '\0';
		}
		
		strcpy(fs->ofile, fs->file);
		
		fs->tfile = fs->file[0] != '\0' ? true : false;

		object_get_spec(form + FS_FILE)->tedinfo->te_ptext = fs->file;
		form[FS_ICONS].ob_flags |= OF_HIDETREE;

		if (path && *path != '\0' && ((*path == '\\' || *path == '/') || (path[1] == ':' && (path[2] == '\\' || path[2] == '/'))))
		{
// 			display("legal paht '%s'", path);
			strcpy(fs->root, path);
		}
		else
		{
			int cwdl, drv = d_getdrv();
			char chr;

			fs->root[0] = drv + 'a';
			fs->root[1] = ':';
			d_getcwd(fs->root + 2, drv + 1, sizeof(fs->root));
			
			if ((chr = fs->root[2]))
			{
				cwdl = strlen(fs->root);
				if (fs->root[cwdl - 1] != chr)
				{
					fs->root[cwdl] = chr;
					fs->root[cwdl+1] = '\0';
				}
			}
			else
				fs->root[2] = '\\', fs->root[3] = '\0';
	
// 			display("illegal path '%s'", path ? path : "nopath");
// 			display("set path to  '%s'", fs->root);
		#if 0
			fs->root[2] = '\\';
			fs->root[3] = '*';
			fs->root[4] = 0;
		#endif
		}

		/* Strip out the pattern description */
		fs->fs_pattern[0] = '*';
		fs->fs_pattern[1] = '\0';
		pat = strrchr(fs->root, '\\');
		pbt = strrchr(fs->root, '/');
		if (!pat) pat = pbt;
		if (pat)
		{
			if (*(pat + 1))
			{
				strcpy(fs->fs_pattern, pat + 1);
				strcpy(fs->fs_origpattern, fs->fs_pattern);
				*(pat + 1) = 0;
				if (strcmp(fs->fs_pattern, "*.*") == 0)
					*(fs->fs_pattern + 1) = 0;
			}
		}

		{
			int drv = get_drv(fs->root);
			if (drv >= 0)
				strcpy(fs_paths[drv], fs->root);
		}
		
		strcpy(fs->path, fs->root);

		{
			short dh, dw;

			
			dh = root_window->wa.h - 7 * screen.c_max_h - form->ob_height;
			dw = root_window->wa.w - (form->ob_width + (screen.c_max_w * 4));
			
			if ((dw + form->ob_width) > 560)
				dw = 560 - form->ob_width;

			form->ob_height += dh;
			form->ob_width += dw;
			form[FS_LIST ].ob_height += dh;
			form[FS_LIST ].ob_width += dw;
			form[FS_UNDER].ob_y += dh;
			form[FS_UNDER].ob_x += dw;
		}

		ob_rectangle(form, 0, &or);

		kind = (XaMENU|NAME|TOOLBAR|BORDER);
		/* Work out sizing */
		if (!remember.w)
		{
			center_rect(&or);
			remember =
			calc_window(lock, client, WC_BORDER,
				    kind, created_for_AES,
				    4, /*C.Aes->options.thinframe,*/
				    C.Aes->options.thinwork,
				    *(RECT*)&or); //form->ob_x);
		}
		
		if (C.update_lock == client->p ||
		    C.mouse_lock  == client->p)
		{
			nolist = true;
			kind |= STORE_BACK;
		}
		else
		{
			kind |= hide_move(&(client->options));
			nolist = false;
		}

		/* Create the window */
		dialog_window = create_window(  lock,
						do_winmesag,
						fs_msg_handler,
						client,
						nolist,
						kind,
						created_for_AES,
						4, /*C.Aes->options.thinframe,*/
						C.Aes->options.thinwork,
						remember,
						NULL,
						NULL); //&remember);
		
		if (!dialog_window)
			goto memerr;

		fix_menu(client, fs->menu, dialog_window, false);
		fs_init_menu(fs);

		/* Set the window title */
		/* XXX - pointer into user space, correct here? */
		/*   ozk: no, absolutely not right here! */
		set_window_title(dialog_window, title, true);

		set_menu_widget(dialog_window, client, fs->menu);

		fs->drives = fsel_drives(fs->menu->tree,
					*(fs->root+1) == ':' ? tolower(*fs->root) - 'a' : d_getdrv());
		
		fsel_filters(fs->menu->tree, fs->fs_pattern);

		fs->clear_on_folder_change = 0;

		wt = set_toolbar_widget(lock, dialog_window, client, form, FS_FILE, 0/*WIP_NOTEXT*/, true, NULL, &or);
		/* This can be of use for drawing. (keep off border & outline :-) */
		wt->zen = true;
		wt->exit_form = fileselector_form_exit;

		/* HR: We need to do some specific things with the key's,
		 *     so we supply our own handler,
		 */
		dialog_window->keypress = fs_key_form_do; //fs_key_handler;

		/* HR: set a scroll list object */
		list = set_slist_object(lock,
				 wt,
				 dialog_window,
				 FS_LIST,
				 SIF_SELECTABLE | SIF_ICONINDENT | (fs->treeview ? SIF_TREEVIEW : 0) | SIF_AUTOSLIDERS,
				 fs_closer, NULL,
				 fs_click/*was dclick*/, fs_click, fs_click_nesticon,
				 NULL, NULL, NULL, NULL/*free_scrollist*/,
				 fs->path, NULL, fs, 30);
		{
			struct seset_tab tab;
// 			RECT r;
// 			short w;

			tab.index = FSLIDX_NAME;
			tab.flags = 0;
			tab.r = (RECT){0,0,-50,0};
			list->set(list, NULL, SESET_TAB, (long)&tab, false);
			
			tab.index = FSLIDX_SIZE;
			tab.flags |= SETAB_RJUST;
			tab.r.w = 0; //-10; //(RECT){0,0,-10,0};
			list->set(list, NULL, SESET_TAB, (long)&tab, false);
			
			tab.index = FSLIDX_TIME;
			tab.flags = 0;
			tab.r.w = 0; //-10;
			list->set(list, NULL, SESET_TAB, (long)&tab, false);
			
			tab.index = FSLIDX_DATE;
			tab.flags = 0;
			tab.r.w = 0; //-20;
			list->set(list, NULL, SESET_TAB, (long)&tab, false);
			
			tab.index = FSLIDX_FLAG;
			tab.flags = 0;
			tab.r.w = 0; //-10;
			list->set(list, NULL, SESET_TAB, (long)&tab, false);

// 			tab.index = 3;
// 			tab.flags = 0;
// 			tab.r = (RECT){0,0,-2,0};
// 			list->set(list, NULL, SESET_TAB, (long)&tab, false);

			
#if 0		
			list->get(list, NULL, SEGET_LISTXYWH, &r);

			w = r.w >> 2;
			
			tab.index = 1;
			list->get(list, NULL, SEGET_TEXTTAB, &tab);
			tab.r.w = w * 3;
			list->set(list, NULL, SESET_TEXTTAB, (long)&tab, false);
			
			tab.index = 2;
			list->get(list, NULL, SEGET_TEXTTAB, &tab);
			tab.r.w = w;
			tab.flags |= SETAB_RJUST;
			list->set(list, NULL, SESET_TEXTTAB, (long)&tab, false);
#endif
		}
		/* HR: after set_menu_widget (fs_destructor must cover what is in menu_destructor())
		 *     Set the window destructor
		 */
		dialog_window->destructor = fs_destructor;

		strcpy(fs->filter, fs->fs_pattern);
		fs->wind = dialog_window;
		open_window(lock, dialog_window, dialog_window->r);

		/* HR: after set_slist_object() & opwn_window */
		//refresh_filelist(lock, fs, 5);
		/* we post this as a client event, so it does not happend before the fsel is drawn... */
		post_cevent(client, CE_refresh_filelist, fs, list, 0, 0, NULL, NULL);

		DIAG((D_fsel,NULL,"done."));

		return true;
	}
	else
	{
memerr:
		if (fs)
		{
			if (fs->form)
			{
				remove_wt(fs->form, true);
				fs->form = NULL;
			}
			else if (form)
				free_object_tree(client, form);
			if (fs->menu)
			{
				remove_wt(fs->menu, true);
				fs->menu = NULL;
			}
			else if (menu)
				free_object_tree(client, form);
		}
		return false;
	}
}

void
close_fileselector(enum locks lock, struct fsel_data *fs)
{
	close_window(lock, fs->wind);
	delete_window(lock, fs->wind);
	rtbuild = fs->rtbuild;
	treeview = fs->treeview;
	fs->wind = NULL;
	fs->menu = NULL;
	fs->form = NULL;
	fs->selected = NULL;
	fs->canceled = NULL;
}

static void
handle_fsel(enum locks lock, struct fsel_data *fs, const char *path, const char *file)
{
	DIAG((D_fsel, NULL, "fsel OK: path=%s, file=%s", path, file));

	close_fileselector(lock, fs);
	fs->ok = 1;
	fs->done = 1;
	fs->owner->usr_evnt = 1;
}

static void
cancel_fsel(enum locks lock, struct fsel_data *fs, const char *path, const char *file)
{
	DIAG((D_fsel, NULL, "fsel CANCEL: path=%s, file=%s", path, file));

	close_fileselector(lock, fs);
	fs->ok = 0;
	fs->done = 1;
	fs->owner->usr_evnt = 1;
}

static int locked = 0;
void
open_fileselector(enum locks lock, struct xa_client *client, struct fsel_data *fs,
		  const char *path, const char *file, const char *title,
		  fsel_handler *s, fsel_handler *c)
{
	if (!locked)
	{
		open_fileselector1(lock, client, fs, path, file, title, s, c);
	}
}

/*
 * File selector interface routines
 */
static void
do_fsel_exinput(enum locks lock, struct xa_client *client, AESPB *pb, const char *text)
{
	char *path = (char *)(pb->addrin[0]);
	char *file = (char *)(pb->addrin[1]);
	struct fsel_data *fs;

	pb->intout[0] = 0;

// 	display("do_fsel_exinput for %s", client->proc_name);
// 	display("path %lx '%s'", path, path ? path : "nopath");
// 	display("file %lx '%s'", file, file ? file : "nofile");
	
	fs = kmalloc(sizeof(*fs));
	
	if (fs)
	{

		DIAG((D_fsel, NULL, "fsel_(ex)input: title=%s, path=%s, file=%s, fs=%lx",
			text, path, file, fs));
	//	display("fsel_(ex)input: title=(%lx)%s, path=(%lx)%s, file=(%lx)%s, fs=%lx",
	//		text, text, path, path, file, file, fs);
			
		if (open_fileselector1( lock|fsel,
					client,
					fs,
					path,
					file,
					text,
					handle_fsel,
					cancel_fsel))
		{
			client->status |= CS_FSEL_INPUT;
			Block(client, 21);
			client->status &= ~CS_FSEL_INPUT;
			pb->intout[0] = 1;
			if ((pb->intout[1] = fs->ok))
			{
				strcpy(path, fs->path);
				strcat(path, fs->fs_origpattern);
				strcpy(file, fs->file);
			//	display("return file '%s', path '%s'", file, path);
			}
		}
		kfree(fs);
	}
}

unsigned long
XA_fsel_input(enum locks lock, struct xa_client *client, AESPB *pb)
{
	CONTROL(0,2,2)

	graf_mouse(ARROW, NULL, NULL, false);
	showm();
	do_fsel_exinput(lock, client, pb, "");
	graf_mouse(client->mouse, client->mouse_form, client, false);

	return XAC_DONE;
}

unsigned long
XA_fsel_exinput(enum locks lock, struct xa_client *client, AESPB *pb)
{
	const char *t = (const char *)(pb->addrin[2]);

	CONTROL(0,2,3)

	if (pb->control[N_ADDRIN] <= 2 || t == NULL)
		t = "";

	graf_mouse(ARROW, NULL, NULL, false);
	showm();
	do_fsel_exinput(lock, client, pb, t);
	graf_mouse(client->mouse, client->mouse_form, client, false);

	return XAC_DONE;
}
