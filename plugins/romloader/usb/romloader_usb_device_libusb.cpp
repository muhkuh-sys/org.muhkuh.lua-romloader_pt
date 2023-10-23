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


#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#include "romloader_usb_device_libusb.h"
#include "../machine_interface/netx/src/monitor_commands.h"

#include <stdlib.h>

#if defined(_MSC_VER)
#	define snprintf _snprintf
#	define SLEEP_MS(ms) Sleep(ms)
#else
#	include <sys/time.h>
#	include <unistd.h>
#	define SLEEP_MS(ms) usleep(ms*1000)
#endif

#include "romloader_usb_main.h"
#include "../uuencoder.h"

#include "usbmon_netx10.h"
#include "usbmon_netx56.h"
#include "usbmon_netx500.h"


/*-------------------------------------*/


romloader_usb_device_libusb::romloader_usb_device_libusb(const char *pcPluginId)
 : romloader_usb_device(pcPluginId)
 , m_ptLibUsbContext(NULL)
 , m_ptDevHandle(NULL)
{
	memset(&m_tDeviceId, 0, sizeof(NETX_USB_DEVICE_T));

	libusb_init(&m_ptLibUsbContext);
	libusb_set_option(m_ptLibUsbContext, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);
}


romloader_usb_device_libusb::~romloader_usb_device_libusb(void)
{
	Disconnect();

	if( m_ptLibUsbContext!=NULL )
	{
		libusb_exit(m_ptLibUsbContext);
	}
}


const char *romloader_usb_device_libusb::m_pcPluginNamePattern = "romloader_usb_%02x_%02x";


int romloader_usb_device_libusb::libusb_reset_and_close_device(void)
{
	int iResult;


	if( m_ptDevHandle!=NULL )
	{
		iResult = libusb_reset_device(m_ptDevHandle);
		if( iResult==LIBUSB_SUCCESS || iResult==LIBUSB_ERROR_NOT_FOUND )
		{
			libusb_close(m_ptDevHandle);
			m_ptDevHandle = NULL;
			iResult = LIBUSB_SUCCESS;
		}
	}
	else
	{
		/* No open device found. */
		iResult = LIBUSB_ERROR_NOT_FOUND;
	}

	return iResult;
}



int romloader_usb_device_libusb::detect_interfaces(romloader_usb_reference ***ppptReferences, size_t *psizReferences, romloader_usb_provider *ptProvider)
{
	int iResult;
	ssize_t ssizDevList;
	libusb_device **ptDeviceList;
	libusb_device **ptDevCnt, **ptDevEnd;
	libusb_device *ptDev;
//	libusb_device_handle *ptDevHandle;
	unsigned int uiBusNr;
	unsigned int uiDevAdr;
	bool fDeviceIsBusy;
	bool fDeviceIsNetx;
	romloader_usb_reference *ptRef;
	const size_t sizMaxName = 32;
	char acName[sizMaxName];

	size_t sizRefCnt;
	size_t sizRefMax;
	romloader_usb_reference **pptRef;
	romloader_usb_reference **pptRefNew;
	const NETX_USB_DEVICE_T *ptId;

	const int iPathMax = 32;
	unsigned char aucPath[iPathMax];
	/* bus number as a digit plus path elements as single digits */
	/* This is the Location ID format used by USBView, but is this sufficient? */
	char acPathString[iPathMax * 2 + 2] = {0};

	int iPathStringPos;
	int iCnt;


	/* Expect success. */
	iResult = 0;

	/* Init References array. */
	sizRefCnt = 0;
	sizRefMax = 16;
	pptRef = (romloader_usb_reference**)malloc(sizRefMax*sizeof(romloader_usb_reference*));
	if( pptRef==NULL )
	{
		fprintf(stderr, "out of memory!\n");
		iResult = -1;
	}
	else
	{
		/* detect devices */
		ptDeviceList = NULL;
		ssizDevList = libusb_get_device_list(m_ptLibUsbContext, &ptDeviceList);
		if( ssizDevList<0 )
		{
			/* failed to detect devices */
			fprintf(stderr, "%s(%p): failed to detect usb devices: %ld:%s\n", m_pcPluginId, this, ssizDevList, libusb_strerror(ssizDevList));
			iResult = -1;
		}
		else
		{
			/* loop over all devices */
			ptDevCnt = ptDeviceList;
			ptDevEnd = ptDevCnt + ssizDevList;
			while( ptDevCnt<ptDevEnd )
			{
				ptDev = *ptDevCnt;
				ptId = identifyDevice(ptDev);
				if( ptId!=NULL )
				{
					/* construct the name */
					uiBusNr = libusb_get_bus_number(ptDev);
					uiDevAdr = libusb_get_device_address(ptDev);
					snprintf(acName, sizMaxName-1, m_pcPluginNamePattern, uiBusNr, uiDevAdr);

					/* Get the location. */
					iResult = libusb_get_port_numbers(ptDev, aucPath, iPathMax);
					if( iResult>0 )
					{
						/* Build the path string. */
						sprintf(acPathString, "%02x", uiBusNr);
						iPathStringPos = 2;
						for(iCnt=0; iCnt<iResult; ++iCnt)
						{
							sprintf(acPathString+iPathStringPos, "%02x", aucPath[iCnt]);
							iPathStringPos += 2;
						}
						acPathString[iPathStringPos] = 0;
						fprintf(stderr, "Path: %s\n", acPathString);
					}
					else
					{
						fprintf(stderr, "Failed to get the port numbers: %d\n", iResult);
					}
/* TODO: replace this with setup_device?
 * It is the same open/set_config/claim, except that here no error is printed if BUSY.
 */
					iResult = setup_netx_device(ptDev, ptId);
					if( iResult!=LIBUSB_SUCCESS && iResult!=LIBUSB_ERROR_BUSY /* && iResult!=LIBUSB_ERROR_SYS_BUSY */ )
					{
						/* Failed to open the interface, do not add it to the list. */
						fprintf(stderr, "%s(%p): failed to setup the device %s: %d:%s\n", m_pcPluginId, this, acName, iResult, libusb_strerror(iResult));
					}
					else
					{
						if( iResult!=LIBUSB_SUCCESS ) /* iResult == LIBUSB_ERROR_BUSY */
						{
							/* the interface is busy! */
							fDeviceIsBusy = true;
							iResult = LIBUSB_SUCCESS;
						}
						else /* iResult == LIBUSB_SUCCESS */
						{
							/* ok, claimed the interface -> the device is not busy */
							fDeviceIsBusy = false;
							/* release the interface */
							/* NOTE: The 'busy' information only represents the device state at detection time.
							 * This function _must_not_ keep the claim on the device or other applications will
							 * not be able to use it.
							 */
							iResult = libusb_release_interface(m_ptDevHandle, ptId->ucInterface);
							if( iResult!=LIBUSB_SUCCESS )
							{
								/* failed to release the interface */
								fprintf(stderr, "%s(%p): failed to release interface %d of device %s after a successful claim: %d:%s\n", m_pcPluginId, this, ptId->ucInterface, acName, iResult, libusb_strerror(iResult));
							}
						}

						/* create the new instance */
						ptRef = new romloader_usb_reference(acName, m_pcPluginId, acPathString, fDeviceIsBusy, ptProvider);
						/* Is enough space in the array for one more entry? */
						if( sizRefCnt>=sizRefMax )
						{
							/* No -> expand the array. */
							sizRefMax *= 2;
							/* Detect overflow or limitation. */
							if( sizRefMax<=sizRefCnt )
							{
								iResult = -1;
								break;
							}
							/* Reallocate the array. */
							pptRefNew = (romloader_usb_reference**)realloc(pptRef, sizRefMax*sizeof(romloader_usb_reference*));
							if( pptRefNew==NULL )
							{
								iResult = -1;
								break;
							}
							pptRef = pptRefNew;
						}
						pptRef[sizRefCnt++] = ptRef;

						/* close the device */
						libusb_close(m_ptDevHandle);
						m_ptDevHandle = NULL;
					}
				}
				/* next list item */
				++ptDevCnt;
			}
			/* free the device list */
			if( ptDeviceList!=NULL )
			{
				libusb_free_device_list(ptDeviceList, 1);
			}
		}
	}

	if( iResult!=0 && pptRef!=NULL )
	{
		/* Free all references in the array. */
		free_references(pptRef, sizRefCnt);
		/* No more array. */
		pptRef = NULL;
		sizRefCnt = 0;
	}

	*ppptReferences = pptRef;
	*psizReferences = sizRefCnt;

	return iResult;
}



