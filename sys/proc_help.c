/*
 * $Id$
 * 
 * This file belongs to FreeMiNT. It's not in the original MiNT 1.12
 * distribution. See the file CHANGES for a detailed log of changes.
 * 
 * 
 * Copyright 2000 Frank Naumann <fnaumann@freemint.de>
 * All rights reserved.
 * 
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * 
 * Author: Frank Naumann <fnaumann@freemint.de>
 * Started: 2000-10-31
 * 
 * Please send suggestions, patches or bug reports to me or
 * the MiNT mailing list.
 * 
 */

# include "proc_help.h"

# include "arch/mprot.h"
# include "libkern/libkern.h"
# include "mint/file.h"

# include "filesys.h"
# include "k_fds.h"
# include "kmemory.h"
# include "memory.h"


/* p_mem */

void
init_page_table_ptr (struct memspace *m)
{
	if (no_mem_prot)
	{
		m->page_table = NULL;
		m->pt_mem = NULL;
	}
	else
	{
# ifdef MMU040
		extern int page_ram_type;	/* mprot040.c */
		MEMREGION *pt;
		
		pt = NULL;
		if (page_ram_type & 2)
			pt = get_region (alt, page_table_size + 512, PROT_S);
		if (!pt && (page_ram_type & 1))
			pt = get_region (core, page_table_size + 512, PROT_S);
		
		/* For the 040, the page tables must be on 512 byte boundaries */
		m->page_table = pt ? ROUND512 (pt->loc) : NULL;
		m->pt_mem = pt;
# else
		void *pt;
		
		pt = kmalloc (page_table_size + 16);
		
		/* page tables must be on 16 byte boundaries, so we
		 * round off by 16 for that; however, we will want to
		 * kfree that memory at some point, so we squirrel
		 * away the original address for later use
		 */
		m->page_table = pt ? ROUND16 (pt) : NULL;
		m->pt_mem = pt;
# endif
		
		if (!pt) DEBUG(("init_page_table_ptr: no mem for page table"));
	}
}

static void
free_page_table_ptr (struct memspace *m)
{
	if (!no_mem_prot)
	{
# ifdef MMU040
		MEMREGION *pt = m->pt_mem;
		
		pt->links--;
		if (!pt->links)
			free_region (pt);
# else
		kfree (m->pt_mem);
# endif
	}
}

struct memspace *
copy_mem (struct proc *p)
{
	struct memspace *m;
	int i;
	
	TRACE (("copy_mem: pid %i (%lx)", p->pid, p));
	assert (p && p->p_mem && p->p_mem->links > 0);
	
	m = kmalloc (sizeof (*m));
	if (!m)
	{
		DEBUG(("copy_mem: kmalloc failed -> NULL"));
		return NULL;
	}
	
	bcopy (p->p_mem, m, sizeof (*m));
	m->links = 1;
	
	init_page_table_ptr (m);
	
	m->mem = kmalloc (m->num_reg * sizeof (MEMREGION *));
	m->addr = kmalloc (m->num_reg * sizeof (long));
	
	if ((!no_mem_prot && !m->pt_mem) || !m->mem || !m->addr)
		goto nomem;
	
	/* copy memory */
	for (i = 0; i < m->num_reg; i++)
	{
		m->mem[i] = p->p_mem->mem[i];
		if (m->mem[i])
			m->mem[i]->links++;
		
		m->addr[i] = p->p_mem->addr[i];
	}
	
	TRACE (("copy_mem: ok (%lx)", m));
	return m;
	
nomem:
	if (m->pt_mem) free_page_table_ptr (m);
	if (m->mem) kfree (m->mem);
	if (m->addr) kfree (m->addr);
	kfree (m);
	
	return NULL;
}

void
free_mem (struct proc *p)
{
	struct memspace *p_mem;
	MEMREGION **hold_mem;
	long *hold_addr;
	int i;
	
	assert (p && p->p_mem && p->p_mem->links > 0);
	p_mem = p->p_mem;
	// XXX p->p_mem = NULL; --- dependency in arch/mprot0?0.c
	
	if (--p_mem->links > 0)
		return;
	
	DEBUG (("freeing p_mem for %s", p->name));
	
	/* free all memory
	 * if mflags & M_KEEP then attach it to process 0
	 */
	for (i = p_mem->num_reg - 1; i >= 0; i--)
	{
		MEMREGION *m = p_mem->mem[i];
		
		p_mem->mem[i] = 0;
		p_mem->addr[i] = 0;
		
		if (m)
		{
			/* don't free specially allocated memory */
			if ((m->mflags & M_KEEP) && (m->links <= 1))
				if (p != rootproc)
					attach_region (rootproc, m);
			
			m->links--;
			if (m->links == 0)
				free_region (m);
		}
	}
	
	/*
	 * mark the mem & addr arrays as void so the memory
	 * protection code won't try to walk them. Do this before
	 * freeing them so we don't try to walk them when marking
	 * those pages themselves as free!
	 *
	 * Note: when a process terminates, the MMU root pointer
	 * still points to that process' page table, until the next
	 * process is dispatched.  This is OK, since the process'
	 * page table is in system memory, and it isn't going to be
	 * freed.  It is going to wind up on the free process list,
	 * though, after dispose_proc. This might be Not A Good
	 * Thing.
	 */
	
	hold_addr = p_mem->addr;
	hold_mem = p_mem->mem;
	
	p_mem->mem = NULL;
	p_mem->addr = NULL;
	p_mem->num_reg = 0;
	
	kfree (hold_addr);
	kfree (hold_mem);
	
	free_page_table_ptr (p_mem);
	kfree (p_mem);
	
	p->p_mem = NULL;
}

