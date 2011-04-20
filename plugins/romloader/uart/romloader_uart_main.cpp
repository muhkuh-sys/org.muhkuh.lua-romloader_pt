/***************************************************************************
 *   Copyright (C) 2008 by Christoph Thelen                                *
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

#include <ctype.h>
#include <stdio.h>

#include "romloader_uart_main.h"

#define UART_BASE_TIMEOUT_MS 500
#define UART_CHAR_TIMEOUT_MS 10

#ifdef _WIN32
#       define snprintf _snprintf
#endif

/*-------------------------------------*/

const char *romloader_uart_provider::m_pcPluginNamePattern = "romloader_uart_%s";

romloader_uart_provider::romloader_uart_provider(swig_type_info *p_romloader_uart, swig_type_info *p_romloader_uart_reference)
 : muhkuh_plugin_provider("romloader_uart")
{
	int iResult;


	printf("%s(%p): provider create\n", m_pcPluginId, this);

	/* get the romloader_uart lua type */
	m_ptPluginTypeInfo = p_romloader_uart;
	m_ptReferenceTypeInfo = p_romloader_uart_reference;
}


romloader_uart_provider::~romloader_uart_provider(void)
{
	printf("%s(%p): provider delete\n", m_pcPluginId, this);
}


int romloader_uart_provider::DetectInterfaces(lua_State *ptLuaStateForTableAccess)
{
	int iResult;
	size_t sizDeviceNames;
	char **ppcDeviceNames;
	char **ppcDeviceNamesCnt;
	char **ppcDeviceNamesEnd;
	romloader_uart_reference *ptReference;
	bool fDeviceIsBusy;


	/* detect all interfaces */
	sizDeviceNames = romloader_uart_device_platform::ScanForPorts(&ppcDeviceNames);
	printf("found %d devs, %p\n", sizDeviceNames, ppcDeviceNames);

	if( ppcDeviceNames!=NULL )
	{
		/* add all detected devices to the table */
		ppcDeviceNamesCnt = ppcDeviceNames;
		ppcDeviceNamesEnd = ppcDeviceNames + sizDeviceNames;
		while( ppcDeviceNamesCnt<ppcDeviceNamesEnd )
		{
			/* TODO: check if the device is busy, but how */
			fDeviceIsBusy = false;
	
			/* create the new instance */
			printf("create instance '%s'\n", *ppcDeviceNamesCnt);

			ptReference = new romloader_uart_reference(*ppcDeviceNamesCnt, m_pcPluginId, fDeviceIsBusy, this);
			add_reference_to_table(ptLuaStateForTableAccess, ptReference);

			/* free the name */
			free(*ppcDeviceNamesCnt);

			/* next device */
			++ppcDeviceNamesCnt;
		}

		/* free the list */
		free(ppcDeviceNames);
	}

	return sizDeviceNames;
}


romloader_uart *romloader_uart_provider::ClaimInterface(const muhkuh_plugin_reference *ptReference)
{
	romloader_uart *ptPlugin;
	const char *pcName;
	char acDevice[PATH_MAX];


	/* expect error */
	ptPlugin = NULL;


	if( ptReference==NULL )
	{
		fprintf(stderr, "%s(%p): claim_interface(): missing reference!\n", m_pcPluginId, this);
	}
	else
	{
		pcName = ptReference->GetName();
		if( pcName==NULL )
		{
			fprintf(stderr, "%s(%p): claim_interface(): missing name!\n", m_pcPluginId, this);
		}
		else if( sscanf(pcName, m_pcPluginNamePattern, acDevice)!=1 )
		{
			fprintf(stderr, "%s(%p): claim_interface(): invalid name: %s\n", m_pcPluginId, this, pcName);
		}
		else
		{
			ptPlugin = new romloader_uart(pcName, m_pcPluginId, this, acDevice);
			printf("%s(%p): claim_interface(): claimed interface %s.\n", m_pcPluginId, this, pcName);
		}
	}

	return ptPlugin;
}


