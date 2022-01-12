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

#include "romloader_jtag_options.h"


/*-------------------------------------*/


romloader_jtag_options::romloader_jtag_options(muhkuh_log *ptLog)
 : muhkuh_plugin_options(ptLog)
 , m_tOption_jtagReset(JTAG_RESET_Attach)
 , m_tOption_jtagFrequencyKhz(0)
{
}


romloader_jtag_options::romloader_jtag_options(const romloader_jtag_options *ptCloneMe)
 : muhkuh_plugin_options(ptCloneMe)
 , m_tOption_jtagReset(ptCloneMe->m_tOption_jtagReset)
 , m_tOption_jtagFrequencyKhz(ptCloneMe->m_tOption_jtagFrequencyKhz)
{
}


romloader_jtag_options::~romloader_jtag_options(void)
{
}


void romloader_jtag_options::set_option(const char *pcKey, lua_State *ptLuaState, int iIndex)
{
	int iType;
	const char *pcValue;
	lua_Number dValue;
	unsigned long ulValue;
	const JTAG_RESET_TO_NAME_T *ptCnt;
	const JTAG_RESET_TO_NAME_T *ptEnd;
	const JTAG_RESET_TO_NAME_T *ptHit;


	if( strcmp(pcKey, "jtag_reset")==0 )
	{
		/* The value for the JTAG reset must be a string. */
		iType = lua_type(ptLuaState, iIndex);
		if( iType!=LUA_TSTRING )
		{
			m_ptLog->debug("Ignoring option '%s': the value must be a string, but it is a %s.", pcKey, lua_typename(ptLuaState, iType));
		}
		else
		{
			pcValue = lua_tostring(ptLuaState, iIndex);
			ptCnt = atJtagResetToName;
			ptEnd = atJtagResetToName + (sizeof(atJtagResetToName)/sizeof(atJtagResetToName[0]));
			ptHit = NULL;
			while( ptCnt<ptEnd )
			{
				if( strcmp(pcValue, ptCnt->pcName)==0 )
				{
					ptHit = ptCnt;
					break;
				}
				else
				{
					++ptCnt;
				}
			}
			if( ptHit==NULL )
			{
				m_ptLog->debug("Ignoring option '%s': the value '%s' is invalid.", pcKey, pcValue);
			}
			else
			{
				m_tOption_jtagReset = ptHit->tJtagReset;
				m_ptLog->debug("Setting option '%s' to '%s'.", pcKey, pcValue);
			}
		}
	}
	else if( strcmp(pcKey, "jtag_frequency_khz")==0 )
	{
		/* The value for the JTAG frequency must be a number. */
		iType = lua_type(ptLuaState, iIndex);
		if( iType!=LUA_TNUMBER )
		{
			m_ptLog->debug("Ignoring option '%s': the value must be a number, but it is a %s.", pcKey, lua_typename(ptLuaState, iType));
		}
		else
		{
			dValue = lua_tonumber(ptLuaState, iIndex);
			ulValue = (unsigned long)dValue;
			m_tOption_jtagFrequencyKhz = ulValue;
			m_ptLog->debug("Setting option '%s' to %d.", pcKey, ulValue);
		}
	}
	else
	{
		m_ptLog->debug("Ignoring unknown option '%s'.", pcKey);
	}
}


romloader_jtag_options::JTAG_RESET_T romloader_jtag_options::getOption_jtagReset(void)
{
	return m_tOption_jtagReset;
}


unsigned long romloader_jtag_options::getOption_jtagFrequencyKhz(void)
{
	return m_tOption_jtagFrequencyKhz;
}
