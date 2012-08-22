/***************************************************************************
 *   Copyright (C) 2011 by Christoph Thelen                                *
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


#include "transport.h"

#include <stddef.h>

#include "monitor.h"
#include "monitor_commands.h"
#include "serial_vectors.h"
#include "systime.h"



#define UART_BUFFER_NO_TIMEOUT 0
#define UART_BUFFER_TIMEOUT 1

#define MONITOR_MAX_PACKET_SIZE_UART 2048


unsigned char aucStreamBuffer[MONITOR_MAX_PACKET_SIZE_UART];
size_t sizStreamBufferHead;
size_t sizStreamBufferFill;

unsigned char aucPacketInputBuffer[MONITOR_MAX_PACKET_SIZE_UART];

unsigned char aucPacketOutputBuffer[MONITOR_MAX_PACKET_SIZE_UART];
size_t sizPacketOutputFill;
size_t sizPacketOutputFillLast;


/* This is a very nice routine for the CITT XModem CRC from
 * http://www.eagleairaust.com.au/code/crc16.htm.
 */
static unsigned short crc16(unsigned short usCrc, unsigned char ucData)
{
	unsigned int uiCrc;


	uiCrc  = (usCrc >> 8U) | ((usCrc & 0xffU) << 8U);
	uiCrc ^= ucData;
	uiCrc ^= (uiCrc & 0xffU) >> 4U;
	uiCrc ^= (uiCrc & 0x0fU) << 12U;
	uiCrc ^= ((uiCrc & 0xffU) << 4U) << 1U;

	return (unsigned short)uiCrc;
}


void transport_init(void)
{
	monitor_init();

	/* Initialize the stream buffer. */
	sizStreamBufferHead = 0;
	sizStreamBufferFill = 0;

	sizPacketOutputFill = 0;
	sizPacketOutputFillLast = 0;
}



static int uart_buffer_fill(size_t sizRequestedFillLevel, unsigned int uiTimeoutFlag)
{
	size_t sizWritePosition;
	int iResult;
	unsigned long ulTimeout;
	HOSTDEF(ptUsbDevFifoArea);
	HOSTDEF(ptUsbDevFifoCtrlArea);
	unsigned long ulFillLevel;
	size_t sizLeft;


//	uprintf("uart_buffer_fill: sizRequestedFillLevel=%d, uiTimeoutFlag=%d\n", sizRequestedFillLevel, uiTimeoutFlag);

//	uprintf("sizStreamBufferHead = %d\n", sizStreamBufferHead);
//	uprintf("sizStreamBufferFill = %d\n", sizStreamBufferFill);

	/* Get the write position. */
	sizWritePosition = sizStreamBufferHead + sizStreamBufferFill;
	if( sizWritePosition>=MONITOR_MAX_PACKET_SIZE_UART )
	{
		sizWritePosition -= MONITOR_MAX_PACKET_SIZE_UART;
	}

	iResult = 0;

	if( uiTimeoutFlag==UART_BUFFER_NO_TIMEOUT )
	{
		/* Fill-up the buffer to the requested level. */
		while( sizStreamBufferFill<sizRequestedFillLevel )
		{
			/* Get the current fill level. */
			ulFillLevel   = ptUsbDevFifoCtrlArea->ulUsb_dev_fifo_ctrl_uart_ep_rx_stat;
			ulFillLevel  &= HOSTMSK(usb_dev_fifo_ctrl_uart_ep_rx_stat_fill_level);
			ulFillLevel >>= HOSTSRT(usb_dev_fifo_ctrl_uart_ep_rx_stat_fill_level);
			if( ulFillLevel!=0 )
			{
				/* Limit the fill level to the number of requested bytes. */
				sizLeft = sizRequestedFillLevel - sizStreamBufferFill;
				if( ulFillLevel>sizLeft )
				{
					ulFillLevel = sizLeft;
				}
				/* Grab all bytes. */
				do
				{
					/* Write the new byte to the buffer. */
					aucStreamBuffer[sizWritePosition] = (unsigned char)(ptUsbDevFifoArea->ulUsb_dev_uart_rx_data);
					/* Increase the write position. */
					++sizWritePosition;
					if( sizWritePosition>=MONITOR_MAX_PACKET_SIZE_UART )
					{
						sizWritePosition -= MONITOR_MAX_PACKET_SIZE_UART;
					}
					/* Increase the fill level. */
					++sizStreamBufferFill;
					
					--ulFillLevel;
				} while( ulFillLevel!=0 );
			}
		}
	}
	else
	{
		/* Fill-up the buffer to the requested level. */
		ulTimeout = systime_get_ms();
		while( sizStreamBufferFill<sizRequestedFillLevel )
		{
			/* Wait for new data. */
			if( systime_elapsed(ulTimeout, 500)!=0 )
			{
				iResult = -1;
				break;
			}

			/* Get the current fill level. */
			ulFillLevel   = ptUsbDevFifoCtrlArea->ulUsb_dev_fifo_ctrl_uart_ep_rx_stat;
			ulFillLevel  &= HOSTMSK(usb_dev_fifo_ctrl_uart_ep_rx_stat_fill_level);
			ulFillLevel >>= HOSTSRT(usb_dev_fifo_ctrl_uart_ep_rx_stat_fill_level);
			if( ulFillLevel!=0 )
			{
				/* Limit the fill level to the number of requested bytes. */
				sizLeft = sizRequestedFillLevel - sizStreamBufferFill;
				if( ulFillLevel>sizLeft )
				{
					ulFillLevel = sizLeft;
				}
				/* Grab all bytes. */
				do
				{
					/* Write the new byte to the buffer. */
					aucStreamBuffer[sizWritePosition] = (unsigned char)(ptUsbDevFifoArea->ulUsb_dev_uart_rx_data);
					/* Increase the write position. */
					++sizWritePosition;
					if( sizWritePosition>=MONITOR_MAX_PACKET_SIZE_UART )
					{
						sizWritePosition -= MONITOR_MAX_PACKET_SIZE_UART;
					}
					/* Increase the fill level. */
					++sizStreamBufferFill;
					
					--ulFillLevel;
				} while( ulFillLevel!=0 );
				
				ulTimeout = systime_get_ms();
			}
		}
	}

	return iResult;
}