bool romloader_uart_provider::ReleaseInterface(muhkuh_plugin *ptPlugin)
{
	bool fOk;
	const char *pcName;
	char acDevice[PATH_MAX];


	/* expect error */
	fOk = false;


	if( ptPlugin==NULL )
	{
		fprintf(stderr, "%s(%p): release_interface(): missing plugin!\n", m_pcPluginId, this);
	}
	else
	{
		pcName = ptPlugin->GetName();
		if( pcName==NULL )
		{
			fprintf(stderr, "%s(%p): release_interface(): missing name!\n", m_pcPluginId, this);
		}
		else if( sscanf(pcName, m_pcPluginNamePattern, acDevice)!=1 )
		{
			fprintf(stderr, "%s(%p): release_interface(): invalid name: %s\n", m_pcPluginId, this, pcName);
		}
		else
		{
			printf("%s(%p): released interface %s.\n", m_pcPluginId, this, pcName);
			fOk = true;
		}
	}

	return fOk;
}


/*-------------------------------------*/


romloader_uart::romloader_uart(const char *pcName, const char *pcTyp, romloader_uart_provider *ptProvider, const char *pcDeviceName)
 : romloader(pcName, pcTyp, ptProvider)
 , m_fIsConnected(false)
 , m_ptUartDev(NULL)
{
	printf("%s(%p): created in romloader_uart\n", m_pcName, this);

	m_ptUartDev = new romloader_uart_device_platform(pcDeviceName);

	packet_ringbuffer_init();
}


romloader_uart::~romloader_uart(void)
{
	printf("%s(%p): deleted in romloader_uart\n", m_pcName, this);

	if( m_ptUartDev!=NULL )
	{
		m_ptUartDev->Close();
	}
}


void romloader_uart::hexdump(const unsigned char *pucData, unsigned long ulSize)
{
	const unsigned char *pucDumpCnt, *pucDumpEnd;
	unsigned long ulAddressCnt;
	size_t sizBytesLeft;
	size_t sizChunkSize;
	size_t sizChunkCnt;


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

		// start a line in the dump with the address
		printf("%08lX: ", ulAddressCnt);
		// append the data bytes
		sizChunkCnt = sizChunkSize;
		while( sizChunkCnt!=0 )
		{
			printf("%02X ", *(pucDumpCnt++));
			--sizChunkCnt;
		}
		// next line
		printf("\n");
		ulAddressCnt += sizChunkSize;
	}
}


bool romloader_uart::chip_init(lua_State *ptClientData)
{
	bool fResult;


	switch( m_tChiptyp )
	{
	case ROMLOADER_CHIPTYP_NETX500:
	case ROMLOADER_CHIPTYP_NETX100:
		switch( m_tRomcode )
		{
		case ROMLOADER_ROMCODE_ABOOT:
			// aboot does not set the serial vectors
			write_data32(ptClientData, 0x10001ff0, 0);
			write_data32(ptClientData, 0x10001ff4, 0);
			write_data32(ptClientData, 0x10001ff8, 0);
			write_data32(ptClientData, 0x10001ffc, 0);
			fResult = true;
			break;
		case ROMLOADER_ROMCODE_HBOOT:
			// hboot needs no special init
			fResult = true;
			break;
		case ROMLOADER_ROMCODE_HBOOT2_SOFT:
			/* hboot2 software emu needs no special init. */
			fResult = true;
			break;
		case ROMLOADER_ROMCODE_UNKNOWN:
			fResult = false;
			break;
		}
		break;

	case ROMLOADER_CHIPTYP_NETX50:
		switch( m_tRomcode )
		{
		case ROMLOADER_ROMCODE_ABOOT:
			// this is an unknown combination
			fResult = false;
			break;
		case ROMLOADER_ROMCODE_HBOOT:
			// hboot needs no special init
			// netx50 needs sdram fix
			printf("%s(%p): running netx50 sdram fix...\n", m_pcName, this);
			write_data32(ptClientData, 0x1c005830, 0x00000001);
			write_data32(ptClientData, 0x1c005104, 0xbffffffc);
			write_data32(ptClientData, 0x1c00510c, 0x00480001);
			write_data32(ptClientData, 0x1c005110, 0x00000001);
			printf("%s(%p): netx50 sdram fix ok!\n", m_pcName, this);
			write_data32(ptClientData, 0x1c00510c, 0);
			write_data32(ptClientData, 0x1c005110, 0);
			write_data32(ptClientData, 0x1c005830, 0);
			fResult = true;
			break;
		case ROMLOADER_ROMCODE_UNKNOWN:
			fResult = false;
			break;
		}
		break;

	case ROMLOADER_CHIPTYP_UNKNOWN:
		fResult = false;
		break;
	}

	return fResult;
}


