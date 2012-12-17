/***************************************************************************
 *   Copyright (C) 2012 by Christoph Thelen                                *
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


#include "romloader_uart_read_functinoid_aboot.h"


/* Load- and entry points for the bootstrap firmware. */
#include "netx/targets/uartmon_netx50_bootstrap_run.h"
#include "netx/targets/uartmon_netx500_bootstrap_run.h"

/* Data of the bootstrap firmware. */
#include "netx/targets/uartmon_netx50_bootstrap.h"
#include "netx/targets/uartmon_netx500_bootstrap.h"

/* Data of the monitor firmware. */
#include "netx/targets/uartmon_netx50_monitor.h"
#include "netx/targets/uartmon_netx500_monitor.h"


#ifdef _MSC_VER
#       define snprintf _snprintf
#endif


romloader_uart_read_functinoid_aboot::romloader_uart_read_functinoid_aboot(romloader_uart_device *ptDevice, char *pcPortName)
{
	m_ptDevice = ptDevice;
	m_pcPortName = 	pcPortName;
}


unsigned long romloader_uart_read_functinoid_aboot::read_data32(unsigned long ulAddress)
{
	bool fOk;
	unsigned long ulValue;


	fOk = legacy_read_v1(ulAddress, &ulValue);
	if( fOk!=true )
	{
		ulValue = 0;
	}
	return ulValue;
}


bool romloader_uart_read_functinoid_aboot::legacy_read_v1(unsigned long ulAddress, unsigned long *pulValue)
{
	union
	{
		unsigned char auc[32];
		char ac[32];
	} uCmd;
	union
	{
		unsigned char *puc;
		char *pc;
	} uResponse;
	size_t sizCmd;
	bool fOk;
	int iResult;
	unsigned long ulReadbackAddress;
	unsigned long ulValue;


	sizCmd = snprintf(uCmd.ac, 32, "DUMP %lx\n", ulAddress);
	/* Send knock sequence with 500ms second timeout. */
	if( m_ptDevice->SendRaw(uCmd.auc, sizCmd, 500)!=sizCmd )
	{
		/* Failed to send the command to the device. */
		fprintf(stderr, "Failed to send the command to the device.\n");
		fOk = false;
	}
	else
	{
		/* Receive one line. This is the command echo. */
		fOk = m_ptDevice->GetLine(&uResponse.puc, "\r\n", 2000);
		if( fOk==false )
		{
			fprintf(stderr, "failed to get command response!\n");
		}
		else
		{
			sizCmd = strlen(uResponse.pc);
			hexdump(uResponse.puc, sizCmd);
			free(uResponse.puc);

			/* Receive one line. This is the command result. */
			fOk = m_ptDevice->GetLine(&uResponse.puc, "\r\n>", 2000);
			if( fOk==false )
			{
				fprintf(stderr, "failed to get command response!\n");
			}
			else
			{
				iResult = sscanf(uResponse.pc, "%08lx: %08lx", &ulReadbackAddress, &ulValue);
				if( iResult==2 && ulAddress==ulReadbackAddress )
				{
					if( pulValue!=NULL )
					{
						*pulValue = ulValue;
					}
				}
				else
				{
					fprintf(stderr, "The command response is invalid!\n");
					fOk = false;
				}
				sizCmd = strlen(uResponse.pc);
				hexdump(uResponse.puc, sizCmd);
				free(uResponse.puc);
			}
		}
	}

	return fOk;
}



void romloader_uart_read_functinoid_aboot::hexdump(const unsigned char *pucData, unsigned long ulSize)
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