void romloader_usb_device_libusb::free_references(romloader_usb_reference **pptReferences, size_t sizReferences)
{
	romloader_usb_reference *ptRef;


	if( pptReferences!=NULL )
	{
		/* Free all references in the array. */
		while( sizReferences>0 )
		{
			--sizReferences;
			ptRef = pptReferences[sizReferences];
			if( ptRef!=NULL )
			{
				delete ptRef;
			}
		}
		/* Free the array. */
		free(pptReferences);
	}
}



libusb_device *romloader_usb_device_libusb::find_netx_device(libusb_device **ptDeviceList, ssize_t ssizDevList, unsigned int uiBusNr, unsigned int uiDeviceAdr)
{
	libusb_device **ptDevCnt;
	libusb_device **ptDevEnd;
	libusb_device *ptDev;
	libusb_device *ptNetxDevice;


	/* No netx found. */
	ptNetxDevice = NULL;

	/* loop over all devices */
	ptDevCnt = ptDeviceList;
	ptDevEnd = ptDevCnt + ssizDevList;
	while( ptNetxDevice==NULL && ptDevCnt<ptDevEnd )
	{
		ptDev = *ptDevCnt;
		if( libusb_get_bus_number(ptDev)==uiBusNr && libusb_get_device_address(ptDev)==uiDeviceAdr )
		{
			ptNetxDevice = ptDev;
			break;
		}

		++ptDevCnt;
	}

	return ptNetxDevice;
}