void romloader_uart::Connect(lua_State *ptClientData)
{
	int iResult;


	/* Expect error. */
	iResult = -1;

	printf("%s(%p): connect\n", m_pcName, this);

	if( m_ptUartDev!=NULL && m_fIsConnected==false )
	{
		if( m_ptUartDev->Open()!=true )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to open device!", m_pcName, this);
		}
/*
		else if( m_ptUartDev->IdentifyLoader()!=true )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to identify loader!", m_pcName, this);
		}
*/
		else if( detect_chiptyp(ptClientData)!=true )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to detect chiptyp!", m_pcName, this);
		}
/*
		else if( chip_init(ptClientData)!=true )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to init chip!", m_pcName, this);
		}
*/
		else
		{
			m_fIsConnected = true;
			iResult = 0;
		}

		if( iResult!=0 )
		{
			m_ptUartDev->Close();
			MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
		}
	}
}


void romloader_uart::Disconnect(lua_State *ptClientData)
{
	printf("%s(%p): disconnect\n", m_pcName, this);

	if( m_ptUartDev!=NULL )
	{
		m_ptUartDev->Close();
	}

	m_fIsConnected = false;
}



void romloader_uart::packet_ringbuffer_init(void)
{
	m_sizPacketRingBufferHead = 0;
	m_sizPacketRingBufferFill = 0;
}


int romloader_uart::packet_ringbuffer_fill(size_t sizRequestedFillLevel)
{
	size_t sizWritePosition;
	int iResult;
	size_t sizReceiveCnt;
	size_t sizChunk;
	size_t sizRead;


	/* Expect success. */
	iResult = 0;

	/* Does the buffer contain enough data? */
	if( m_sizPacketRingBufferFill<sizRequestedFillLevel )
	{
		/* No -> receive the remaining amount of bytes. */
		sizReceiveCnt = sizRequestedFillLevel - m_sizPacketRingBufferFill;
		do
		{
			/* Get the write position. */
			sizWritePosition = m_sizPacketRingBufferHead + m_sizPacketRingBufferFill;
			if( sizWritePosition>=MONITOR_MAX_PACKET_SIZE )
			{
				sizWritePosition -= MONITOR_MAX_PACKET_SIZE;
			}

			/* Get the size of the remaining continuous buffer space. */
			/* This is the maximum chunk which can be read in one piece. */
			sizChunk = MONITOR_MAX_PACKET_SIZE - sizWritePosition;
			/* Limit the chunk size to the requested size. */
			if( sizChunk>sizReceiveCnt )
			{
				sizChunk = sizReceiveCnt;
			}

			/* Receive the chunk. */
			sizRead = m_ptUartDev->RecvRaw(m_aucPacketRingBuffer+sizWritePosition, sizChunk, UART_BASE_TIMEOUT_MS + sizChunk*UART_CHAR_TIMEOUT_MS);

			m_sizPacketRingBufferFill += sizChunk;
			sizReceiveCnt -= sizChunk;

			if( sizRead!=sizChunk )
			{
				iResult = -1;
				break;
			}
		} while( sizReceiveCnt>0 );
	}

	return iResult;
}


unsigned char romloader_uart::packet_ringbuffer_get(void)
{
	unsigned char ucByte;


	ucByte = m_aucPacketRingBuffer[m_sizPacketRingBufferHead];

	++m_sizPacketRingBufferHead;
	if( m_sizPacketRingBufferHead>=MONITOR_MAX_PACKET_SIZE )
	{
		m_sizPacketRingBufferHead -= MONITOR_MAX_PACKET_SIZE;
	}

	--m_sizPacketRingBufferFill;

	return ucByte;
}


