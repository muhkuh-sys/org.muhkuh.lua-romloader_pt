/***************************************************************************
 *   Copyright (C) 2022 by Christoph Thelen                                *
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

extern "C" {
#include "lua.h"
}
#include "muhkuh_log.h"

#ifndef __MUHKUH_PLUGIN_OPTIONS__
#define __MUHKUH_PLUGIN_OPTIONS__


class muhkuh_plugin_options
{
public:
	muhkuh_plugin_options(muhkuh_log *ptLog);
	muhkuh_plugin_options(const muhkuh_plugin_options *ptCloneMe);
	~muhkuh_plugin_options(void);

	void setLog(muhkuh_log *ptLog);
	virtual void set_option(const char *pcKey, lua_State *ptLuaState, int iIndex) = 0;

protected:
	muhkuh_log *m_ptLog;
};


#endif  /* __MUHKUH_PLUGIN_OPTIONS__ */