static unsigned char uart_buffer_get(void)
{
	unsigned char ucByte;


	ucByte = aucStreamBuffer[sizStreamBufferHead];

//	uprintf("Get from %d = 0x%02x\n", sizStreamBufferHead, ucByte);

	++sizStreamBufferHead;
	if( sizStreamBufferHead>=MONITOR_MAX_PACKET_SIZE_UART )
	{
		sizStreamBufferHead -= MONITOR_MAX_PACKET_SIZE_UART;
	}

	--sizStreamBufferFill;

	return ucByte;
}


static unsigned char uart_buffer_peek(size_t sizOffset)
{
	size_t sizReadPosition;
	unsigned char ucByte;


	sizReadPosition = sizStreamBufferHead + sizOffset;
	if( sizReadPosition>=MONITOR_MAX_PACKET_SIZE_UART )
	{
		sizReadPosition -= MONITOR_MAX_PACKET_SIZE_UART;
	}

	ucByte = aucStreamBuffer[sizReadPosition];
//	uprintf("peek at offset %d = 0x%02x\n", sizOffset, ucByte);

	return ucByte;
}


static void uart_buffer_skip(size_t sizSkip)
{
	sizStreamBufferHead += sizSkip;
	if( sizStreamBufferHead>=MONITOR_MAX_PACKET_SIZE_UART )
	{
		sizStreamBufferHead -= MONITOR_MAX_PACKET_SIZE_UART;
	}

	sizStreamBufferFill -= sizSkip;
}