/* p_fd */

struct filedesc *
copy_fd (struct proc *p)
{
	struct filedesc *org_fd;
	struct filedesc *fd;
	long i;
	
	TRACE (("copy_fd: pid %i (%lx)", p->pid, p));
	assert (p && p->p_fd && p->p_fd->links > 0);
	org_fd = p->p_fd;
	
	fd = kmalloc (sizeof (*fd));
	if (!fd)
	{
		DEBUG(("copy_fd: kmalloc failed -> NULL"));
		return NULL;
	}
	
	bcopy (org_fd, fd, sizeof (*fd));
	fd->links = 1;
	
# if 0
	if (!(fd->lastfile < NDFILE))
	{
		i = fd->nfiles;
		while (i >= 2 * NDEXTENT && i > fd->lastfile * 2)
			i /= 2;
		
		fd->ofiles = kmalloc (i * OFILESIZE);
		fd->ofileflags = (char *) &fd->ofiles[i];
	}
	else
# endif
	{
		fd->ofiles = &fd->dfiles[0];
		fd->ofileflags = &fd->dfileflags[0];
		i = NDFILE;
	}
	
	fd->nfiles = i;
	
	//bcopy (org_fd->ofiles, fd->ofiles, i * sizeof (FILEPTR **));
	//bcopy (org_fd->ofileflags, fd->ofileflags, i * sizeof (char));
	
	for (i = MIN_HANDLE; i < fd->nfiles; i++)
	{
		FILEPTR *f = fd->ofiles[i];
		
		if (f)
		{
			if ((f == (FILEPTR *) 1L) || (f->flags & O_NOINHERIT))
				/* oops, we didn't really want to copy this
				 * handle
				 */
				fd->ofiles[i] = NULL;
			else
				f->links++;
		}
	}
	
	/* clear directory search info */
	bzero (fd->srchdta, NUM_SEARCH * sizeof (DTABUF *));
	bzero (fd->srchdir, sizeof (fd->srchdir));
	fd->searches = NULL;
	
	TRACE (("copy_fd: ok (%lx)", fd));
	return fd;
}

void
free_fd (struct proc *p)
{
	struct filedesc *p_fd;
	FILEPTR *f;
	int i;
	
	assert (p && p->p_fd && p->p_fd->links > 0);
	p_fd = p->p_fd;
	p->p_fd = NULL;
	
	if (--p_fd->links > 0)
		return;
	
	DEBUG (("freeing p_fd for %s", p->name));
	
	/* release the controlling terminal,
	 * if we're the last member of this pgroup
	 */
	f = p_fd->control;
	if (f && is_terminal (f))
	{
		struct tty *tty = (struct tty *) f->devinfo;
		int pgrp = p->pgrp;
		
		if (pgrp == tty->pgrp)
		{
			FILEPTR *pfp;
			PROC *p1;
			
			if (tty->use_cnt > 1)
			{
				for (p1 = proclist; p1; p1 = p1->gl_next)
				{
					if (p1->wait_q == ZOMBIE_Q || p1->wait_q == TSR_Q)
						continue;
					
					if (p1->pgrp == pgrp
						&& p1 != p
						&& ((pfp = p1->p_fd->control) != NULL)
						&& pfp->fc.index == f->fc.index
						&& pfp->fc.dev == f->fc.dev)
					{
						goto found;
					}
				}
			}
			else
			{
				for (p1 = proclist; p1; p1 = p1->gl_next)
				{
					if (p1->wait_q == ZOMBIE_Q || p1->wait_q == TSR_Q)
						continue;
					
					if (p1->pgrp == pgrp
						&& p1 != p
						&& p1->p_fd->control == f)
					{
						goto found;
					}
				}
			}
			tty->pgrp = 0;
		}
found:
		;
	}
	
	/* close all files */
	for (i = MIN_HANDLE; i < p_fd->nfiles; i++)
	{
		f = p_fd->ofiles[i];
		
		if (f)
		{
			p_fd->ofiles[i] = NULL;
			do_close (p, f);
		}
	}
	
	/* close any unresolved Fsfirst/Fsnext directory searches */
	for (i = 0; i < NUM_SEARCH; i++)
	{
		if (p_fd->srchdta[i])
		{
			DIR *dirh = &p_fd->srchdir[i];
			xfs_closedir (dirh->fc.fs, dirh);
			release_cookie (&dirh->fc);
			dirh->fc.fs = 0;
		}
	}
	
	/* close pending opendir/readdir searches */
	{
		register DIR *dirh = p_fd->searches;
		
		while (dirh)
		{
			register DIR *nexth = dirh->next;
			
			if (dirh->fc.fs)
			{
				xfs_closedir (dirh->fc.fs, dirh);
				release_cookie (&dirh->fc);
			}
			
			kfree (dirh);
			dirh = nexth;
		}
	}
	
	if (p_fd->nfiles > NDFILE)
		kfree (p_fd->ofiles);
	
	kfree (p_fd);
}

