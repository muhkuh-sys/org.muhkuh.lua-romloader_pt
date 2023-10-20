/***************************************************************************
 *   Copyright (C) 2019 by Christoph Thelen                                *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "muhkuh_log.h"


/*-----------------------------------*/


muhkuh_log::muhkuh_log(void)
 : m_ptLoggerState(NULL)
 , m_iLoggerReference(LUA_NOREF)
{
}


void muhkuh_log::setLogger(lua_State *ptLoggerState, int iLoggerReference)
{
	if( ptLoggerState!=NULL && iLoggerReference!=LUA_NOREF && iLoggerReference!=LUA_REFNIL )
	{
		m_ptLoggerState = ptLoggerState;
		m_iLoggerReference = iLoggerReference;
	}
	else
	{
		m_ptLoggerState = NULL;
		m_iLoggerReference = LUA_NOREF;
	}
}


void muhkuh_log::copyLogger(muhkuh_log *ptOtherLogger)
{
	if( ptOtherLogger!=NULL )
	{
		ptOtherLogger->setLogger(this->m_ptLoggerState, this->m_iLoggerReference);
	}
}


void muhkuh_log::emerg(const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(MUHKUH_LOG_LEVEL_EMERG, pcFormat, argptr);
	va_end(argptr);
}


void muhkuh_log::alert(const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(MUHKUH_LOG_LEVEL_ALERT, pcFormat, argptr);
	va_end(argptr);
}


void muhkuh_log::fatal(const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(MUHKUH_LOG_LEVEL_FATAL, pcFormat, argptr);
	va_end(argptr);
}


void muhkuh_log::error(const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(MUHKUH_LOG_LEVEL_ERROR, pcFormat, argptr);
	va_end(argptr);
}


void muhkuh_log::warning(const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(MUHKUH_LOG_LEVEL_WARNING, pcFormat, argptr);
	va_end(argptr);
}


void muhkuh_log::notice(const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(MUHKUH_LOG_LEVEL_NOTICE, pcFormat, argptr);
	va_end(argptr);
}


void muhkuh_log::info(const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(MUHKUH_LOG_LEVEL_INFO, pcFormat, argptr);
	va_end(argptr);

}


void muhkuh_log::debug(const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(MUHKUH_LOG_LEVEL_DEBUG, pcFormat, argptr);
	va_end(argptr);
}


void muhkuh_log::trace(const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(MUHKUH_LOG_LEVEL_TRACE, pcFormat, argptr);
	va_end(argptr);
}


void muhkuh_log::log(MUHKUH_LOG_LEVEL_T tLevel, const char *pcFormat, ...)
{
	va_list argptr;


	va_start(argptr, pcFormat);
	this->vlog(tLevel, pcFormat, argptr);
	va_end(argptr);
}


void muhkuh_log::hexdump(MUHKUH_LOG_LEVEL_T tLevel, const uint8_t *pucData, uint32_t ulSize)
{
	/* This is the buffer for one line of hexdump data.
	 * A full line consists of...
	 *  8 address digits
	 *  2 separators (": ")
	 *  2*16 digits of hex data
	 *  16 separators for the hex data (" ")
	 *  1 terminating 0
	 * -------
	 *  59 bytes
	 */
	char acBuffer[59];
	const uint8_t *pucDumpCnt, *pucDumpEnd;
	uint32_t ulAddressCnt;
	size_t sizBytesLeft;
	size_t sizChunkSize;
	size_t sizChunkCnt;
	size_t sizOffset;


	// show a hexdump of the data
	pucDumpCnt = pucData;
	pucDumpEnd = pucData + ulSize;
	ulAddressCnt = 0;
	while( pucDumpCnt<pucDumpEnd )
	{
		// get number of bytes for the next line
		sizChunkSize = 16;
		sizBytesLeft = pucDumpEnd - pucDumpCnt;
		if( sizChunkSize>sizBytesLeft )
		{
			sizChunkSize = sizBytesLeft;
		}

		/* Start a line in the dump with the address. */
		sizOffset = snprintf(acBuffer, sizeof(acBuffer), "%08X: ", ulAddressCnt);
		/* Append the data bytes. */
		sizChunkCnt = 0;
		while( sizChunkCnt<sizChunkSize )
		{
			sizOffset += snprintf(acBuffer+sizOffset, sizeof(acBuffer)-sizOffset, "%02X ", *(pucDumpCnt++));
			++sizChunkCnt;
		}
		/* Print the complete line. */
		this->slog(tLevel, acBuffer);
		ulAddressCnt += sizChunkSize;
	}
}


