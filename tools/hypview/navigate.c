/*
 * $Id$
 * 
 * HypView - (c)      - 2006 Philipp Donze
 *               2006 -      Philipp Donze & Odd Skancke
 *
 * A replacement hypertext viewer
 *
 * This file is part of HypView.
 *
 * HypView is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HypView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HypView; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef __GNUC__
#include <string.h>
#include <ctype.h>
#include <mintbind.h>
#include <fcntl.h>
#include <mt_gem.h>
#include <stdio.h>
#include <macros.h>

#include "include/types.h"
#include "include/scancode.h"
// #include "include/av.h"
// #include "mem.h"
#include "diallib.h"
#include "hyp.h"
#include "stat.h"
#else
#include <string.h>
#include <tos.h>
#include <aes.h>
#include "diallib.h"
#include SPEC_DEFINITION_FILE
#endif

extern WINDOW_DATA *Win;

void GotoPage(DOCUMENT *doc, long num, long line)
{
	WINDOW_DATA *win = Win;

	graf_mouse(BUSYBEE,NULL);
	doc->gotoNodeProc(doc,NULL,num);
	doc->start_line=line;
	graf_mouse(ARROW,NULL);
	ReInitWindow(win,doc);
}

void GoBack(DOCUMENT *old_doc)
{
	WINDOW_DATA *win = Win;
	DOCUMENT *new_doc;
	long page;
	long line;
	if(RemoveHistoryEntry(&new_doc,&page,&line))
	{
		/*	Bereits geschlossene Datei ffnen? => Datei wechsel	*/
		if(new_doc!=old_doc)
		{
		long ret;
	
			/*	Falls das alte Dokument nicht mehr bentigt wird...	*/
			if(!CountDocumentHistoryEntries(old_doc))
			{
				win->data = old_doc->next;
				CloseFile(old_doc);		/*	Dokument schliessen	*/
			}
			else
			{
			DOCUMENT *prev_doc=old_doc;
				old_doc->closeProc(old_doc);

				/*	Platziere neues Dokument an den Anfang der Liste	*/
				while(prev_doc->next != new_doc)
					prev_doc=prev_doc->next;
				prev_doc->next=new_doc->next;
				new_doc->next=old_doc;
			}

			/*	neue Datei laden	*/
			ret=Fopen(new_doc->path,O_RDONLY);
			if(ret>=0)
				LoadFile(new_doc,(short)ret);
			else
				FileErrorNr(new_doc->filename,ret);
			Fclose((short)ret);
		}

		if(new_doc->type>=0)					/*	bekannter Typ?	*/
			GotoPage(new_doc,page,line);	/*	Zur Seite wechseln	*/
		else
			FileError(new_doc->filename,"format not recognized");
	}
}