int romloader_uart::packet_ringbuffer_peek(size_t sizOffset)
{
	size_t sizReadPosition;


	sizReadPosition = m_sizPacketRingBufferHead + sizOffset;
	if( sizReadPosition>=MONITOR_MAX_PACKET_SIZE )
	{
		sizReadPosition -= MONITOR_MAX_PACKET_SIZE;
	}

	return m_aucPacketRingBuffer[sizReadPosition];
}


int romloader_uart::send_packet(const unsigned char *pucData, size_t sizData)
{
	unsigned char aucBuffer[MONITOR_MAX_PACKET_SIZE];
	int iResult;
	unsigned short usCrc;
	const unsigned char *pucCnt;
	const unsigned char *pucEnd;
	unsigned char ucData;
	size_t sizSend;
	size_t sizPacket;


	/* Expect failure. */
	iResult = -1;

	/* The maximum packet size is reduced by the start char (1 byte), the size information (2 bytes) and the crc (2 bytes). */
	if( sizData<=MONITOR_MAX_PACKET_SIZE-5 )
	{
		/* Set the packet start character. */
		aucBuffer[0] = MONITOR_STREAM_PACKET_START;
		/* Set the size of the user data. */
		aucBuffer[1] = (unsigned char)(sizData & 0xff);
		aucBuffer[2] = (unsigned char)(sizData >> 8);

		/* Copy the user data. */
		memcpy(aucBuffer+3, pucData, sizData);

		/* Generate the crc. */
		usCrc = 0;
		pucCnt = aucBuffer + 1;
		pucEnd = aucBuffer + 1 + 2 + sizData;
		while( pucCnt<pucEnd )
		{
			usCrc = crc16(usCrc, *pucCnt);
			++pucCnt;
		}

		/* Append the CRC. */
		aucBuffer[sizData+3] = (usCrc>>8 ) & 0xffU;
		aucBuffer[sizData+4] =  usCrc      & 0xffU;

		/* Send the buffer. */
		sizPacket = sizData + 5;
		sizSend = m_ptUartDev->SendRaw(aucBuffer, sizPacket, UART_BASE_TIMEOUT_MS + sizPacket*UART_CHAR_TIMEOUT_MS);
		if( sizSend==sizPacket )
		{
			iResult = 0;
		}
		else
		{
			fprintf(stderr, "! send_packet: failed to send the packet!\n");
		}
	}
	else
	{
		fprintf(stderr, "! send_packet: packet too large: %d bytes!\n", sizData);
	}

	return iResult;
}


int romloader_uart::receive_packet(void)
{
	size_t sizRead;
	unsigned int uiRetries;
	unsigned char ucData;
	bool fFound;
	int iResult;
	size_t sizData;
	size_t sizPacket;
	unsigned short usCrc;
	size_t sizCnt;


	/* Wait for the start character. */
	fFound = false;
	uiRetries = 10;
	do
	{
		iResult = packet_ringbuffer_fill(1);
		if( iResult==0 )
		{
			ucData = packet_ringbuffer_get();
		}
		if( ucData==MONITOR_STREAM_PACKET_START )
		{
			fFound = true;
			break;
		}
		--uiRetries;
	} while( uiRetries>0 );

	if( fFound!=true )
	{
		fprintf(stderr, "receive_packet: no start char found!\n");
		iResult = -1;
	}
	else
	{
		/* Get the size information. */
		iResult = packet_ringbuffer_fill(2);
		if( iResult==0 )
		{
			sizData  = packet_ringbuffer_peek(0);
			sizData |= packet_ringbuffer_peek(1) << 8;

			/* The complete packet consists of the size info, the user data and the crc.*/
			sizPacket = 2 + sizData + 2;

			/* Get the rest of the packet. */
			iResult = packet_ringbuffer_fill(sizPacket);
			if( iResult==0 )
			{
				/* Generate the crc. */
				usCrc = 0;
				sizCnt = 0;
				do
				{
					ucData = packet_ringbuffer_peek(sizCnt);
					usCrc = crc16(usCrc, ucData);
					++sizCnt;
				} while( sizCnt<sizPacket );
				if( usCrc==0 )
				{
					/* Get the complete packet. */
					sizCnt = 0;
					do
					{
						m_aucPacketInputBuffer[sizCnt] = packet_ringbuffer_get();
						++sizCnt;
					} while( sizCnt<sizPacket );

					m_sizPacketInputBuffer = sizPacket;
				}
				else
				{
					fprintf(stderr, "! receive_packet: crc failed.\n");

					/* Debug: get the complete packet and dump it. */
					printf("packet size: 0x%08x bytes\n", sizPacket);
					sizCnt = 0;
					do
					{
						m_aucPacketInputBuffer[sizCnt] = packet_ringbuffer_get();
						++sizCnt;
					} while( sizCnt<sizPacket );
					printf("Packet data:\n");
					hexdump(m_aucPacketInputBuffer, sizPacket);

					iResult = -1;
				}
			}
		}
	}

	return iResult;
}