void transport_loop(void)
{
	unsigned char ucByte;
	size_t sizPacket;
	unsigned short usCrc16;
	size_t sizCrcPosition;
	int iResult;
	unsigned char *pucBuffer;


	/* Collect a complete packet. */

	/* Wait for the start character. */
	do
	{
		uart_buffer_fill(1, UART_BUFFER_NO_TIMEOUT);
		ucByte = uart_buffer_get();
	} while( ucByte!=MONITOR_STREAM_PACKET_START );

//	uprintf("Startchar\n");

	/* Get the size of the data packet in bytes. */
	uart_buffer_fill(2, UART_BUFFER_NO_TIMEOUT);
	sizPacket  = uart_buffer_peek(0);
	sizPacket |= (size_t)(uart_buffer_peek(1) << 8U);

//	uprintf("Size: 0x%08x\n", sizPacket);

	/* Is the packet's size valid? */
	if( sizPacket==0 )
	{
		/* Get the magic "*#". This is the knock sequence of the old ROM code. */
		iResult = uart_buffer_fill(4, UART_BUFFER_TIMEOUT);
		if( iResult==0 && uart_buffer_peek(2)=='*' && uart_buffer_peek(3)=='#' )
		{
			/* Discard the size and magic. */
			for(sizCrcPosition=0; sizCrcPosition<4; ++sizCrcPosition)
			{
				uart_buffer_get();
			}

			/* Send magic cookie and version info. */
			monitor_send_magic(MONITOR_MAX_PACKET_SIZE_UART);
		}
	}
	else if( sizPacket<=MONITOR_MAX_PACKET_SIZE_UART-4 )
	{
		/* Yes, the packet size is valid. */

		/* Get the size, data and CRC16. */
		iResult = uart_buffer_fill(sizPacket+4, UART_BUFFER_TIMEOUT);
		if( iResult==0 )
		{
			/* Loop over all bytes and build the CRC16 checksum. */
			/* NOTE: the size is just for the user data, but the CRC16 includes the size. */
			usCrc16 = crc16(0, uart_buffer_peek(0));
			usCrc16 = crc16(usCrc16, uart_buffer_peek(1));
			pucBuffer = aucPacketInputBuffer;
			sizCrcPosition = 2;
			while( sizCrcPosition<sizPacket+4 )
			{
				ucByte = uart_buffer_peek(sizCrcPosition);
				*(pucBuffer++) = ucByte;
				usCrc16 = crc16(usCrc16, ucByte);
				++sizCrcPosition;
			}

			if( usCrc16==0 )
			{
				/* OK, the CRC matches! */

				/* TODO: process the packet. */

				/* Skip over the complete packet. It is already copied. */
				uart_buffer_skip(sizPacket+4);

//				uprintf("Received packet (%d bytes): ", sizPacket);


				monitor_process_packet(aucPacketInputBuffer, sizPacket, MONITOR_MAX_PACKET_SIZE_UART-5U);
			}
/*
			else
			{
				uprintf("crc failed: %04x\n", usCrc16);
			}
*/
		}
/*
		else
		{
			uprintf("Failed to fill buffer: %d\n", iResult);
		}
*/
	}
/*
	else
	{
		uprintf("size %d exceeds maximum %d\n", sizPacket, MONITOR_MAX_PACKET_SIZE-4);
	}
*/
}



void transport_send_byte(unsigned char ucData)
{
	if( sizPacketOutputFill<MONITOR_MAX_PACKET_SIZE_UART )
	{
		aucPacketOutputBuffer[sizPacketOutputFill] = ucData;
		++sizPacketOutputFill;
	}
/*
	else
	{
		uprintf("discarding byte 0x%02x\n", ucData);
	}
*/
}