void MoreBackPopup(DOCUMENT *old_doc, short x, short y)
{
	WINDOW_DATA *win = Win;
	OBJECT *tree=tree_addr[EMPTYPOPUP];
	short i=EM_1,m;
	long len=0;
	HISTORY *entry=history;
	while(entry)
	{
		if(entry->win == win)
		{
			tree[i].ob_flags &= ~OF_HIDETREE;
			tree[i].ob_spec.free_string=&entry->title;
			len=max(strlen(&entry->title)+1,len);
			i++;
		}
		if(i>EM_12)
			break;
		entry=entry->next;
	}

	/* Hide unused entries */	
	m=i;
	while(i<=EM_12)
		tree[i++].ob_flags |= OF_HIDETREE;
	i=m;
	
	len=len*pwchar;
/* [GS] 0.35.2a Start */
	tree[EM_BACK].ob_x = x;
	tree[EM_BACK].ob_y = y + 5;
/* Ende */
	tree[EM_BACK].ob_width=(short)len;
	tree[EM_BACK].ob_height=(i-EM_1)*phchar;
	
	while(--i>=EM_1)
	{
		tree[i].ob_width=(short)len;
		tree[i].ob_height=phchar;
	}

/* [GS] 0.35.2a Start */
	i=form_popup(tree_addr[EMPTYPOPUP], 0, 0);
/* Ende; alt:
	i=form_popup(tree_addr[EMPTYPOPUP],x+(short)(len>>1),y+5);
*/
	if(i>0)
	{
	HISTORY *selected_entry=history;
	DOCUMENT *new_doc=old_doc;
	long page;
	long line;

		/*	Den ausgewhlten Eintrag bestimmen	*/
		while(selected_entry)
		{
			if(selected_entry->win == win)
			{
				i--;
				if(i==0)
					break;
			}
			selected_entry=selected_entry->next;
		}

		/*	Bisheriges Dokument freigeben	*/
		old_doc->closeProc(old_doc);

		/*	So viele Eintrge wie ntig entfernen	*/
		entry=history;
		for(;;)
		{
			if(entry->win == win)
			{
				RemoveHistoryEntry(&new_doc,&page,&line);

				/*	Anderes Dokument? => Dokument schliessen	*/
				if(new_doc!=old_doc)
				{
					/*	Falls das alte Dokument nicht mehr bentigt wird...	*/
					if(!CountDocumentHistoryEntries(old_doc))
					{
						win->data = old_doc->next;
						CloseFile(old_doc);		/*	Dokument schliessen	*/
					}
					else
					{
					DOCUMENT *prev_doc=old_doc;

						/*	Platziere neues Dokument an den Anfang der Liste	*/
						while(prev_doc->next != new_doc)
							prev_doc=prev_doc->next;
						prev_doc->next=new_doc->next;
						new_doc->next=old_doc;
					}
					old_doc=new_doc;
				}

				if(entry==selected_entry)
					break;
				entry=history;
			}
			else
				entry=entry->next;
		}

		if(!new_doc->data)
		{
		long ret;
			/*	neue Datei laden	*/
			ret=Fopen(new_doc->path,O_RDONLY);
			if(ret>=0)
				LoadFile(new_doc,(short)ret);
			else
				FileErrorNr(new_doc->filename,ret);
			Fclose((short)ret);
		}

		if(new_doc->type>=0)					/*	bekannter Typ?	*/
			GotoPage(new_doc,page,line);	/*	Zur Seite wechseln	*/
		else
			FileError(new_doc->filename,"format not recognized");
	}
}




/****** Module dependend	****/
#ifndef __GNUC__
#include "source\hyp.h"
#endif

void GotoHelp(DOCUMENT *doc)
{
	WINDOW_DATA *win = Win;
	HYP_DOCUMENT *hyp = doc->data;

	if(hyp->help_page>=0)
	{
		short page_nr = hyp->help_page;
		/*	Seite wird schon angezeigt?	*/
		if(page_nr != hyp->entry->number)
		{
			AddHistoryEntry(win);
			GotoPage(doc, page_nr, 0);
		}
		else
		{
			/*	Visuelles Feedback	*/
			graf_mouse(BUSYBEE,NULL);
			evnt_timer(10);
			graf_mouse(ARROW,NULL);
		}
	}
}

void GotoIndex(DOCUMENT *doc)
{
	WINDOW_DATA *win = Win;
/* 	extern char *index_entry; */
	HYP_DOCUMENT *hyp=doc->data;

	if(hyp->index_page!=hyp->entry->number)
	{
		AddHistoryEntry(win);
		GotoPage(doc,hyp->index_page,0);
	}
	else
	{
		/*	Visuelles Feedback	*/
		graf_mouse(BUSYBEE,NULL);
		evnt_timer(10);
		graf_mouse(ARROW,NULL);
	}
}

void GoThisButton(DOCUMENT *doc,short obj)
{
	WINDOW_DATA *win = Win;			
	HYP_DOCUMENT *hyp=doc->data;
	short new_node = 0;

	if(obj==TO_NEXT)
		new_node=hyp->indextable[hyp->entry->number]->next;
	else if(obj==TO_PREVIOUS)
		new_node=hyp->indextable[hyp->entry->number]->previous;
	else if(obj==TO_HOME)
		new_node=hyp->indextable[hyp->entry->number]->dir_index;
	
	new_node = max(0, new_node);
	new_node = min(hyp->num_index-1, new_node);

	/*	Letzte Seite erreicht?	*/
	if(new_node == doc->getNodeProc(doc))
		return;

	/*	Eintrag ist keine Seite?	*/
	if(hyp->indextable[new_node]->type > POPUP)
		return;
	
	if(obj == TO_HOME)
		AddHistoryEntry(win);
	GotoPage(doc,new_node,0);
}