int romloader_uart::execute_command(const unsigned char *aucCommand, size_t sizCommand)
{
	int iResult;
	unsigned char ucStatus;


	iResult = send_packet(aucCommand, sizCommand);
	if( iResult!=0 )
	{
		fprintf(stderr, "! execute_command: send_packet failed with errorcode %d\n", iResult);
	}
	else
	{
		iResult = receive_packet();
		if( iResult!=0 )
		{
			fprintf(stderr, "! execute_command: receive_packet failed with errorcode %d\n", iResult);
		}
		else
		{
			if( m_sizPacketInputBuffer<5 )
			{
				fprintf(stderr, "Error: received no user data!\n");
				iResult = -1;
			}
			else
			{
				ucStatus = m_aucPacketInputBuffer[2];
				if( ucStatus==0 )
				{
					iResult = 0;
				}
				else
				{
					fprintf(stderr, "Error: status is not ok: %d\n", ucStatus);
					iResult = -1;
				}
			}
		}
	}

	return iResult;
}


unsigned char romloader_uart::read_data08(lua_State *ptClientData, unsigned long ulNetxAddress)
{
	unsigned char aucCommand[6];
	int iResult;
	unsigned char ucValue;
	bool fOk;


	/* Expect error. */
	fOk = false;

	ucValue = 0;

	aucCommand[0] = MONITOR_COMMAND_Read | (MONITOR_ACCESSSIZE_Byte<<6);
	aucCommand[1] = 1;
	aucCommand[2] =  ulNetxAddress      & 0xffU;
	aucCommand[3] = (ulNetxAddress>>8 ) & 0xffU;
	aucCommand[4] = (ulNetxAddress>>16) & 0xffU;
	aucCommand[5] = (ulNetxAddress>>24) & 0xffU;
	iResult = execute_command(aucCommand, 6);
	if( iResult!=0 )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to execute command!", m_pcName, this);
	}
	else
	{
		if( m_sizPacketInputBuffer!=4+2 )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): answer to read_data08 has wrong packet size of %d!", m_pcName, this, m_sizPacketInputBuffer);
		}
		else
		{
			ucValue = m_aucPacketInputBuffer[3];
			fOk = true;
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}

	return ucValue;
}


unsigned short romloader_uart::read_data16(lua_State *ptClientData, unsigned long ulNetxAddress)
{
	unsigned char aucCommand[6];
	int iResult;
	unsigned short usValue;
	bool fOk;


	/* Expect error. */
	fOk = false;

	usValue = 0;

	aucCommand[0] = MONITOR_COMMAND_Read | (MONITOR_ACCESSSIZE_Word<<6);
	aucCommand[1] = 2;
	aucCommand[2] =  ulNetxAddress      & 0xffU;
	aucCommand[3] = (ulNetxAddress>>8 ) & 0xffU;
	aucCommand[4] = (ulNetxAddress>>16) & 0xffU;
	aucCommand[5] = (ulNetxAddress>>24) & 0xffU;
	iResult = execute_command(aucCommand, 6);
	if( iResult!=0 )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to execute command!", m_pcName, this);
	}
	else
	{
		if( m_sizPacketInputBuffer!=4+3 )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): answer to read_data16 has wrong packet size of %d!", m_pcName, this, m_sizPacketInputBuffer);
		}
		else
		{
			usValue = m_aucPacketInputBuffer[3] |
				m_aucPacketInputBuffer[4]<<8;
			fOk = true;
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}

	return usValue;
}


