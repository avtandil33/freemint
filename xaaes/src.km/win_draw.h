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

#ifndef _wind_draw_h
#define _wind_draw_h

struct xa_window;

struct xawin_functions
{
	void	(*draw_border)(struct xa_window *wind);
	void	(*draw_title)(struct xa_window *wind);
	void	(*draw_info)(struct xa_window *wind);
	void	(*draw_widgets)(struct xa_window *wind);
	void	(*draw_hslide)(struct xa_window *wind);
	void	(*draw_vslide)(struct xa_window *wind);
	void	(*draw_sizer)(struct xa_window *wind);
};

#endif /* _wind_draw_h */