void muhkuh_log::vlog(MUHKUH_LOG_LEVEL_T tLevel, const  char *pcFormat, va_list argptr)
{
	char acBuffer[1024];
	char *pcMessage;
	int iMsgSize;
	int iBufferSize;


	/* Get the size of the message. */
	iMsgSize = vsnprintf(acBuffer, sizeof(acBuffer), pcFormat, argptr);
	if( iMsgSize<0 )
	{
		/* Failed to print. */
		pcMessage = NULL;
	}
	else
	{
		/* Allocate a new buffer for the message and a terminating 0. */
		iBufferSize = iMsgSize + 1;
		pcMessage = (char*)malloc(iBufferSize);
		if( pcMessage!=NULL )
		{
			if( iMsgSize<sizeof(acBuffer) )
			{
				memcpy(pcMessage, acBuffer, iBufferSize);
			}
			else
			{
				vsnprintf(pcMessage, iBufferSize, pcFormat, argptr);
			}

			this->slog(tLevel, pcMessage);

			free(pcMessage);
		}
	}
}


void muhkuh_log::slog(MUHKUH_LOG_LEVEL_T tLevel, const char *pcMessage)
{
	const char *pcLogMethod;
	const LOG_LEVEL_METHOD_T *ptCnt;
	const LOG_LEVEL_METHOD_T *ptEnd;
	lua_State* ptL;
	int iLoggerRef;
	int iCnt;
	int iType;


	/* Refuse to log NULLs. */
	if( pcMessage!=NULL )
	{
		pcLogMethod = NULL;
		/* Get the logger method for the level. */
		ptCnt = this->m_atLogLevelMethod;
		ptEnd = this->m_atLogLevelMethod + (sizeof(this->m_atLogLevelMethod)/sizeof(this->m_atLogLevelMethod[0]));
		while( ptCnt<ptEnd )
		{
			if( ptCnt->tLogLevel==tLevel )
			{
				pcLogMethod = ptCnt->pcMethod;
				break;
			}
			++ptCnt;
		}
		/* Is the log level valid? */
		if( pcLogMethod!=NULL )
		{
			/* Is the logger valid? */
			ptL = m_ptLoggerState;
			iLoggerRef = m_iLoggerReference;
			if( ptL!=NULL && iLoggerRef!=LUA_NOREF && iLoggerRef!=LUA_REFNIL )
			{
				lua_rawgeti(ptL, LUA_REGISTRYINDEX, iLoggerRef);
				/* Get the type of the referenced object. It must be a table. */
				iType = lua_type(ptL, -1);
				if( iType==LUA_TTABLE )
				{
					/* Get the "info" element for a test. */
					lua_getfield(ptL, -1, pcLogMethod);
					/* Remove the logger table from the stack. */
					lua_remove(ptL, -2);

					/* Is this really a function? */
					iType = lua_type(ptL, -1);
					if( iType==LUA_TFUNCTION )
					{
						/* Call the function with a message. */
						lua_pushstring(ptL, pcMessage);
						lua_call(ptL, 1, 0);
					}
					else
					{
						fprintf(
							stderr,
							"The '%s' element of the logger table should be a function, but it is a %s.\n",
							pcLogMethod,
							lua_typename(ptL, iType)
						);
						/* Remove the invalid function from the stack. */
						lua_remove(ptL, -1);
					}
				}
				else
				{
					fprintf(stderr, "The logger must be a table, but it is a %s.\n", lua_typename(ptL, iType));
					/* Remove the invalid logger object from the stack. */
					lua_remove(ptL, -1);
				}
			}
			else
			{
				printf("[%s] %s\n", pcLogMethod, pcMessage);
			}
		}
		else
		{
			fprintf(stderr, "Invalid log level %d. Message: %s\n", tLevel, pcMessage);
		}
	}
}


const muhkuh_log::LOG_LEVEL_METHOD_T muhkuh_log::m_atLogLevelMethod[9] =
{
	{ MUHKUH_LOG_LEVEL_EMERG,   "emerg" },
	{ MUHKUH_LOG_LEVEL_ALERT,   "alert" },
	{ MUHKUH_LOG_LEVEL_FATAL,   "fatal" },
	{ MUHKUH_LOG_LEVEL_ERROR,   "error" },
	{ MUHKUH_LOG_LEVEL_WARNING, "warning" },
	{ MUHKUH_LOG_LEVEL_NOTICE,  "notice" },
	{ MUHKUH_LOG_LEVEL_INFO,    "info" },
	{ MUHKUH_LOG_LEVEL_DEBUG,   "debug" },
	{ MUHKUH_LOG_LEVEL_TRACE,   "trace" }
};