void transport_send_packet(void)
{
	HOSTDEF(ptUsbDevFifoArea);
	HOSTDEF(ptUsbDevFifoCtrlArea);
	unsigned long ulFillLevel;
	const unsigned char *pucCnt;
	unsigned char ucData;
	unsigned short usCrc;
	unsigned long ulChunk;
	size_t sizDataLeft;


	/* Wait until 3 bytes fit into the FIFO. */
	do
	{
		ulFillLevel   = ptUsbDevFifoCtrlArea->ulUsb_dev_fifo_ctrl_uart_ep_tx_stat;
		ulFillLevel  &= HOSTMSK(usb_dev_fifo_ctrl_uart_ep_tx_stat_fill_level);
		ulFillLevel >>= HOSTSRT(usb_dev_fifo_ctrl_uart_ep_tx_stat_fill_level);
	} while( ulFillLevel>=(64-3) );

	/* Send the start character. */
	ptUsbDevFifoArea->ulUsb_dev_uart_tx_data = MONITOR_STREAM_PACKET_START;

	/* Send the size. */
	ucData = (unsigned char)( sizPacketOutputFill        & 0xffU);
	ptUsbDevFifoArea->ulUsb_dev_uart_tx_data = (unsigned long)ucData;
	usCrc = crc16(0, ucData);
	ucData = (unsigned char)((sizPacketOutputFill >> 8U) & 0xffU);
	ptUsbDevFifoArea->ulUsb_dev_uart_tx_data = (unsigned long)ucData;
	usCrc = crc16(usCrc, ucData);


	/* Send the packet and build the CRC16. */
	pucCnt = aucPacketOutputBuffer;
	sizDataLeft = sizPacketOutputFill;
	while( sizDataLeft!=0 )
	{
		ulFillLevel   = ptUsbDevFifoCtrlArea->ulUsb_dev_fifo_ctrl_uart_ep_tx_stat;
		ulFillLevel  &= HOSTMSK(usb_dev_fifo_ctrl_uart_ep_tx_stat_fill_level);
		ulFillLevel >>= HOSTSRT(usb_dev_fifo_ctrl_uart_ep_tx_stat_fill_level);
		if( ulFillLevel<64 )
		{
			ulChunk = 64 - ulFillLevel;
			if( ulChunk>sizDataLeft )
			{
				ulChunk = sizDataLeft;
			}
			sizDataLeft -= ulChunk;
			do
			{
				ucData = *(pucCnt++);
				ptUsbDevFifoArea->ulUsb_dev_uart_tx_data = (unsigned long)ucData;
				usCrc = crc16(usCrc, ucData);
				--ulChunk;
			} while( ulChunk!=0 );
		}
	}

	/* Wait until 2 bytes fit into the FIFO. */
	do
	{
		ulFillLevel   = ptUsbDevFifoCtrlArea->ulUsb_dev_fifo_ctrl_uart_ep_tx_stat;
		ulFillLevel  &= HOSTMSK(usb_dev_fifo_ctrl_uart_ep_tx_stat_fill_level);
		ulFillLevel >>= HOSTSRT(usb_dev_fifo_ctrl_uart_ep_tx_stat_fill_level);
	} while( ulFillLevel>=(64-2) );

	/* Send the CRC16. */
	ucData = (unsigned char)(usCrc>>8U);
	ptUsbDevFifoArea->ulUsb_dev_uart_tx_data = (unsigned long)ucData;
	ucData = (unsigned char)(usCrc&0xffU);
	ptUsbDevFifoArea->ulUsb_dev_uart_tx_data = (unsigned long)ucData;

	/* Flush the buffer. */
//	SERIAL_V1_FLUSH();

	/* Remember the packet size for resends. */
	sizPacketOutputFillLast = sizPacketOutputFill;

	sizPacketOutputFill = 0;
}



void transport_resend_packet(void)
{
	/* Restore the last packet size. */
	sizPacketOutputFill = sizPacketOutputFillLast;

	/* Send the buffer again. */
	transport_send_packet();
}



unsigned char transport_call_console_get(void)
{
	return 0;
}



void transport_call_console_put(unsigned int uiChar)
{
	/* Add the byte to the FIFO. */
	transport_send_byte((unsigned char)uiChar);

	/* Reached the maximum packet size? */
	if( sizPacketOutputFill>=MONITOR_MAX_PACKET_SIZE_UART-5 )
	{
		/* Yes -> send the packet. */
		transport_send_packet();

		/* Start a new packet. */
		transport_send_byte(MONITOR_STATUS_CallMessage);
	}

}



unsigned int transport_call_console_peek(void)
{
	return 0;
}



void transport_call_console_flush(void)
{
	/* Send the packet. */
	transport_send_packet();

	/* Start a new packet. */
	transport_send_byte(MONITOR_STATUS_CallMessage);
}