unsigned long romloader_uart::read_data32(lua_State *ptClientData, unsigned long ulNetxAddress)
{
	unsigned char aucCommand[6];
	int iResult;
	unsigned long ulValue;
	bool fOk;


	/* Expect error. */
	fOk = false;

	ulValue = 0;

	aucCommand[0] = MONITOR_COMMAND_Read | (MONITOR_ACCESSSIZE_Long<<6);
	aucCommand[1] = 4;
	aucCommand[2] =  ulNetxAddress      & 0xffU;
	aucCommand[3] = (ulNetxAddress>>8 ) & 0xffU;
	aucCommand[4] = (ulNetxAddress>>16) & 0xffU;
	aucCommand[5] = (ulNetxAddress>>24) & 0xffU;
	iResult = execute_command(aucCommand, 6);
	if( iResult!=0 )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to execute command!", m_pcName, this);
	}
	else
	{
		if( m_sizPacketInputBuffer!=4+5 )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): answer to read_data32 has wrong packet size of %d!", m_pcName, this, m_sizPacketInputBuffer);
		}
		else
		{
			ulValue = m_aucPacketInputBuffer[3] |
				m_aucPacketInputBuffer[4]<<8 |
				m_aucPacketInputBuffer[5]<<16 |
				m_aucPacketInputBuffer[6]<<24;
			fOk = true;
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}

	return ulValue;
}


void romloader_uart::read_image(unsigned long ulNetxAddress, unsigned long ulSize, char **ppcBUFFER_OUT, size_t *psizBUFFER_OUT, SWIGLUA_REF tLuaFn, long lCallbackUserData)
{
	char *pcBuffer;
	size_t sizBuffer;
	bool fOk;
	size_t sizChunk;
	int iResult;
	unsigned char aucCommand[MONITOR_MAX_PACKET_SIZE];
	bool fIsRunning;
	long lBytesProcessed;


	/* Be optimistic. */
	fOk = true;

	if( m_fIsConnected==false )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(tLuaFn.L, "%s(%p): not connected!", m_pcName, this);
		fOk = false;
	}
	else if( ulSize==0 )
	{
		pcBuffer = NULL;
		sizBuffer = 0;
	}
	else
	{
		sizBuffer = ulSize;
		pcBuffer = (char*)malloc(sizBuffer);
		if( pcBuffer==NULL )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(tLuaFn.L, "%s(%p): failed to allocate %d bytes!", m_pcName, this, ulSize);
			fOk = false;
		}
		else
		{
			*ppcBUFFER_OUT = pcBuffer;
			*psizBUFFER_OUT = sizBuffer;

			lBytesProcessed = 0;
			do
			{
				sizChunk = ulSize;
				if( sizChunk>MONITOR_MAX_PACKET_SIZE-6 )
				{
					sizChunk = MONITOR_MAX_PACKET_SIZE-6;
				}

				aucCommand[0] = MONITOR_COMMAND_Read | (MONITOR_ACCESSSIZE_Byte<<6);
				aucCommand[1] = (unsigned char)sizChunk;
				aucCommand[2] =  ulNetxAddress      & 0xffU;
				aucCommand[3] = (ulNetxAddress>>8 ) & 0xffU;
				aucCommand[4] = (ulNetxAddress>>16) & 0xffU;
				aucCommand[5] = (ulNetxAddress>>24) & 0xffU;

				iResult = execute_command(aucCommand, 6);
				if( iResult!=0 )
				{
					MUHKUH_PLUGIN_PUSH_ERROR(tLuaFn.L, "%s(%p): failed to execute command!", m_pcName, this);
					fOk = false;
					break;
				}
				else
				{
					if( m_sizPacketInputBuffer!=4+sizChunk+1 )
					{
						MUHKUH_PLUGIN_PUSH_ERROR(tLuaFn.L, "%s(%p): answer to read_image has wrong packet size of %d!", m_pcName, this, m_sizPacketInputBuffer);
						fOk = false;
						break;
					}
					else
					{
						memcpy(pcBuffer, m_aucPacketInputBuffer+3, sizChunk);
						pcBuffer += sizChunk;
						ulSize -= sizChunk;
						ulNetxAddress += sizChunk;
						lBytesProcessed += sizChunk;

						fIsRunning = callback_long(&tLuaFn, lBytesProcessed, lCallbackUserData);
						if( fIsRunning!=true )
						{
							break;
						}
					}
				}
			} while( ulSize!=0 );
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(tLuaFn.L);
	}
}


