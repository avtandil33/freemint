/*
 * This file belongs to FreeMiNT. It's not in the original MiNT 1.12
 * distribution. See the file CHANGES for a detailed log of changes.
 * 
 * 
 * Copyright 2004 Frank Naumann <fnaumann@freemint.de>
 * All rights reserved.
 * 
 * Please send suggestions, patches or bug reports to me or
 * the MiNT mailing list
 * 
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
 */

# ifndef _semaphores_h
# define _semaphores_h

# include "mint/mint.h"

#if 0

struct semaphore
{
	volatile unsigned short lock;
	unsigned short sleepers;
	short pid;
	short pad;
};

# if __KERNEL__ == 1

void _semaphore_init(struct semaphore *s, const char *);
void _semaphore_lock(struct semaphore *s, const char *);
int  _semaphore_rel (struct semaphore *s, const char *);
int  _semaphore_try (struct semaphore *s, const char *);

# define semaphore_init(s)	_semaphore_init(s, FUNCTION)
# define semaphore_lock(s)	_semaphore_lock(s, FUNCTION)
# define semaphore_rel(s)	_semaphore_rel (s, FUNCTION)
# define semaphore_try(s)	_semaphore_try (s, FUNCTION)

# endif

#endif

# endif /* _semaphores_h */
