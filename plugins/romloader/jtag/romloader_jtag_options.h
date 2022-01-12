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


#include <cstring>
#include "../../muhkuh_plugin_options.h"

#ifndef __ROMLOADER_JTAG_OPTIONS_H__
#define __ROMLOADER_JTAG_OPTIONS_H__

/*-----------------------------------*/


class romloader_jtag_options : public muhkuh_plugin_options
{
public:
	romloader_jtag_options(muhkuh_log *ptLog);
	romloader_jtag_options(const romloader_jtag_options *ptCloneMe);
	~romloader_jtag_options(void);

	virtual void set_option(const char *pcKey, lua_State *ptLuaState, int iIndex);

	typedef enum JTAG_RESET_ENUM
	{
		JTAG_RESET_HardReset = 0,
		JTAG_RESET_SoftReset = 1,
		JTAG_RESET_Attach = 2
	} JTAG_RESET_T;

	JTAG_RESET_T getOption_jtagReset(void);
	unsigned long getOption_jtagFrequencyKhz(void);

private:
	typedef struct JTAG_RESET_TO_NAME_STRUCT
	{
		JTAG_RESET_T tJtagReset;
		const char *pcName;
	} JTAG_RESET_TO_NAME_T;

	const JTAG_RESET_TO_NAME_T atJtagResetToName[3] =
	{
		{ JTAG_RESET_HardReset, "HardReset" },
		{ JTAG_RESET_SoftReset, "SoftReset" },
		{ JTAG_RESET_Attach,    "Attach" }
	};

	JTAG_RESET_T m_tOption_jtagReset;
	unsigned long m_tOption_jtagFrequencyKhz;
};


#endif  /* __ROMLOADER_JTAG_OPTIONS_H__ */