void romloader_uart::write_data08(lua_State *ptClientData, unsigned long ulNetxAddress, unsigned char ucData)
{
	unsigned char aucCommand[7];
	int iResult;
	unsigned long ulValue;
	bool fOk;


	/* Expect error. */
	fOk = false;

	ulValue = 0;

	aucCommand[0] = MONITOR_COMMAND_Write | (MONITOR_ACCESSSIZE_Byte<<6);
	aucCommand[1] = 1;
	aucCommand[2] =  ulNetxAddress      & 0xffU;
	aucCommand[3] = (ulNetxAddress>>8 ) & 0xffU;
	aucCommand[4] = (ulNetxAddress>>16) & 0xffU;
	aucCommand[5] = (ulNetxAddress>>24) & 0xffU;
	aucCommand[6] = ucData;
	iResult = execute_command(aucCommand, 7);
	if( iResult!=0 )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to execute command!", m_pcName, this);
	}
	else
	{
		if( m_sizPacketInputBuffer!=4+1 )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): answer to write_data08 has wrong packet size of %d!", m_pcName, this, m_sizPacketInputBuffer);
		}
		else
		{
			fOk = true;
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}
}


void romloader_uart::write_data16(lua_State *ptClientData, unsigned long ulNetxAddress, unsigned short usData)
{
	unsigned char aucCommand[8];
	int iResult;
	unsigned long ulValue;
	bool fOk;


	/* Expect error. */
	fOk = false;

	ulValue = 0;

	aucCommand[0] = MONITOR_COMMAND_Write | (MONITOR_ACCESSSIZE_Word<<6);
	aucCommand[1] = 2;
	aucCommand[2] =  ulNetxAddress      & 0xffU;
	aucCommand[3] = (ulNetxAddress>>8 ) & 0xffU;
	aucCommand[4] = (ulNetxAddress>>16) & 0xffU;
	aucCommand[5] = (ulNetxAddress>>24) & 0xffU;
	aucCommand[6] =  usData    & 0xffU;
	aucCommand[7] = (usData>>8)& 0xffU;
	iResult = execute_command(aucCommand, 8);
	if( iResult!=0 )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to execute command!", m_pcName, this);
	}
	else
	{
		if( m_sizPacketInputBuffer!=4+1 )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): answer to write_data16 has wrong packet size of %d!", m_pcName, this, m_sizPacketInputBuffer);
		}
		else
		{
			fOk = true;
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}
}


void romloader_uart::write_data32(lua_State *ptClientData, unsigned long ulNetxAddress, unsigned long ulData)
{
	unsigned char aucCommand[10];
	int iResult;
	unsigned long ulValue;
	bool fOk;


	/* Expect error. */
	fOk = false;

	ulValue = 0;

	aucCommand[0] = MONITOR_COMMAND_Write | (MONITOR_ACCESSSIZE_Word<<6);
	aucCommand[1] = 4;
	aucCommand[2] =  ulNetxAddress      & 0xffU;
	aucCommand[3] = (ulNetxAddress>>8 ) & 0xffU;
	aucCommand[4] = (ulNetxAddress>>16) & 0xffU;
	aucCommand[5] = (ulNetxAddress>>24) & 0xffU;
	aucCommand[6] =  ulData     & 0xffU;
	aucCommand[7] = (ulData>> 8)& 0xffU;
	aucCommand[8] = (ulData>>16)& 0xffU;
	aucCommand[9] = (ulData>>24)& 0xffU;
	iResult = execute_command(aucCommand, 10);
	if( iResult!=0 )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to execute command!", m_pcName, this);
	}
	else
	{
		if( m_sizPacketInputBuffer!=4+1 )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): answer to write_data32 has wrong packet size of %d!", m_pcName, this, m_sizPacketInputBuffer);
		}
		else
		{
			fOk = true;
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}
}