bool romloader_uart_read_functinoid_aboot::netx50_load_code(const unsigned char *pucNetxCode, size_t sizNetxCode)
{
	size_t sizLine;
	unsigned long ulLoadAddress;
	union
	{
		unsigned char auc[64];
		char ac[64];
	} uBuffer;
	union
	{
		unsigned char *puc;
		char *pc;
	} uResponse;
	unsigned int uiTimeoutMs;
	bool fOk;
	uuencoder tUuencoder;
	UUENCODER_PROGRESS_INFO_T tProgressInfo;


	/* Be optimistic. */
	fOk = true;

	uiTimeoutMs = 100;

	/* Construct the command. */
	sizLine = snprintf(uBuffer.ac, sizeof(uBuffer), "luue %lx\n", BOOTSTRAP_DATA_START_NETX50);
	if( m_ptDevice->SendRaw(uBuffer.auc, sizLine, 500)!=sizLine )
	{
		fprintf(stderr, "%s(%p): Failed to send command!\n", m_pcPortName, this);
		fOk = false;
	}
	else if( m_ptDevice->GetLine(&uResponse.puc, "\r\n", 500)!=true )
	{
		fprintf(stderr, "%s(%p): Failed to get command echo!\n", m_pcPortName, this);
		fOk = false;
	}
	else
	{
		free(uResponse.puc);

		printf("Uploading firmware...\n");
		tUuencoder.set_data(pucNetxCode, sizNetxCode);

		/* Send the data line by line with a delay of 10ms. */
		do
		{
			sizLine = tUuencoder.process(uBuffer.ac, sizeof(uBuffer));
			if( sizLine!=0 )
			{
				uiTimeoutMs = 100;
				tUuencoder.get_progress_info(&tProgressInfo);
				printf("%05d/%05d (%d%%)\n", tProgressInfo.sizProcessed, tProgressInfo.sizTotal, tProgressInfo.uiPercent);
				if( m_ptDevice->SendRaw(uBuffer.auc, sizLine, 500)!=sizLine )
				{
					fprintf(stderr, "%s(%p): Failed to send uue data!\n", m_pcPortName, this);
					fOk = false;
					break;
				}
				else if( m_ptDevice->GetLine(&uResponse.puc, "\r\n", 500)!=true )
				{
					fprintf(stderr, "%s(%p): Failed to get response line!\n", m_pcPortName, this);
					fOk = false;
					break;
				}
				free(uResponse.puc);

				// FIXME: The delay is not necessary for a USB connection. Detect USB/UART and enable the delay for UART.
				//SLEEP_MS(10);
			}
		} while( tUuencoder.isFinished()==false );

		if( fOk==true )
		{
			fOk = m_ptDevice->GetLine(&uResponse.puc, "\r\n>", 2000);
			if( fOk==true )
			{
				free(uResponse.puc);
			}
			else
			{
				fprintf(stderr, "Failed to get response.\n");
			}
		}
		else
		{
			fprintf(stderr, "%s(%p): Failed to upload the firmware!\n", m_pcPortName, this);
		}
	}

	return fOk;
}


bool romloader_uart_read_functinoid_aboot::netx500_load_code(const unsigned char *pucNetxCode, size_t sizNetxCode)
{
	unsigned short usCrc;
	size_t sizCnt;
	size_t sizLine;
	union
	{
		unsigned char auc[64];
		char ac[64];
	} uBuffer;
	union
	{
		unsigned char *puc;
		char *pc;
	} uResponse;
	unsigned int uiTimeoutMs;
	bool fOk;


	/* Be optimistic. */
	fOk = true;

	uiTimeoutMs = 100;

	/* Get the CRC16 checksum for the bootstrap code. */
	usCrc = 0xffff;
	for(sizCnt=0; sizCnt<sizNetxCode; sizCnt++)
	{
		usCrc = crc16(usCrc, pucNetxCode[sizCnt]);
	}

	/* Construct the command. */
	sizLine = snprintf(uBuffer.ac, sizeof(uBuffer), "LOAD %lx %x %x 10000\n", BOOTSTRAP_DATA_START_NETX500, sizNetxCode, usCrc);
	printf("Load command:\n");
	hexdump(uBuffer.auc, sizLine);
	if( m_ptDevice->SendRaw(uBuffer.auc, sizLine, 500)!=sizLine )
	{
		fprintf(stderr, "%s(%p): Failed to send command!\n", m_pcPortName, this);
		fOk = false;
	}
	else if( m_ptDevice->GetLine(&uResponse.puc, "\r\n", 500)!=true )
	{
		fprintf(stderr, "%s(%p): Failed to get command echo!\n", m_pcPortName, this);
		fOk = false;
	}
	else
	{
		printf("response: '%s'\n", uResponse.pc);
		free(uResponse.puc);

		printf("Uploading firmware...\n");
		if( m_ptDevice->SendRaw(pucNetxCode, sizNetxCode, 500)!=sizNetxCode )
		{
			fprintf(stderr, "%s(%p): Failed to upload the firmware!\n", m_pcPortName, this);
			fOk = false;
		}
		else
		{
			fOk = m_ptDevice->GetLine(&uResponse.puc, "\r\n>", 2000);
			if( fOk==true )
			{
				printf("response: '%s'\n", uResponse.pc);
				free(uResponse.puc);
			}
			else
			{
				fprintf(stderr, "Failed to get response.\n");
			}
		}
	}

	return fOk;
}