/* p_cwd */

struct cwd *
copy_cwd (struct proc *p)
{
	struct cwd *org_cwd;
	struct cwd *cwd;
	int i;
	
	TRACE (("copy_cwd: pid %i (%lx)", p->pid, p));
	assert (p && p->p_cwd && p->p_cwd->links > 0);
	org_cwd = p->p_cwd;
	
	cwd = kmalloc (sizeof (*cwd));
	if (!cwd)
	{
		DEBUG(("copy_cwd: kmalloc failed -> NULL"));
		return NULL;
	}
	
	bzero (cwd, sizeof (*cwd));
	
	cwd->links = 1;
	cwd->cmask =  org_cwd->cmask;
	
	if (cwd->root_dir)
	{
		cwd->root_dir = kmalloc (strlen (cwd->root_dir) + 1);
		if (!cwd->root_dir)
		{
			kfree (cwd);
			return NULL;
		}
		
		strcpy (cwd->root_dir, org_cwd->root_dir);
		dup_cookie (&cwd->rootdir, &org_cwd->rootdir);
	}
	
	cwd->curdrv = org_cwd->curdrv;
	
	/* copy root and current directories */
	for (i = 0; i < NUM_DRIVES; i++)
	{
		dup_cookie (&cwd->root[i], &org_cwd->root[i]);
		dup_cookie (&cwd->curdir[i], &org_cwd->curdir[i]);
	}
	
	TRACE (("copy_cwd: ok (%lx)", cwd));
	return cwd;
}

void
free_cwd (struct proc *p)
{
	struct cwd *p_cwd;
	int i;
	
	assert (p && p->p_cwd && p->p_cwd->links > 0);
	p_cwd = p->p_cwd;
	p->p_cwd = NULL;
	
	if (--p_cwd->links > 0)
		return;
	
	DEBUG (("freeing p_cwd for %s", p->name));
	
	/* release the directory cookies held by the process */
	for (i = 0; i < NUM_DRIVES; i++)
	{
		release_cookie (&p_cwd->curdir[i]);
		p_cwd->curdir[i].fs = NULL;
		release_cookie (&p_cwd->root[i]);
		p_cwd->root[i].fs = NULL;
	}
	
	if (p_cwd->root_dir)
	{
		DEBUG (("free root_dir = %s", p_cwd->root_dir));
		
		release_cookie (&p_cwd->rootdir);
		p_cwd->rootdir.fs = NULL;
		kfree (p_cwd->root_dir);
		p_cwd->root_dir = NULL;
	}
	
	kfree (p_cwd);
}

/* p_sigacts */

struct sigacts *
copy_sigacts (struct proc *p)
{
	struct sigacts *p_sigacts;
	
	TRACE (("copy_sigacts: pid %i (%lx)", p->pid, p));
	assert (p && p->p_sigacts && p->p_sigacts->links > 0);
	
	p_sigacts = kmalloc (sizeof (*p_sigacts));
	if (!p_sigacts)
	{
		DEBUG(("copy_sigacts: kmalloc failed -> NULL"));
		return NULL;
	}
	
	bcopy (p->p_sigacts, p_sigacts, sizeof (*p_sigacts));
	p_sigacts->links = 1;
	
	TRACE (("copy_sigacts: ok (%lx)", p_sigacts));
	return p_sigacts;
}

void
free_sigacts (struct proc *p)
{
	struct sigacts *p_sigacts;
	
	assert (p && p->p_sigacts && p->p_sigacts->links > 0);
	p_sigacts = p->p_sigacts;
	p->p_sigacts = NULL;
	
	if (--p_sigacts->links > 0)
		return;
	
	DEBUG (("freeing p_sigacts for %s", p->name));
	
	kfree (p_sigacts);
}

/* p_limits */

struct plimit *
copy_limits (struct proc *p)
{
	return NULL;
}

void
free_limits (struct proc *p)
{
}