void romloader_uart::write_image(unsigned long ulNetxAddress, const char *pcBUFFER_IN, size_t sizBUFFER_IN, SWIGLUA_REF tLuaFn, long lCallbackUserData)
{
	bool fOk;
	size_t sizChunk;
	int iResult;
	unsigned char aucCommand[MONITOR_MAX_PACKET_SIZE];
	size_t sizOutBuf;
	size_t sizInBuf;
	unsigned char ucCommand;
	unsigned char ucStatus;
	bool fIsRunning;
	long lBytesProcessed;


	/* Be optimistic. */
	fOk = true;

	if( m_fIsConnected==false )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(tLuaFn.L, "%s(%p): not connected!", m_pcName, this);
		fOk = false;
	}
	else if( sizBUFFER_IN!=0 )
	{
		lBytesProcessed = 0;
		do
		{
			sizChunk = sizBUFFER_IN;
			if( sizChunk>MONITOR_MAX_PACKET_SIZE-11 )
			{
				sizChunk = MONITOR_MAX_PACKET_SIZE-11;
			}

			aucCommand[0] = MONITOR_COMMAND_Write | (MONITOR_ACCESSSIZE_Byte<<6);
			aucCommand[1] = sizChunk;
			aucCommand[2] =  ulNetxAddress      & 0xffU;
			aucCommand[3] = (ulNetxAddress>>8 ) & 0xffU;
			aucCommand[4] = (ulNetxAddress>>16) & 0xffU;
			aucCommand[5] = (ulNetxAddress>>24) & 0xffU;
			memcpy(aucCommand+6, pcBUFFER_IN, sizChunk);
			iResult = execute_command(aucCommand, sizChunk+6);
			if( iResult!=0 )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(tLuaFn.L, "%s(%p): failed to execute command!", m_pcName, this);
				fOk = false;
				break;
			}
			else
			{
				if( m_sizPacketInputBuffer!=4+1 )
				{
					MUHKUH_PLUGIN_PUSH_ERROR(tLuaFn.L, "%s(%p): answer to write_data08 has wrong packet size of %d!", m_pcName, this, m_sizPacketInputBuffer);
					fOk = false;
					break;
				}
				else
				{
					pcBUFFER_IN += sizChunk;
					sizBUFFER_IN -= sizChunk;
					ulNetxAddress += sizChunk;
					lBytesProcessed += sizChunk;

					fIsRunning = callback_long(&tLuaFn, lBytesProcessed, lCallbackUserData);
					if( fIsRunning!=true )
					{
						break;
					}
				}
			}
		} while( sizBUFFER_IN!=0 );
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(tLuaFn.L);
	}
}


void romloader_uart::call(unsigned long ulNetxAddress, unsigned long ulParameterR0, SWIGLUA_REF tLuaFn, long lCallbackUserData)
{
	fprintf(stderr, "call: 0x%08x, 0x%08x\n", ulNetxAddress, ulParameterR0);
}


/*-------------------------------------*/


romloader_uart_reference::romloader_uart_reference(void)
 : muhkuh_plugin_reference()
{
}


romloader_uart_reference::romloader_uart_reference(const char *pcName, const char *pcTyp, bool fIsUsed, romloader_uart_provider *ptProvider)
 : muhkuh_plugin_reference(pcName, pcTyp, fIsUsed, ptProvider)
{
}


romloader_uart_reference::romloader_uart_reference(const romloader_uart_reference *ptCloneMe)
 : muhkuh_plugin_reference(ptCloneMe)
{
}


/*-------------------------------------*/
