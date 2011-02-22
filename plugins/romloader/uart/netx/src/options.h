/***************************************************************************
 *   Copyright (C) 2010 by Christoph Thelen                                *
 *   doc_bacardi@users.sourceforge.net                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#ifndef __OPTIONS_H__
#define __OPTIONS_H__


typedef struct
{
	union
	{
		unsigned char auc[16*sizeof(unsigned long)];
		unsigned long aul[16];
	} uCfg;
} USB_CONFIGURATION_T;

typedef struct
{
	unsigned long ul_main;
} DEBUG_CONFIGURATION_T;


typedef struct
{
	USB_CONFIGURATION_T t_usb_settings;

#if CFG_DEBUGMSG==1
	/* TODO: Always enable this block once the debug messages are
	 * included in the normal build.
	 */
	/* debug config */
	DEBUG_CONFIGURATION_T t_debug_settings;
#endif
} OPTIONS_T;


extern OPTIONS_T g_t_options;

void options_set_default(void);

#endif	/* __OPTIONS_H__ */