bool romloader_uart_read_functinoid_aboot::netx50_start_code(void)
{
	union
	{
		unsigned char auc[64];
		char ac[64];
	} uBuffer;
	union
	{
		unsigned char *puc;
		char *pc;
	} uResponse;
	size_t sizLine;
	bool fOk;
	unsigned long ulExecAddress;


	/* Construct the command. */
	sizLine = sprintf(uBuffer.ac, "call %lx\n", BOOTSTRAP_EXEC_NETX50);

	if( m_ptDevice->SendRaw(uBuffer.auc, sizLine, 500)!=sizLine )
	{
		fprintf(stderr, "%s(%p): Failed to send command!\n", m_pcPortName, this);
		fOk = false;
	}
	else if( m_ptDevice->GetLine(&uResponse.puc, "\r\n", 2000)!=true )
	{
		fprintf(stderr, "%s(%p): Failed to get command echo!\n", m_pcPortName, this);
		fOk = false;
	}
	else
	{
		free(uResponse.puc);
		fOk = true;
	}

	return fOk;
}


bool romloader_uart_read_functinoid_aboot::netx500_start_code(void)
{
	union
	{
		unsigned char auc[64];
		char ac[64];
	} uBuffer;
	union
	{
		unsigned char *puc;
		char *pc;
	} uResponse;
	size_t sizLine;
	bool fOk;
	unsigned long ulExecAddress;


	/* Construct the command. */
	sizLine = sprintf(uBuffer.ac, "CALL %lx\n", BOOTSTRAP_EXEC_NETX500);
	printf("Load command: '%s'\n", uBuffer.ac);

	if( m_ptDevice->SendRaw(uBuffer.auc, sizLine, 500)!=sizLine )
	{
		fprintf(stderr, "%s(%p): Failed to send command!\n", m_pcPortName, this);
		fOk = false;
	}
	else if( m_ptDevice->GetLine(&uResponse.puc, "\r\n", 2000)!=true )
	{
		fprintf(stderr, "%s(%p): Failed to get command echo!\n", m_pcPortName, this);
		fOk = false;
	}
	else
	{
		printf("Response: '%s'\n", uResponse.pc);
		free(uResponse.puc);
		fOk = true;
	}

	return fOk;
}



int romloader_uart_read_functinoid_aboot::update_device(ROMLOADER_CHIPTYP tChiptyp)
{
	bool fOk;
	int iResult;


	fprintf(stderr, "update device.\n");

	/* Expect failure. */
	iResult = -1;

	switch(tChiptyp)
	{
	case ROMLOADER_CHIPTYP_NETX50:
		fprintf(stderr, "update netx50.\n");

		fOk = netx50_load_code(auc_uartmon_netx50_bootstrap, sizeof(auc_uartmon_netx50_bootstrap));
		if( fOk==true )
		{
			fOk = netx50_start_code();
			if( fOk==true )
			{
				if( m_ptDevice->SendRaw(auc_uartmon_netx50_monitor, sizeof(auc_uartmon_netx50_monitor), 1000)!=sizeof(auc_uartmon_netx50_monitor) )
				{
					fprintf(stderr, "%s(%p): Failed to send command!\n", m_pcPortName, this);
				}
				else
				{
					/* The ROM code is now HBOOT_SOFT */
					iResult = 0;
				}
			}
		}
		break;


	case ROMLOADER_CHIPTYP_NETX500:
	case ROMLOADER_CHIPTYP_NETX100:
		fprintf(stderr, "update netx500.\n");

		fOk = netx500_load_code(auc_uartmon_netx500_bootstrap, sizeof(auc_uartmon_netx500_bootstrap));
		{
			fOk = netx500_start_code();
			if( fOk==true )
			{
				if( m_ptDevice->SendRaw(auc_uartmon_netx500_monitor, sizeof(auc_uartmon_netx500_monitor), 500)!=sizeof(auc_uartmon_netx500_monitor) )
				{
					fprintf(stderr, "%s(%p): Failed to send command!\n", m_pcPortName, this);
				}
				else
				{
					/* The ROM code is now HBOOT_SOFT */
					iResult = 0;
				}
			}
		}
		break;


	case ROMLOADER_CHIPTYP_NETX10:
	default:
		/* No idea how to update this one! */
		fprintf(stderr, "%s(%p): No strategy to update chip type %d!\n", m_pcPortName, this, tChiptyp);
		break;
	}
	
	return iResult;
}