int romloader_usb_device_libusb::setup_netx_device(libusb_device *ptNetxDevice, const NETX_USB_DEVICE_T *ptId)
{
	int iResult;


	printf("romloader_usb_device_libusb::setup_netx_device(): ptNetxDevice=%p, ptId=%p\n", ptNetxDevice, ptId);

	iResult = libusb_open(ptNetxDevice, &m_ptDevHandle);
	if( iResult!=LIBUSB_SUCCESS )
	{
		fprintf(stderr, "%s(%p): failed to open the device: %d:%s\n", m_pcPluginId, this, iResult, libusb_strerror(iResult));
	}
	else
	{
		/* Set the configuration. */
		/* Do not set the configuration for a combo device. The composite interface is owned by the composite device driver.
		 * The good thing is: the configuration is already set.
		 */
		if( ptId->ucConfiguration!=0 )
		{
//			printf("%s(%p): set device configuration to %d.\n", m_pcPluginId, this, ptId->ucConfiguration);
			iResult = libusb_set_configuration(m_ptDevHandle, ptId->ucConfiguration);
		}
		if( iResult!=LIBUSB_SUCCESS )
		{
			/* failed to set the configuration */
			fprintf(stderr, "%s(%p): failed to set the configuration %d of device: %d:%s\n", m_pcPluginId, this, ptId->ucConfiguration, iResult, libusb_strerror(iResult));
		}
		else
		{
			/* Claim the interface. */
//			printf("%s(%p): claim interface %d.\n", m_pcPluginId, this, ptId->ucInterface);
			iResult = libusb_claim_interface(m_ptDevHandle, ptId->ucInterface);
			/* Do not print an error message for busy devices. This will be done above. */
			if( iResult!=LIBUSB_SUCCESS && iResult!=LIBUSB_ERROR_BUSY )
			{
				/* Failed to claim the interface. */
				fprintf(stderr, "%s(%p): failed to claim interface %d: %d:%s\n", m_pcPluginId, this, ptId->ucInterface, iResult, libusb_strerror(iResult));
			}
		}
		/* Cleanup in error case. */
		if( iResult!=LIBUSB_SUCCESS )
		{
			/* Close the device. */
			libusb_close(m_ptDevHandle);
			m_ptDevHandle = NULL;
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::Connect(unsigned int uiBusNr, unsigned int uiDeviceAdr)
{
	int iResult;
	ssize_t ssizDevList;
	libusb_device **ptDeviceList;
	libusb_device *ptUsbDevice;
	libusb_device *ptUpdatedNetxDevice;
	const NETX_USB_DEVICE_T *ptId;


	ptDeviceList = NULL;

	/* Search device with bus and address. */
	ssizDevList = libusb_get_device_list(m_ptLibUsbContext, &ptDeviceList);
	if( ssizDevList<0 )
	{
		/* Failed to detect devices. */
		fprintf(stderr, "%s(%p): failed to detect usb devices: %ld:%s\n", m_pcPluginId, this, ssizDevList, libusb_strerror(ssizDevList));
		iResult = (int)ssizDevList;
	}
	else
	{
		ptUsbDevice = find_netx_device(ptDeviceList, ssizDevList, uiBusNr, uiDeviceAdr);
		if( ptUsbDevice==NULL )
		{
			fprintf(stderr, "%s(%p): interface not found. Maybe it was plugged out.\n", m_pcPluginId, this);
			iResult = LIBUSB_ERROR_NOT_FOUND;
		}
		else
		{
			ptId = identifyDevice(ptUsbDevice);
			if( ptId==NULL )
			{
				fprintf(stderr, "%s(%p): The device could not be identified as a netX.\n", m_pcPluginId, this);
				iResult = LIBUSB_ERROR_NOT_FOUND;
			}
			else
			{
				/* Copy all data of the chip id. */
				memcpy(&m_tDeviceId, ptId, sizeof(NETX_USB_DEVICE_T));

				/* Reference the device before freeing the list. */
				libusb_ref_device(ptUsbDevice);
				iResult = LIBUSB_SUCCESS;
			}
		}

		/* free the device list */
		libusb_free_device_list(ptDeviceList, 1);

		if( iResult==LIBUSB_SUCCESS )
		{
			/* NOTE: hier sollte eine generelle Entscheidung rein, ob das Gerät geupdated werden soll. */



			/* Does this device need an update? */
			iResult = LIBUSB_ERROR_OTHER;
			switch(m_tDeviceId.tCommandSet)
			{
			case ROMLOADER_COMMANDSET_UNKNOWN:
				/* No update plan for an unknown device. */
				iResult = LIBUSB_ERROR_OTHER;
				break;

			case ROMLOADER_COMMANDSET_ABOOT_OR_HBOOT1:
			case ROMLOADER_COMMANDSET_MI1:
				iResult = update_old_netx_device(ptUsbDevice, &ptUpdatedNetxDevice);
				if( iResult==LIBUSB_SUCCESS )
				{
					/* Use the updated device. */
					ptUsbDevice = ptUpdatedNetxDevice;
				}
				break;

			case ROMLOADER_COMMANDSET_MI2:
				/* The device uses the hboot v3 protocol. */
				iResult = LIBUSB_SUCCESS;
				break;
			}

			if( iResult==LIBUSB_SUCCESS )
			{
				iResult = setup_netx_device(ptUsbDevice, ptId);
				if( iResult==LIBUSB_ERROR_BUSY /* || iResult==LIBUSB_ERROR_SYS_BUSY */ )
				{
					fprintf(stderr, "%s(%p): the device is busy. Maybe some other program is accessing it right now.\n", m_pcPluginId, this);
					libusb_unref_device(ptUsbDevice);
				}
				else if( iResult!=LIBUSB_SUCCESS )
				{
					fprintf(stderr, "%s(%p): failed to setup the device, trying to reset it.\n", m_pcPluginId, this);

					/* Reset the device and try again. */
					/* FIXME: setup_netx_device already closed the device. */
					iResult = libusb_reset_and_close_device();
					if( iResult!=LIBUSB_SUCCESS )
					{
						fprintf(stderr, "%s(%p): failed to reset the netx, giving up: %d:%s\n", m_pcPluginId, this, iResult, libusb_strerror(iResult));
						libusb_release_interface(m_ptDevHandle, ptId->ucInterface);
						libusb_close(m_ptDevHandle);
						m_ptDevHandle = NULL;
					}
					else
					{
						fprintf(stderr, "%s(%p): reset ok!\n", m_pcPluginId, this);

						iResult = setup_netx_device(ptUsbDevice, ptId);
						if( iResult==LIBUSB_ERROR_BUSY /* || iResult==LIBUSB_ERROR_SYS_BUSY */ )
						{
							fprintf(stderr, "%s(%p): the device is busy after the reset. Maybe some other program is accessing it right now.\n", m_pcPluginId, this);
						}
						else if( iResult!=LIBUSB_SUCCESS )
						{
							/* Free the old device. */
							fprintf(stderr, "%s(%p): lost device after reset!\n", m_pcPluginId, this);
							iResult = LIBUSB_ERROR_OTHER;
						}
					}

					libusb_unref_device(ptUsbDevice);
				}
			}
		}
	}

	printf("-Connect(): iResult=%d\n", iResult);

	return iResult;
}


void romloader_usb_device_libusb::Disconnect(void)
{
	if( m_ptDevHandle!=NULL )
	{
		libusb_release_interface(m_ptDevHandle, m_tDeviceId.ucInterface);
		libusb_close(m_ptDevHandle);
		m_ptDevHandle = NULL;
	}
	m_ptDevHandle = NULL;
}


const romloader_usb_device::NETX_USB_DEVICE_T *romloader_usb_device_libusb::identifyDevice(libusb_device *ptDevice) const
{
	const NETX_USB_DEVICE_T *ptDevHit;
	int iResult;
	struct libusb_device_descriptor sDevDesc;
	const NETX_USB_DEVICE_T *ptDevCnt;
	const NETX_USB_DEVICE_T *ptDevEnd;
	libusb_device_handle *ptDevHandle;


	ptDevHit = NULL;
	if( ptDevice!=NULL )
	{
		/* Try to open the device. */
		iResult = libusb_open(ptDevice, &ptDevHandle);
		if( iResult==LIBUSB_SUCCESS )
		{
			iResult = libusb_get_descriptor(ptDevHandle, LIBUSB_DT_DEVICE, 0, (unsigned char*)&sDevDesc, sizeof(struct libusb_device_descriptor));
			if( iResult==sizeof(struct libusb_device_descriptor) )
			{
				ptDevCnt = atNetxUsbDevices;
				ptDevEnd = atNetxUsbDevices + (sizeof(atNetxUsbDevices)/sizeof(atNetxUsbDevices[0]));
				while( ptDevCnt<ptDevEnd )
				{
					if(
						sDevDesc.idVendor==ptDevCnt->usVendorId &&
						sDevDesc.idProduct==ptDevCnt->usDeviceId &&
						sDevDesc.bcdDevice==ptDevCnt->usBcdDevice
					)
					{
						/* Found a matching device. */
						printf("identifyDevice: Found device %04x:%04x:%04x\n", sDevDesc.idVendor, sDevDesc.idProduct, sDevDesc.bcdDevice);
						ptDevHit = ptDevCnt;
						break;
					}
					else
					{
						++ptDevCnt;
					}
				}
			}
			libusb_close(ptDevHandle);
		}
	}

	return ptDevHit;
}


const romloader_usb_device_libusb::LIBUSB_STRERROR_T romloader_usb_device_libusb::atStrError[14] =
{
	{ LIBUSB_SUCCESS,               "success" },
	{ LIBUSB_ERROR_IO,              "input/output error" },
	{ LIBUSB_ERROR_INVALID_PARAM,   "invalid parameter" },
	{ LIBUSB_ERROR_ACCESS,          "access denied (insufficient permissions)" },
	{ LIBUSB_ERROR_NO_DEVICE,       "no such device (it may have been disconnected)" },
	{ LIBUSB_ERROR_NOT_FOUND,       "entity not found" },
	{ LIBUSB_ERROR_BUSY,            "resource busy" },
	{ LIBUSB_ERROR_TIMEOUT,         "operation timed out" },
	{ LIBUSB_ERROR_OVERFLOW,        "overflow" },
	{ LIBUSB_ERROR_PIPE,            "pipe error" },
	{ LIBUSB_ERROR_INTERRUPTED,     "system call interrupted (perhaps due to signal)" },
	{ LIBUSB_ERROR_NO_MEM,          "insufficient memory" },
	{ LIBUSB_ERROR_NOT_SUPPORTED,   "operation not supported or unimplemented on this platform" },
	{ LIBUSB_ERROR_OTHER,           "other error" }
};


const char *romloader_usb_device_libusb::libusb_strerror(int iError)
{
	const char *pcMessage = "unknown error";
	const LIBUSB_STRERROR_T *ptCnt;
	const LIBUSB_STRERROR_T *ptEnd;


	ptCnt = atStrError;
	ptEnd = atStrError + (sizeof(atStrError)/sizeof(atStrError[0]));
	while( ptCnt<ptEnd )
	{
		if(ptCnt->iError==iError)
		{
			pcMessage = ptCnt->pcMessage;
			break;
		}
		++ptCnt;
	}

	return pcMessage;
}


int romloader_usb_device_libusb::netx10_discard_until_timeout(libusb_device_handle *ptDevHandle)
{
	unsigned char aucBuffer[64];
	int iResult;
	int iProcessed;
	unsigned int uiTimeoutMs;


	uiTimeoutMs = 100;

	do
	{
		iProcessed=0;
		iResult = libusb_bulk_transfer(ptDevHandle, m_tDeviceId.ucEndpoint_In, aucBuffer, sizeof(aucBuffer), &iProcessed, uiTimeoutMs);
		if( iResult==LIBUSB_ERROR_TIMEOUT )
		{
			/* Timeouts are great! */
			iResult = 0;
			break;
		}
		else if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to receive data: (%d)%s\n", m_pcPluginId, this, iResult, libusb_strerror(iResult));
			break;
		}
	} while( iResult==0 );

	return iResult;
}


int romloader_usb_device_libusb::netx500_exchange_data(libusb_device_handle *ptDevHandle, const unsigned char *pucOutBuffer, unsigned char *pucInBuffer)
{
	int iResult;
	int iProcessed;
	const unsigned int uiTimeoutMs = 100;
	/* The packet size is fixed to 64 bytes in this protocol. */
	const int iPacketSize = 64;


	iResult = libusb_bulk_transfer(ptDevHandle, m_tDeviceId.ucEndpoint_Out, (unsigned char*)pucOutBuffer, iPacketSize, &iProcessed, uiTimeoutMs);
	if( iResult==0 )
	{
		iResult = libusb_bulk_transfer(ptDevHandle, m_tDeviceId.ucEndpoint_In, pucInBuffer, iPacketSize, &iProcessed, uiTimeoutMs);
	}

	return iResult;
}


int romloader_usb_device_libusb::netx500_discard_until_timeout(libusb_device_handle *ptDevHandle)
{
	unsigned char aucOutBuffer[64] = {0};
	unsigned char aucInBuffer[64] = {0};
	int iResult;


	/* Throw away all data until the end. */
	do
	{
		iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
		if( iResult!=LIBUSB_SUCCESS )
		{
			/* Error! */
			break;
		}
//		printf("discard packet:\n");
//		hexdump(aucInBuffer, 64);
	} while( aucInBuffer[0]!=0 );

	return iResult;
}


int romloader_usb_device_libusb::netx500_load_code(libusb_device_handle *ptDevHandle, const unsigned char* pucNetxCode, size_t sizNetxCode)
{
	unsigned short usCrc;
	size_t sizLine;
	unsigned long ulLoadAddress;
	int iResult;
	const unsigned char *pucDataCnt;
	const unsigned char *pucDataEnd;
	unsigned char aucOutBuffer[64] = {0};
	unsigned char aucInBuffer[64] = {0};
	size_t sizChunkSize;


	/* Be optimistic. */
	iResult = 0;

	if( pucNetxCode[0x00]!='m' || pucNetxCode[0x01]!='o' || pucNetxCode[0x02]!='o' || pucNetxCode[0x03]!='h' )
	{
		fprintf(stderr, "%s(%p): Invalid netx code, header missing.\n", m_pcPluginId, this);
		iResult = -1;
	}
	else
	{
		/* Get the load address. */
		ulLoadAddress  =  (unsigned long)(pucNetxCode[0x04]);
		ulLoadAddress |= ((unsigned long)(pucNetxCode[0x05])) <<  8U;
		ulLoadAddress |= ((unsigned long)(pucNetxCode[0x06])) << 16U;
		ulLoadAddress |= ((unsigned long)(pucNetxCode[0x07])) << 24U;

//		printf("Load 0x%08x bytes to 0x%08x.\n", sizNetxCode, ulLoadAddress);

		/* Generate crc16 checksum. */
		usCrc = netx500_crc16(pucNetxCode, sizNetxCode);

		/* Generate load command. */
		sizLine = snprintf((char*)(aucOutBuffer+1), sizeof(aucOutBuffer)-1, "load %lx %lx %04X\n", ulLoadAddress, sizNetxCode, usCrc);
		/* Set the length. */
		aucOutBuffer[0] = (unsigned char)(sizLine + 1);

		/* Send the command. */
		iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
		if( iResult==LIBUSB_SUCCESS )
		{
			/* Terminate the command. */
			aucOutBuffer[0] = 0;
			iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
			if( iResult==LIBUSB_SUCCESS )
			{
				/* Now send the data part. */
				pucDataCnt = pucNetxCode;
				pucDataEnd = pucNetxCode + sizNetxCode;
				while( pucDataCnt<pucDataEnd )
				{
					/* Get the size of the next data chunk. */
					sizChunkSize = pucDataEnd - pucDataCnt;
					if( sizChunkSize>63 )
					{
						sizChunkSize = 63;
					}
					/* Copy data to the packet. */
					memcpy(aucOutBuffer+1, pucDataCnt, sizChunkSize);
					aucOutBuffer[0] = (unsigned char)(sizChunkSize+1);

					iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
					if( iResult!=LIBUSB_SUCCESS )
					{
						break;
					}
					pucDataCnt += sizChunkSize;
				}
			}
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::netx10_load_code(libusb_device_handle* ptDevHandle, const unsigned char* pucNetxCode, size_t sizNetxCode)
{
	size_t sizLine;
	unsigned long ulLoadAddress;
	unsigned char aucBuffer[64];
	unsigned int uiTimeoutMs;
	int iProcessed;
	int iResult;
	uuencoder tUuencoder;


	/* Be optimistic. */
	iResult = 0;

	uiTimeoutMs = 100;

	if( pucNetxCode[0x00]!='m' || pucNetxCode[0x01]!='o' || pucNetxCode[0x02]!='o' || pucNetxCode[0x03]!='h' )
	{
		fprintf(stderr, "%s(%p): Invalid netx code, header missing.\n", m_pcPluginId, this);
		iResult = -1;
	}
	else
	{
		/* Construct the command. */
		ulLoadAddress  =  (unsigned long)(pucNetxCode[0x04]);
		ulLoadAddress |= ((unsigned long)(pucNetxCode[0x05])) <<  8U;
		ulLoadAddress |= ((unsigned long)(pucNetxCode[0x06])) << 16U;
		ulLoadAddress |= ((unsigned long)(pucNetxCode[0x07])) << 24U;
		sizLine = snprintf((char*)aucBuffer, sizeof(aucBuffer), "l %lx\n", ulLoadAddress);
		iResult = libusb_bulk_transfer(ptDevHandle, m_tDeviceId.ucEndpoint_Out, aucBuffer, sizLine, &iProcessed, uiTimeoutMs);
		if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to send packet!\n", m_pcPluginId, this);
			iResult = -1;
		}
		else if( sizLine!=iProcessed )
		{
			fprintf(stderr, "%s(%p): Requested to send %ld bytes, but only %d were processed!\n", m_pcPluginId, this, sizLine, iProcessed);
			iResult = -1;
		}
		else
		{
			netx10_discard_until_timeout(ptDevHandle);

			tUuencoder.set_data(pucNetxCode, sizNetxCode);

			/* Send the data line by line with a delay of 10ms. */
			do
			{
				sizLine = tUuencoder.process((char*)aucBuffer, sizeof(aucBuffer));
				if( sizLine!=0 )
				{
					uiTimeoutMs = 100;

					iResult = libusb_bulk_transfer(ptDevHandle, m_tDeviceId.ucEndpoint_Out, aucBuffer, sizLine, &iProcessed, uiTimeoutMs);
					if( iResult!=0 )
					{
						fprintf(stderr, "%s(%p): Failed to send packet!\n", m_pcPluginId, this);
						iResult = -1;
						break;
					}
					else if( sizLine!=iProcessed )
					{
						fprintf(stderr, "%s(%p): Requested to send %ld bytes, but only %d were processed!\n", m_pcPluginId, this, sizLine, iProcessed);
						iResult = -1;
						break;
					}

					SLEEP_MS(10);
				}
			} while( tUuencoder.isFinished()==false );

			if( iResult==0 )
			{
				netx10_discard_until_timeout(ptDevHandle);
			}
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::netx56_execute_command(libusb_device_handle *ptDevHandle, const unsigned char *aucOutBuf, size_t sizOutBuf, unsigned char *aucInBuf, size_t *psizInBuf)
{
	int iResult;
	size_t sizProcessed;
	int iProcessed;
	unsigned int uiTimeoutMs;
	const unsigned char ucEndpoint_In = 0x85;
	const unsigned char ucEndpoint_Out = 0x04;


	uiTimeoutMs = 500; // 100

	iResult = libusb_bulk_transfer(ptDevHandle, ucEndpoint_Out, (unsigned char*)aucOutBuf, sizOutBuf, &iProcessed, uiTimeoutMs);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to send data: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
	}
	else if( sizOutBuf!=iProcessed )
	{
		fprintf(stderr, "%s(%p): Requested to send %ld bytes, but only %d were processed!\n", m_pcPluginId, this, sizOutBuf, iProcessed);
		iResult = 1;
	}
	else
	{
		iResult = libusb_bulk_transfer(ptDevHandle, ucEndpoint_In, aucInBuf, 64, &iProcessed, uiTimeoutMs);
		if( iResult==0 )
		{
			if( iProcessed==0 )
			{
				fprintf(stderr, "%s(%p): Received empty packet!\n", m_pcPluginId, this);
				iResult = 1;
			}
			else
			{
				*psizInBuf = iProcessed;
			}
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::netx56_load_code(libusb_device_handle* ptDevHandle, const unsigned char* pucNetxCode, size_t sizNetxCode)
{
	size_t sizChunk;
	int iResult;
	size_t sizInBuf;
	unsigned char ucCommand;
	unsigned char ucStatus;
	bool fIsRunning;
	long lBytesProcessed;
	unsigned char aucTxBuf[64];
	unsigned char aucRxBuf[64];
	unsigned long ulNetxAddress;


//	fprintf(stderr, "+netx56_load_code(): ptDevHandle=%p, pucNetxCode=%p, sizNetxCode=%d\n", ptDevHandle, pucNetxCode, sizNetxCode);

	/* Be optimistic. */
	iResult = 0;

	if( pucNetxCode[0x00]!='m' || pucNetxCode[0x01]!='o' || pucNetxCode[0x02]!='o' || pucNetxCode[0x03]!='h' )
	{
		fprintf(stderr, "%s(%p): Invalid netx code, header missing.\n", m_pcPluginId, this);
		iResult = -1;
	}
	else
	{
		/* Construct the command. */
		ulNetxAddress  =  (unsigned long)(pucNetxCode[0x04]);
		ulNetxAddress |= ((unsigned long)(pucNetxCode[0x05])) <<  8U;
		ulNetxAddress |= ((unsigned long)(pucNetxCode[0x06])) << 16U;
		ulNetxAddress |= ((unsigned long)(pucNetxCode[0x07])) << 24U;

		do
		{
			sizChunk = sizNetxCode;
			if( sizChunk>58 )
			{
				sizChunk = 58;
			}

			/* Construct the command packet. */
			aucTxBuf[0x00] = MONITOR_PACKET_TYP_CommandWrite |
			                 (MONITOR_ACCESSSIZE_Byte<<MONITOR_ACCESSSIZE_SRT);
			aucTxBuf[0x01] = (unsigned char)sizChunk;
			aucTxBuf[0x02] = (unsigned char)( ulNetxAddress       & 0xffU);
			aucTxBuf[0x03] = (unsigned char)((ulNetxAddress>> 8U) & 0xffU);
			aucTxBuf[0x04] = (unsigned char)((ulNetxAddress>>16U) & 0xffU);
			aucTxBuf[0x05] = (unsigned char)((ulNetxAddress>>24U) & 0xffU);
			memcpy(aucTxBuf+6, pucNetxCode, sizChunk);

			iResult = netx56_execute_command(ptDevHandle, aucTxBuf, sizChunk+6, aucRxBuf, &sizInBuf);
			if( iResult!=0 )
			{
				break;
			}
			else if( sizInBuf!=1 )
			{
				hexdump(aucRxBuf, sizInBuf);
				iResult = -1;
				break;
			}
			else
			{
				pucNetxCode += sizChunk;
				sizNetxCode -= sizChunk;
				ulNetxAddress += sizChunk;
			}
		} while( sizNetxCode!=0 );
	}

	return iResult;
}




int romloader_usb_device_libusb::netx500_start_code(libusb_device_handle *ptDevHandle, const unsigned char *pucNetxCode)
{
	int iResult;
	unsigned long ulExecAddress;
	size_t sizLine;
	unsigned char aucOutBuffer[64] = {0};
	unsigned char aucInBuffer[64] = {0};


	ulExecAddress  =  (unsigned long)(pucNetxCode[0x08]);
	ulExecAddress |= ((unsigned long)(pucNetxCode[0x09])) <<  8U;
	ulExecAddress |= ((unsigned long)(pucNetxCode[0x0a])) << 16U;
	ulExecAddress |= ((unsigned long)(pucNetxCode[0x0b])) << 24U;

	/* Generate call command. */
	sizLine = snprintf((char*)(aucOutBuffer+1), sizeof(aucOutBuffer)-1, "call %lx 0\n", ulExecAddress);
	/* Set the length. */
	aucOutBuffer[0] = (unsigned char)(sizLine + 1);

//	printf("Start code at 0x%08x.\n", ulExecAddress);

	/* Send the command. */
	iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
	if( iResult==LIBUSB_SUCCESS )
	{
		/* Terminate the command. */
		aucOutBuffer[0] = 0;
		iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
	}

	return iResult;
}


int romloader_usb_device_libusb::netx10_start_code(libusb_device_handle *ptDevHandle, const unsigned char *pucNetxCode)
{
	unsigned char aucBuffer[64];
	size_t sizBlock;
	int iResult;
	int iProcessed;
	unsigned int uiTimeoutMs;
	unsigned long ulExecAddress;


	/* Construct the command. */
	ulExecAddress  =  (unsigned long)(pucNetxCode[0x08]);
	ulExecAddress |= ((unsigned long)(pucNetxCode[0x09])) <<  8U;
	ulExecAddress |= ((unsigned long)(pucNetxCode[0x0a])) << 16U;
	ulExecAddress |= ((unsigned long)(pucNetxCode[0x0b])) << 24U;

	/* Construct the command. */
	sizBlock = sprintf((char*)aucBuffer, "g %lx 0\n", ulExecAddress);

	uiTimeoutMs = 1000;
	iResult = libusb_bulk_transfer(ptDevHandle, m_tDeviceId.ucEndpoint_Out, aucBuffer, sizBlock, &iProcessed, uiTimeoutMs);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to send packet!\n", m_pcPluginId, this);
		iResult = -1;
	}
	else if( sizBlock!=iProcessed )
	{
		fprintf(stderr, "%s(%p): Requested to send %ld bytes, but only %d were processed!\n", m_pcPluginId, this, sizBlock, iProcessed);
		iResult = -1;
	}
	else
	{
		netx10_discard_until_timeout(ptDevHandle);

		iResult = 0;
	}

	return iResult;
}



int romloader_usb_device_libusb::netx56_start_code(libusb_device_handle *ptDevHandle, const unsigned char *pucNetxCode)
{
	int iResult;
	int iProcessed;
	unsigned int uiTimeoutMs;
	unsigned long ulExecAddress;
	unsigned char aucTxBuf[64];
	unsigned char aucRxBuf[64];
	size_t sizRxBuf;


	/* Construct the command. */
	ulExecAddress  =  (unsigned long)(pucNetxCode[0x08]);
	ulExecAddress |= ((unsigned long)(pucNetxCode[0x09])) <<  8U;
	ulExecAddress |= ((unsigned long)(pucNetxCode[0x0a])) << 16U;
	ulExecAddress |= ((unsigned long)(pucNetxCode[0x0b])) << 24U;

	/* Construct the command packet. */
	aucTxBuf[0x00] = MONITOR_PACKET_TYP_CommandExecute;
	aucTxBuf[0x01] = (unsigned char)( ulExecAddress       & 0xffU);
	aucTxBuf[0x02] = (unsigned char)((ulExecAddress>> 8U) & 0xffU);
	aucTxBuf[0x03] = (unsigned char)((ulExecAddress>>16U) & 0xffU);
	aucTxBuf[0x04] = (unsigned char)((ulExecAddress>>24U) & 0xffU);
	aucTxBuf[0x05] = 0x00U;
	aucTxBuf[0x06] = 0x00U;
	aucTxBuf[0x07] = 0x00U;
	aucTxBuf[0x08] = 0x00U;

	iResult = netx56_execute_command(ptDevHandle, aucTxBuf, 9, aucRxBuf, &sizRxBuf);
	if( iResult!=0 )
	{
		iResult = -1;
	}
	else if( sizRxBuf!=1 )
	{
		fprintf(stderr, "call answer has invalid size: %ld\n", sizRxBuf);
		hexdump(aucRxBuf, sizRxBuf);
		iResult = -1;
	}
	else if( aucRxBuf[0]!=0x00 )
	{
		fprintf(stderr, "call answer status is not OK: 0x%02x\n", aucRxBuf[0]);
		iResult = -1;
	}

	return iResult;
}



int romloader_usb_device_libusb::netx10_upgrade_romcode(libusb_device *ptDevice, libusb_device **pptUpdatedNetxDevice)
{
	int iResult;
	libusb_device_handle *ptDevHandle;


//	printf(". Found old netX10 romcode, starting download.\n");

	iResult = libusb_open(ptDevice, &ptDevHandle);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to open the device: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
	}
	else
	{
		/* set the configuration */
		iResult = libusb_set_configuration(ptDevHandle, 1);
		if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to set config 1: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
		}
		else
		{
			iResult = libusb_claim_interface(ptDevHandle, 0);
			if( iResult!=0 )
			{
				fprintf(stderr, "%s(%p): Failed to claim interface 0: %s\n", m_pcPluginId, this, libusb_strerror(iResult));

				libusb_close(ptDevHandle);
			}
			else
			{
				/* Read data until 'newline''>' or timeout. */
				netx10_discard_until_timeout(ptDevHandle);

				/* Load data. */
				netx10_load_code(ptDevHandle, auc_usbmon_netx10, sizeof(auc_usbmon_netx10));

				/* Start the code. */
				netx10_start_code(ptDevHandle, auc_usbmon_netx10);

				libusb_release_interface(ptDevHandle, m_tDeviceId.ucInterface);
				libusb_close(ptDevHandle);

				SLEEP_MS(100);

				*pptUpdatedNetxDevice = ptDevice;

				iResult = 0;
			}
		}
	}

	return iResult;
}

#if 0
libusb_device *romloader_usb_device_libusb::find_usb_device_by_location(unsigned char ucLocation_Bus, unsigned char ucLocation_Port)
{
	libusb_device *ptFound;
	ssize_t ssizDevices;
	libusb_device **pptUsbDevCnt;
	libusb_device **pptUsbDevMax;
	unsigned char ucDevice_Bus;
	unsigned char ucDevice_Port;


	/* Nothing found yet. */
	ptFound = NULL;

	/* Get a list with all connected devices. */
	ssizDevices = libusb_get_device_list(m_ptLibUsbContext, &pptUsbDevCnt);
	pptUsbDevMax = pptUsbDevCnt + ssizDevices;
	while(pptUsbDevCnt<pptUsbDevMax)
	{
		/* Get the bus and the port of the device. */
		ucDevice_Bus = libusb_get_bus_number(*pptUsbDevCnt);
		ucDevice_Port = libusb_get_port_number(*pptUsbDevCnt);

		/* Is this the requested location? */
		if( ucLocation_Bus==ucDevice_Bus && ucLocation_Port==ucDevice_Port )
		{
			/* Yes -> this is the device. */
			ptFound = *pptUsbDevCnt;

			/* Reference the device. */
			libusb_ref_device(ptFound);

			break;
		}
		++pptUsbDevCnt;
	}

	return ptFound;
}
#endif


int romloader_usb_device_libusb::netx500_upgrade_romcode(libusb_device *ptDevice, libusb_device **pptUpdatedNetxDevice)
{
	int iResult;
	libusb_device_handle *ptDevHandle;
	unsigned char ucDevAddr_Bus;
	unsigned char ucDevAddr_Port;
	int iDelay;


	printf(". Found old netX500 romcode, starting download.\n");

	iResult = libusb_open(ptDevice, &ptDevHandle);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to open the device: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
	}
	else
	{
		/* set the configuration */
		iResult = libusb_set_configuration(ptDevHandle, 1);
		if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to set config 1: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
		}
		else
		{
			iResult = libusb_claim_interface(ptDevHandle, 0);
			if( iResult!=0 )
			{
				fprintf(stderr, "%s(%p): Failed to claim interface 0: %s\n", m_pcPluginId, this, libusb_strerror(iResult));

				libusb_close(ptDevHandle);
			}
			else
			{
				/* Read data until 'newline''>' or timeout. */
				netx500_discard_until_timeout(ptDevHandle);

				/* Load data. */
				netx500_load_code(ptDevHandle, auc_usbmon_netx500, sizeof(auc_usbmon_netx500));
				/* Discard load response. */
				netx500_discard_until_timeout(ptDevHandle);

				/* Start the code parameter. */
				netx500_start_code(ptDevHandle, auc_usbmon_netx500);

				/* Release the interface. */
				libusb_release_interface(ptDevHandle, m_tDeviceId.ucInterface);

				libusb_close(ptDevHandle);

				SLEEP_MS(100);

				*pptUpdatedNetxDevice = ptDevice;
				iResult = 0;
			}
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::netx56_upgrade_romcode(libusb_device *ptDevice, libusb_device **pptUpdatedNetxDevice)
{
	int iResult;
	libusb_device_handle *ptDevHandle;


	iResult = libusb_open(ptDevice, &ptDevHandle);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to open the device: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
	}
	else
	{
		iResult = libusb_claim_interface(ptDevHandle, 1);
		if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to claim interface 1: %s\n", m_pcPluginId, this, libusb_strerror(iResult));

			libusb_close(ptDevHandle);
		}
		else
		{
			/* Load data. */
			netx56_load_code(ptDevHandle, auc_usbmon_netx56, sizeof(auc_usbmon_netx56));

			/* Start the code. */
			netx56_start_code(ptDevHandle, auc_usbmon_netx56);

			libusb_release_interface(ptDevHandle, m_tDeviceId.ucInterface);
			libusb_close(ptDevHandle);

			SLEEP_MS(100);

			*pptUpdatedNetxDevice = ptDevice;

			iResult = 0;
		}
	}

	return iResult;
}



romloader::TRANSPORTSTATUS_T romloader_usb_device_libusb::send_packet(const unsigned char *aucOutBuf, size_t sizOutBuf, unsigned int uiTimeoutMs)
{
	romloader::TRANSPORTSTATUS_T tResult;
	int iResult;
	int iProcessed;


	tResult = romloader::TRANSPORTSTATUS_OK;

	iResult = libusb_bulk_transfer(m_ptDevHandle, m_tDeviceId.ucEndpoint_Out, (unsigned char*)aucOutBuf, sizOutBuf, &iProcessed, uiTimeoutMs);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to send data: %s  iProcessed == %d \n", m_pcPluginId, this, libusb_strerror(iResult), iProcessed);
		tResult = romloader::TRANSPORTSTATUS_SEND_FAILED;
	}
	else if( sizOutBuf!=iProcessed )
	{
		fprintf(stderr, "%s(%p): Requested to send %ld bytes, but only %d were processed!\n", m_pcPluginId, this, sizOutBuf, iProcessed);
		iResult = romloader::TRANSPORTSTATUS_SEND_FAILED;
	}
	/* The commands are transfered as transactions. This means data is grouped by packets smaller than 64 bytes.
	 * Does the transfer fill the last packet completely? */
	else if( m_tDeviceId.fDeviceSupportsTransactions==true && (sizOutBuf&0x3f)==0 )
	{
		/* Yes -> terminate the transaction with an empty packet. */
		iResult = libusb_bulk_transfer(m_ptDevHandle, m_tDeviceId.ucEndpoint_Out, (unsigned char*)aucOutBuf, 0, &iProcessed, uiTimeoutMs);
		if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to send the terminating empty packet: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
			iResult = romloader::TRANSPORTSTATUS_SEND_FAILED;
		}
	}

	return tResult;
}



romloader::TRANSPORTSTATUS_T romloader_usb_device_libusb::receive_packet(unsigned char *aucInBuf, size_t sizInBuf, size_t *psizInBuf, unsigned int uiTimeoutMs)
{
	romloader::TRANSPORTSTATUS_T tResult;
	int iResult;
	int iProcessed;
	unsigned char *pucBuffer;
	size_t sizTotal;
	size_t sizChunk;
	size_t sizData;
	uint16_t usCrc;
	uint8_t *pucCnt;
	uint8_t *pucEnd;


	/* Receive the data. */
	tResult = romloader::TRANSPORTSTATUS_OK;
	pucBuffer = aucInBuf;
	sizTotal = 0;
	do
	{
		iResult = libusb_bulk_transfer(m_ptDevHandle, m_tDeviceId.ucEndpoint_In, pucBuffer, 64, &iProcessed, uiTimeoutMs);
		if( iResult==0 )
		{
			if( iProcessed<0 )
			{
				fprintf(stderr, "Strange number of processed bytes from libusb_bulk_transfer: %d\n", iProcessed);
				tResult = romloader::TRANSPORTSTATUS_RECEIVE_FAILED;
			}
			else if( iProcessed==0 )
			{
				/* Received a zero length packet. This is the end of the transaction. */
				break;
			}
			/* The first packet must start with a "*". */
			else if( sizTotal==0 && pucBuffer[0]!=MONITOR_STREAM_PACKET_START )
			{
				/* No stream start. This is not the start of a packet. */
			}
			else
			{
				/* Now it is safe to cast the signed integer to size_t. */
				sizChunk = (size_t)iProcessed;

				sizTotal += sizChunk;
				pucBuffer += sizChunk;

				if( m_tDeviceId.fDeviceSupportsTransactions==true )
				{
					if( sizChunk<64 )
					{
						break;
					}

					/* Is enough space for one more packet left? */
					if( (sizTotal+64)>sizInBuf )
					{
						/* No -> do not continue! */
						fprintf(stderr, "Too much data, not enough space for another packet after 0x%08lx bytes.\n", sizTotal);
						iResult = romloader::TRANSPORTSTATUS_PACKET_TOO_LARGE;
					}
				}
				else
				{
					break;
				}
			}
		}
		else
		{
			tResult = romloader::TRANSPORTSTATUS_RECEIVE_FAILED;
		}
	} while( tResult==romloader::TRANSPORTSTATUS_OK );

	if( tResult==romloader::TRANSPORTSTATUS_OK )
	{
		/* Get the size of the data block. */
		sizData = aucInBuf[1] | (aucInBuf[2]<<8U);
		/* Is the data size valid? */
		if( (sizData+5)!=sizTotal )
		{
			fprintf(stderr, "Invalid packet size: %zd / %zd\n", sizData, sizTotal);
			tResult = romloader::TRANSPORTSTATUS_INVALID_PACKET_SIZE;
		}
		else
		{
			/* Build the CRC16. */
			usCrc = 0;
			pucCnt = aucInBuf + offsetof(struct MIV3_PACKET_HEADER_STRUCT, usDataSize);
			pucEnd = aucInBuf + sizTotal;
			while( pucCnt<pucEnd )
			{
				usCrc = crc16(usCrc, *(pucCnt++));
			}
			if( usCrc!=0 )
			{
				fprintf(stderr, "CRC does not match.\n");
				hexdump(aucInBuf, sizTotal);
				tResult = romloader::TRANSPORTSTATUS_CRC_MISMATCH;
			}
			else
			{
				*psizInBuf = sizTotal;
			}
		}
	}

	return tResult;
}



int romloader_usb_device_libusb::update_old_netx_device(libusb_device *ptNetxDevice, libusb_device **pptUpdatedNetxDevice)
{
	int iResult;
	const NETX_USB_DEVICE_T *ptId;
	libusb_device *ptUpdatedDevice;


	switch(m_tDeviceId.tChiptyp)
	{
	case ROMLOADER_CHIPTYP_UNKNOWN:
		iResult = LIBUSB_ERROR_OTHER;
		break;

	case ROMLOADER_CHIPTYP_NETX500:
		iResult = netx500_upgrade_romcode(ptNetxDevice, &ptUpdatedDevice);
		break;

	case ROMLOADER_CHIPTYP_NETX100:
		iResult = netx500_upgrade_romcode(ptNetxDevice, &ptUpdatedDevice);
		break;

	case ROMLOADER_CHIPTYP_NETX50:
		/* The netX50 romcode provides a CDC device over USB. It is not handled by this plugin. */
		iResult = LIBUSB_ERROR_OTHER;
		break;

	case ROMLOADER_CHIPTYP_NETX5:
		/* TODO: insert update code for the netX5. */
		iResult = LIBUSB_ERROR_OTHER;
		break;

	case ROMLOADER_CHIPTYP_NETX10:
		iResult = netx10_upgrade_romcode(ptNetxDevice, &ptUpdatedDevice);
		break;

	case ROMLOADER_CHIPTYP_NETX56:
		iResult = netx56_upgrade_romcode(ptNetxDevice, &ptUpdatedDevice);
		break;
	}

	/* Did the update succeed? */
	if( iResult==LIBUSB_SUCCESS )
	{
		/* Yes -> try to identify the device again. */
		ptId = identifyDevice(ptUpdatedDevice);
		if( ptId==NULL )
		{
			fprintf(stderr, "%s(%p): Failed to identify the updated device!\n", m_pcPluginId, this);
			iResult = LIBUSB_ERROR_OTHER;
		}
		else
		{
//			printf("After update: %s\n", ptId->pcName);
			/* Update the settings. */
			memcpy(&m_tDeviceId, ptId, sizeof(NETX_USB_DEVICE_T));

			*pptUpdatedNetxDevice = ptUpdatedDevice;
		}
	}

	return iResult;
}


unsigned short romloader_usb_device_libusb::netx500_crc16(const unsigned char *pucData, size_t sizData)
{
	const unsigned char *pucDataCnt;
	const unsigned char *pucDataEnd;
	unsigned int uiCrc;


	pucDataCnt = pucData;
	pucDataEnd = pucData + sizData;

	uiCrc = 0xffffU;
	while( pucDataCnt<pucDataEnd )
	{
		uiCrc  = (uiCrc >> 8U) | ((uiCrc & 0xff) << 8U);
		uiCrc ^= *(pucDataCnt++);
		uiCrc ^= (uiCrc & 0xffU) >> 4U;
		uiCrc ^= (uiCrc & 0x0fU) << 12U;
		uiCrc ^= ((uiCrc & 0xffU) << 4U) << 1U;
	}

	return (unsigned short)uiCrc;
}

