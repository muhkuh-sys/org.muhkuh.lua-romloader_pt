#include "romloader_jtag_openocd.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "shared_library.h"

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#       define OPENOCD_SHARED_LIBRARY_FILENAME "openocd.dll"
#       include <windows.h>

#elif defined(__GNUC__)
#       define OPENOCD_SHARED_LIBRARY_FILENAME "openocd.so"
#       include <dlfcn.h>
#       include <unistd.h>
#endif

/* NOTE: this must end with a slash. */
#define OPENOCD_SUBFOLDER "openocd/"



romloader_jtag_openocd::romloader_jtag_openocd(muhkuh_log *ptLog, romloader_jtag_options *ptOptions)
 : fJtagDeviceIsConnected(false)
 , m_ptDetected(NULL)
 , m_sizDetectedCnt(0)
 , m_sizDetectedMax(0)
 , m_pcPluginPath(NULL)
 , m_pcOpenocdDataPath(NULL)
 , m_pcOpenocdSharedObjectPath(NULL)
 , m_ptLibUsbContext(NULL)
 , m_ptLog(NULL)
 , m_ptOptions(NULL)
{
	memset(&m_tJtagDevice, 0, sizeof(ROMLOADER_JTAG_DEVICE_T));

	m_ptLog = ptLog;
	m_ptOptions = ptOptions;

	libusb_init(&m_ptLibUsbContext);
	libusb_set_option(m_ptLibUsbContext, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);

	get_plugin_path();
	get_openocd_path();
}



romloader_jtag_openocd::~romloader_jtag_openocd(void)
{
	free_detect_entries();

	if( m_pcPluginPath!=NULL )
	{
		free(m_pcPluginPath);
		m_pcPluginPath = NULL;
	}
	if( m_pcOpenocdDataPath!=NULL )
	{
		free(m_pcOpenocdDataPath);
		m_pcOpenocdDataPath = NULL;
	}
	if( m_pcOpenocdSharedObjectPath!=NULL )
	{
		free(m_pcOpenocdSharedObjectPath);
		m_pcOpenocdSharedObjectPath = NULL;
	}

	if( m_ptLibUsbContext!=NULL )
	{
		libusb_exit(m_ptLibUsbContext);
	}
}



void romloader_jtag_openocd::get_plugin_path(void)
{
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	char *pcPath;
	DWORD dwFlags;
	BOOL fResult;
	LPCTSTR pfnMember;
	HMODULE hModule;
	DWORD dwBufferSize;
	DWORD dwResult;
	char *pcSlash;
	size_t sizPath;


	pcPath = NULL;

	/* Get the module by an address, but do not increase the refcount. */
	dwFlags =   GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
	          | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
	/* Use this function to identify the module. */
	pfnMember = (LPCTSTR)(romloader_jtag_openocd::atOpenOcdResolve);
	fResult = GetModuleHandleEx(dwFlags, pfnMember, &hModule);
	if( fResult==0 )
	{
		m_ptLog->fatal("Failed to get the module handle: %d", GetLastError());
	}
	else
	{
		dwBufferSize = 0x00010000;
		pcPath = (char*)malloc(dwBufferSize);
		if( pcPath==NULL )
		{
			m_ptLog->fatal("Failed to allocate %d bytes for the path buffer.", dwBufferSize);
		}
		else
		{
			dwResult = GetModuleFileName(hModule, pcPath, dwBufferSize);
			/* NOTE: dwResult contains the length of the string without the terminating 0.
			 *       If the buffer is too small, the function returns the provided size of
			 *       the buffer, in this case "dwBufferSize".
			 *       Therefore the function failed if the result is dwBufferSize.
			 */
			if( dwResult>0 && dwResult<dwBufferSize )
			{
				m_ptLog->debug("Module path: '%s'", pcPath);

				/* Find the last backslash in the path. */
				pcSlash = strrchr(pcPath, '\\');
				if( pcSlash==NULL )
				{
					m_ptLog->fatal("Failed to find the end of the path!");
					free(pcPath);
					pcPath = NULL;
				}
				else
				{
					/* Terminate the string after the last slash. */
					pcSlash[1] = 0;

					/* Allocate the new buffer. */
					sizPath = strlen(pcPath);
					m_pcPluginPath = (char*)malloc(sizPath + 1);
					if( m_pcPluginPath==NULL )
					{
						m_ptLog->fatal("Failed to allocate a buffer for the plugin path.");
					}
					else
					{
						memcpy(m_pcPluginPath, pcPath, sizPath + 1);
					}

					free(pcPath);
					pcPath = NULL;
				}
			}
			else
			{
				m_ptLog->fatal("Failed to get the module file name: %d", dwResult);
				free(pcPath);
				pcPath = NULL;
			}
		}
	}
#elif defined(__GNUC__)
	Dl_info tDlInfo;
	int iResult;
	size_t sizPath;
	const char *pcSlash;
	size_t sizCwdBufferSize;
	size_t sizCwd;
	char *pcCwd;
	char *pcGetCwdResult;
	int iCwdAddSlash;


	iResult = dladdr(romloader_jtag_openocd::atOpenOcdResolve, &tDlInfo);
	if( iResult==0 )
	{
		m_ptLog->fatal("Failed to get information about the shared object.");
	}
	else
	{
		if( tDlInfo.dli_fname!=NULL )
		{
			m_ptLog->debug("Path to the shared object: '%s'", tDlInfo.dli_fname);

			/* Is this an absolute path? */
			iResult = -1;
			if( tDlInfo.dli_fname[0]=='/' )
			{
				/* Yes -> no need to prepend the current working directory. */
				sizCwd = 0;
				pcCwd = NULL;
				iCwdAddSlash = 0;
				iResult = 0;
			}
			else
			{
				/* No, prepend the current working folder. */
				sizCwdBufferSize = 65536;
				pcCwd = (char*)malloc(sizCwdBufferSize);
				if( pcCwd==NULL )
				{
					m_ptLog->fatal("Failed to allocate a buffer for the current working folder.");
				}
				else
				{
					pcGetCwdResult = getcwd(pcCwd, sizCwdBufferSize);
					if( pcGetCwdResult==NULL )
					{
						m_ptLog->fatal("Failed to get the current working folder.");
					}
					else
					{
						sizCwd = strlen(pcCwd);
						iCwdAddSlash = 0;
						if( sizCwd>0 && pcCwd[sizCwd-1]!='/' )
						{
							iCwdAddSlash = 1;
						}
						iResult = 0;
					}
				}
			}

			if( iResult==0 )
			{
				/* Find the last backslash in the path. */
				pcSlash = strrchr(tDlInfo.dli_fname, '/');
				if( pcSlash==NULL )
				{
					m_ptLog->fatal("Failed to find the end of the path!");
					if( pcCwd!=NULL )
					{
						free(pcCwd);
						pcCwd = NULL;
						sizCwd = 0;
					}
				}
				else
				{
					sizPath = (size_t)(pcSlash - tDlInfo.dli_fname) + 1;
					m_pcPluginPath = (char*)malloc(sizCwd + iCwdAddSlash + sizPath + 1);
					if( m_pcPluginPath==NULL )
					{
						m_ptLog->fatal("Failed to allocate a buffer for the path.");
						if( pcCwd!=NULL )
						{
							free(pcCwd);
							pcCwd = NULL;
							sizCwd = 0;
						}
					}
					else
					{
						if( pcCwd!=NULL && sizCwd!=0 )
						{
							memcpy(m_pcPluginPath, pcCwd, sizCwd);
						}
						if( iCwdAddSlash!=0 )
						{
							m_pcPluginPath[sizCwd] = '/';
						}
						memcpy(m_pcPluginPath + sizCwd + iCwdAddSlash, tDlInfo.dli_fname, sizPath);
						m_pcPluginPath[sizCwd + iCwdAddSlash + sizPath] = 0;
					}
				}
			}
		}
	}
#endif
}



void romloader_jtag_openocd::get_openocd_path(void)
{
	size_t sizOpenOcdSo;
	size_t sizPluginPath;
	size_t sizOpenOcdSubfolder;


	sizOpenOcdSo = sizeof(OPENOCD_SHARED_LIBRARY_FILENAME);

	if( m_pcPluginPath==NULL )
	{
		m_pcOpenocdDataPath = (char*)malloc(2);
		if( m_pcOpenocdDataPath!=NULL )
		{
			m_pcOpenocdDataPath[0] = '.';
			m_pcOpenocdDataPath[1] = 0;

			m_pcOpenocdSharedObjectPath = (char*)malloc(sizOpenOcdSo + 1);
			if( m_pcOpenocdSharedObjectPath==NULL )
			{
				free(m_pcOpenocdDataPath);
				m_pcOpenocdDataPath = NULL;
			}
			else
			{
				/* Initialize the path with the name of the shared object only. */
				memcpy(m_pcOpenocdSharedObjectPath, OPENOCD_SHARED_LIBRARY_FILENAME, sizOpenOcdSo);
				/* Terminate the name with a 0. */
				m_pcOpenocdSharedObjectPath[sizOpenOcdSo] = 0;
			}
		}
	}
	else
	{
		sizPluginPath = strlen(m_pcPluginPath);
		sizOpenOcdSubfolder = strlen(OPENOCD_SUBFOLDER);
		m_pcOpenocdDataPath = (char*)malloc(sizPluginPath + sizOpenOcdSubfolder + 1);
		if( m_pcOpenocdDataPath!=NULL )
		{
			/* Copy the path to this module to the start of the OpenOCD path. */
			memcpy(m_pcOpenocdDataPath, m_pcPluginPath, sizPluginPath);
			/* Append the subfolder. */
			memcpy(m_pcOpenocdDataPath + sizPluginPath, OPENOCD_SUBFOLDER, sizOpenOcdSubfolder);
			/* Terminate the path. */
			m_pcOpenocdDataPath[sizPluginPath + sizOpenOcdSubfolder] = 0;

			m_pcOpenocdSharedObjectPath = (char*)malloc(sizPluginPath + sizOpenOcdSubfolder + sizOpenOcdSo + 1);
			if( m_pcOpenocdSharedObjectPath==NULL )
			{
				free(m_pcOpenocdDataPath);
				m_pcOpenocdDataPath = NULL;
			}
			else
			{
				/* Copy the path to this module to the start of the OpenOCD path. */
				memcpy(m_pcOpenocdSharedObjectPath, m_pcPluginPath, sizPluginPath);
				/* Append the subfolder. */
				memcpy(m_pcOpenocdSharedObjectPath + sizPluginPath, OPENOCD_SUBFOLDER, sizOpenOcdSubfolder);
				/* Append the name of the OpenOCD shared object. */
				memcpy(m_pcOpenocdSharedObjectPath + sizPluginPath + sizOpenOcdSubfolder, OPENOCD_SHARED_LIBRARY_FILENAME, sizOpenOcdSo);
				/* Terminate the name with a 0. */
				m_pcOpenocdSharedObjectPath[sizPluginPath + sizOpenOcdSubfolder + sizOpenOcdSo] = 0;
			}
		}
	}

	if( m_pcOpenocdDataPath!=NULL && m_pcOpenocdSharedObjectPath!=NULL )
	{
		m_ptLog->debug("The path to the OpenOCD data files is:    '%s'", m_pcOpenocdDataPath);
		m_ptLog->debug("The path to the OpenOCD shared object is: '%s'", m_pcOpenocdSharedObjectPath);
	}
	else
	{
		m_ptLog->fatal("Failed to get the path to the OpenOCD shared object.");
	}
}



const romloader_jtag_openocd::OPENOCD_NAME_RESOLVE_T romloader_jtag_openocd::atOpenOcdResolve[13] =
{
	{
		"muhkuh_openocd_init",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnInit) / sizeof(void*)
	},
	{
		"muhkuh_openocd_get_result",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnGetResult) / sizeof(void*)
	},
	{
		"muhkuh_openocd_command_run_line",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnCommandRunLine) / sizeof(void*)
	},
	{
		"muhkuh_openocd_uninit",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnUninit) / sizeof(void*)
	},
	{
		"muhkuh_openocd_read_data08",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnReadData08) / sizeof(void*)
	},
	{
		"muhkuh_openocd_read_data16",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnReadData16) / sizeof(void*)
	},
	{
		"muhkuh_openocd_read_data32",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnReadData32) / sizeof(void*)
	},
	{
		"muhkuh_openocd_read_image",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnReadImage) / sizeof(void*)
	},
	{
		"muhkuh_openocd_write_data08",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnWriteData08) / sizeof(void*)
	},
	{
		"muhkuh_openocd_write_data16",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnWriteData16) / sizeof(void*)
	},
	{
		"muhkuh_openocd_write_data32",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnWriteData32) / sizeof(void*)
	},
	{
		"muhkuh_openocd_write_image",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnWriteImage) / sizeof(void*)
	},
	{
		"muhkuh_openocd_call",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnCall) / sizeof(void*)
	}
};



void romloader_jtag_openocd::free_detect_entries(void)
{
	ROMLOADER_JTAG_DETECT_ENTRY_T *ptCnt;
	ROMLOADER_JTAG_DETECT_ENTRY_T *ptEnd;


	if( m_ptDetected!=NULL )
	{
		/* Loop over all entries in the list. */
		ptCnt = m_ptDetected;
		ptEnd = m_ptDetected + m_sizDetectedCnt;
		while( ptCnt<ptEnd )
		{
			if( ptCnt->pcInterface!=NULL )
			{
				free(ptCnt->pcInterface);
			}
			if( ptCnt->pcTarget!=NULL )
			{
				free(ptCnt->pcTarget);
			}
			if( ptCnt->pcLocation!=NULL )
			{
				free(ptCnt->pcLocation);
			}

			++ptCnt;
		}
		free(m_ptDetected);
		m_ptDetected = NULL;
	}
	m_sizDetectedCnt = 0;
	m_sizDetectedMax = 0;

}



int romloader_jtag_openocd::add_detected_entry(const char *pcInterface, const char *pcTarget, const char *pcLocation)
{
	int iResult;
	ROMLOADER_JTAG_DETECT_ENTRY_T *ptDetectedNew;


	/* Be optimistic. */
	iResult = 0;

	/* Is enough space in the array for one more entry? */
	if( m_sizDetectedCnt>=m_sizDetectedMax )
	{
		/* No -> expand the array. */
		m_sizDetectedMax *= 2;
		/* Detect overflow or limitation. */
		if( m_sizDetectedMax<=m_sizDetectedCnt )
		{
			iResult = -1;
		}
		else
		{
			/* Reallocate the array. */
			ptDetectedNew = (ROMLOADER_JTAG_DETECT_ENTRY_T*)realloc(m_ptDetected, m_sizDetectedMax*sizeof(ROMLOADER_JTAG_DETECT_ENTRY_T));
			if( ptDetectedNew==NULL )
			{
				iResult = -1;
			}
			else
			{
				m_ptDetected = ptDetectedNew;
			}
		}
	}

	if( iResult==0 )
	{
		m_ptDetected[m_sizDetectedCnt].pcInterface = strdup(pcInterface);
		m_ptDetected[m_sizDetectedCnt].pcTarget = strdup(pcTarget);
		m_ptDetected[m_sizDetectedCnt].pcLocation = strdup(pcLocation);
		++m_sizDetectedCnt;
	}

	return iResult;
}

/*
   Try to open the shared library.
   If successful, resolve method names and initialize the shared library.
 */
int romloader_jtag_openocd::openocd_open(ROMLOADER_JTAG_DEVICE_T *ptDevice)
{
	int iResult;
	void *pvSharedLibraryHandle;
	const OPENOCD_NAME_RESOLVE_T *ptCnt;
	const OPENOCD_NAME_RESOLVE_T *ptEnd;
	void *pvFn;
	void *pvOpenocdContext;
	char acError[1024];
	char acTclCmd[1024];


	/* Be optimistic. */
	iResult = 0;

	/* Try to open the shared library. */
	pvSharedLibraryHandle = sharedlib_open(m_pcOpenocdSharedObjectPath);
	if( pvSharedLibraryHandle==NULL )
	{
		/* Failed to open the shared library. */
		sharedlib_get_error(acError, sizeof(acError));
		m_ptLog->fatal("Failed to open the shared library %s: %s", m_pcOpenocdSharedObjectPath, acError);
		iResult = -1;
	}
	else
	{
		ptDevice->pvSharedLibraryHandle = pvSharedLibraryHandle;

		/* Try to resolve all symbols. */
		ptCnt = atOpenOcdResolve;
		ptEnd = atOpenOcdResolve + (sizeof(atOpenOcdResolve)/sizeof(atOpenOcdResolve[0]));
		while( ptCnt<ptEnd )
		{
			pvFn = sharedlib_resolve_symbol(pvSharedLibraryHandle, ptCnt->pstrSymbolName);
			if( pvFn==NULL )
			{
				iResult = -1;
				break;
			}
			else
			{
				ptDevice->tFunctions.pv[ptCnt->sizPointerOffset] = pvFn;
				++ptCnt;
			}
		}

		if( iResult==0 )
		{
			/* Call the init function and pass the data path as a search path for scripts. */
			pvOpenocdContext = ptDevice->tFunctions.tFn.pfnInit(m_pcOpenocdDataPath, romloader_jtag_openocd::openocd_output_handler, this);
			if( pvOpenocdContext==NULL )
			{
				m_ptLog->fatal("Failed to initialize the OpenOCD device context.");
				iResult = -1;
			}
			else
			{
				ptDevice->pvOpenocdContext = pvOpenocdContext;

				if( m_ptOptions==NULL )
				{
					m_ptLog->debug("No options to set.");
				}
				else
				{
					m_ptLog->debug("Setting options.");
					snprintf(acTclCmd, sizeof(acTclCmd),
					         "set __JTAG_RESET__ %d\nset __JTAG_SPEED__ %ld\n",
					         m_ptOptions->getOption_jtagReset(),
					         m_ptOptions->getOption_jtagFrequencyKhz()
					);
					iResult = ptDevice->tFunctions.tFn.pfnCommandRunLine(ptDevice->pvOpenocdContext, acTclCmd);
					if( iResult!=0 )
					{
						m_ptLog->fatal("Failed to set the options: %d", iResult);
						iResult = -1;
					}
				}
				if( iResult==0 )
				{
					m_ptLog->debug("Loading script.");
					iResult = ptDevice->tFunctions.tFn.pfnCommandRunLine(ptDevice->pvOpenocdContext, "source [find jtag_detect_init.tcl]");
					if( iResult!=0 )
					{
						m_ptLog->fatal("Failed to load the script: %d", iResult);
						iResult = -1;
					}
				}
			}
		}

		if( iResult!=0 )
		{
			/* Close the shared library. */
			sharedlib_close(pvSharedLibraryHandle);
			ptDevice->pvSharedLibraryHandle = NULL;
		}
	}

	return iResult;
}


/* Uninitialize and cose the shared library. */
void romloader_jtag_openocd::openocd_close(ROMLOADER_JTAG_DEVICE_T *ptDevice)
{
	if( ptDevice->pvSharedLibraryHandle!=NULL )
	{
		if( ptDevice->pvOpenocdContext!=NULL )
		{
			ptDevice->tFunctions.tFn.pfnUninit(ptDevice->pvOpenocdContext);
			ptDevice->pvOpenocdContext=NULL;
		}

		/* Close the shared library. */
		sharedlib_close(ptDevice->pvSharedLibraryHandle);
		ptDevice->pvSharedLibraryHandle=NULL;
	}
}



/* The romloader_jtag_openocd_init function opens the OpenOCD shared library,
   executes the "version" command and closes the library.

   This is a test if the shared library can be used.
*/
int romloader_jtag_openocd::initialize(void)
{
	int iResult;
	ROMLOADER_JTAG_DEVICE_T tDevice;
	char acResult[256];


	/* Initialize the device handle. */
	memset(&tDevice, 0, sizeof(ROMLOADER_JTAG_DEVICE_T));

	iResult = openocd_open(&tDevice);
	if( iResult==0 )
	{
		/* Run the version command. */
		iResult = tDevice.tFunctions.tFn.pfnCommandRunLine(tDevice.pvOpenocdContext, "version\n");
		if( iResult!=0 )
		{
			m_ptLog->fatal("Failed to run the version command!");
		}
		else
		{
			iResult = tDevice.tFunctions.tFn.pfnGetResult(tDevice.pvOpenocdContext, acResult, sizeof(acResult));
			if( iResult!=0 )
			{
				m_ptLog->fatal("Failed to get the result for the version command.");
			}
			else
			{
				m_ptLog->debug("OpenOCD version %s", acResult);
			}
		}

		openocd_close(&tDevice);
	}

	return iResult;
}


int romloader_jtag_openocd::setup_interface(ROMLOADER_JTAG_DEVICE_T *ptDevice, const char *pcInterfaceName, const char *pcInterfaceLocation)
{
	int iResult;
	char acCommand[256];


	/* Combine the command with the path. */
	snprintf(acCommand, sizeof(acCommand)-1, "setup_interface %s %s", pcInterfaceName, pcInterfaceLocation);

	/* Run the command chunk. */
	m_ptLog->debug("Run setup chunk for interface %s.", pcInterfaceName);
	iResult = ptDevice->tFunctions.tFn.pfnCommandRunLine(ptDevice->pvOpenocdContext, acCommand);
	if( iResult!=0 )
	{
		m_ptLog->debug("Failed to run the chunk: %d", iResult);
		m_ptLog->debug("Failed command: %s", acCommand);
	}

	return iResult;
}


/* Probe for a single interface. */
int romloader_jtag_openocd::probe_interface(ROMLOADER_JTAG_DEVICE_T *ptDevice, ROMLOADER_JTAG_SCAN_USB_RESULT_T *ptLocation)
{
	int iResult;
	int sizResult;
	char strResult[256];


	/* Try to setup the interface. */
	iResult = setup_interface(ptDevice, ptLocation->pcName, ptLocation->pcLocation);
	if( iResult!=0 )
	{
		/* This is no fatal error. It just means that this interface can not be used. */
		iResult = 1;
	}
	else
	{
		/* Run the probe chunk. */
		m_ptLog->debug("Run probe chunk for interface %s.", ptLocation->pcName);
		iResult = ptDevice->tFunctions.tFn.pfnCommandRunLine(ptDevice->pvOpenocdContext, "probe_interface");
		if( iResult!=0 )
		{
			/* This is no fatal error. It just means that this interface can not be used. */
			m_ptLog->debug("Failed to run the chunk: %d", iResult);
			iResult = 1;
		}
		else
		{
			iResult = ptDevice->tFunctions.tFn.pfnGetResult(ptDevice->pvOpenocdContext, strResult, sizeof(strResult));
			if( iResult!=0 )
			{
				/* This is a fatal error. */
				m_ptLog->error("Failed to get the result.");
				iResult = -1;
			}
			else
			{
				m_ptLog->debug("Result from probe: %s", strResult);
				iResult = strncmp(strResult, "OK", 3);
				if( iResult!=0 )
				{
					/* This is no fatal error. */
					iResult = 1;
				}
				else
				{
					m_ptLog->debug("Found interface %s!", ptLocation->pcName);
				}
			}
		}
	}

	return iResult;
}


/* Probe for a single interface/target combination. */
int romloader_jtag_openocd::probe_target(ROMLOADER_JTAG_DEVICE_T *ptDevice, const char *pcInterfaceName, const char *pcInterfaceLocation, const char *pcCpuName)
{
	int iResult;
	int sizResult;
	char strResult[256];
	char acTclCmd[256];


	iResult = setup_interface(ptDevice, pcInterfaceName, pcInterfaceLocation);
	if( iResult!=0 )
	{
		/* This is always a fatal error here as the interface has been used before. */
		iResult = -1;
	}
	else
	{
		m_ptLog->debug("Running detect code for CPU %s!", pcCpuName);
		snprintf(acTclCmd, sizeof(acTclCmd), "probe_cpu %s", pcCpuName);
		iResult = ptDevice->tFunctions.tFn.pfnCommandRunLine(ptDevice->pvOpenocdContext, acTclCmd);
		if( iResult!=0 )
		{
			/* This is no fatal error. It just means that this CPU is not present. */
			m_ptLog->debug("Failed to run the command chunk: %d", iResult);
			iResult = 1;
		}
		else
		{
			iResult = ptDevice->tFunctions.tFn.pfnGetResult(ptDevice->pvOpenocdContext, strResult, sizeof(strResult));
			if( iResult!=0 )
			{
				/* This is a fatal error. */
				m_ptLog->error("Failed to get the result for the code.");
				iResult = -1;
			}
			else
			{
				m_ptLog->debug("Result from detect: %s", strResult);
				iResult = strncmp(strResult, "OK", 3);
				if( iResult!=0 )
				{
					/* This is no fatal error. */
					iResult = 1;
				}
				else
				{
					m_ptLog->debug("Found target %s!", pcCpuName);
				}
			}
		}
	}

	return iResult;
}


/* Try all available targets with a given interface. */
int romloader_jtag_openocd::detect_target(ROMLOADER_JTAG_SCAN_USB_RESULT_T *ptLocation)
{
	int iResult;
	int sizResult;
	int iScanResult;
	char *pcNameStart;
	char *pcNameCnt;
	char *pcNameEnd;
	ROMLOADER_JTAG_DEVICE_T tDevice;
	char strResult[512];


	/* Get the number of known CPUs to iterate over. */

	/* Open the shared library. */
	iResult = openocd_open(&tDevice);
	if( iResult==0 )
	{
		iResult = tDevice.tFunctions.tFn.pfnCommandRunLine(tDevice.pvOpenocdContext, "get_known_cpus");
		if( iResult!=0 )
		{
			m_ptLog->error("Failed to run the 'get_known_cpus' command chunk: %d", iResult);
		}
		else
		{
			iResult = tDevice.tFunctions.tFn.pfnGetResult(tDevice.pvOpenocdContext, strResult, sizeof(strResult));
			if( iResult!=0 )
			{
				m_ptLog->error("Failed to get the result for the 'get_known_cpus' command.");
			}
			else
			{
				m_ptLog->debug("Known CPUs: %s", strResult);
			}
		}

		openocd_close(&tDevice);
	}

	/* Try to run all command chunks to see which CPU is there. */
	pcNameStart = strResult;
	pcNameEnd = strResult + strlen(strResult);
	while( pcNameStart<pcNameEnd )
	{
		/* Find the next comma or end of string. */
		pcNameCnt = pcNameStart;
		while( *pcNameCnt!=',' && *pcNameCnt!='\0' && pcNameCnt<pcNameEnd )
		{
			++pcNameCnt;
		}
		*(pcNameCnt++) = '\0';

		/* Open the shared library. */
		iResult = openocd_open(&tDevice);
		if( iResult!=0 )
		{
			/* This is a fatal error. */
			break;
		}
		else
		{
			m_ptLog->debug("Probe CPU %s", pcNameStart);
			iResult = probe_target(&tDevice, ptLocation->pcName, ptLocation->pcLocation, pcNameStart);

			openocd_close(&tDevice);
		}

		if( iResult==0 )
		{
			/* Found an entry! */
			iResult = add_detected_entry(ptLocation->pcName, pcNameStart, ptLocation->pcLocation);
			break;
		}
		else if( iResult<0 )
		{
			/* Fatal error! */
			break;
		}

		/* Move to the next CPU name. */
		pcNameStart = pcNameCnt;
	}

	return iResult;
}


int romloader_jtag_openocd::parse_scan_usb_result(const char *pcBuffer, ROMLOADER_JTAG_SCAN_USB_RESULT_T *ptEntries, size_t sizEntriesMax, size_t *psizEntries)
{
	const char *pcCnt;
	const char *pcName;
	size_t sizName;
	char c;
	int iResult;
	ROMLOADER_JTAG_SCAN_USB_RESULT_T *ptEntryCnt;
	ROMLOADER_JTAG_SCAN_USB_RESULT_T *ptEntryEnd;
	size_t sizEntries;
	int iScanResult;
	unsigned int uiIndex;
	unsigned int uiVID;
	unsigned int uiPID;
	int iConsumed;
	char acLocation[4 + USB_MAX_PATH_ELEMENTS * 4 + 1];


	pcCnt = pcBuffer;
	sizEntries = 0;
	ptEntryCnt = ptEntries;
	ptEntryEnd = ptEntries + sizEntriesMax;

	/* Be optimistic. */
	iResult = 0;

	/* Is this the end of the string? */
	while( *pcCnt!=0 )
	{
		/* Skip any whitespace. */
		while(*pcCnt==' ')
		{
			++pcCnt;
		}

		/* The first character must be an opening curly bracket. */
		if( *pcCnt!='{' )
		{
			m_ptLog->debug("Missing opening curly bracket for entry %d.", sizEntries);
			iResult = -1;
			break;
		}
		++pcCnt;

		/* Skip spaces. */
		while(*pcCnt==' ')
		{
			++pcCnt;
		}

		/* A string can be "as it is" or enclosed in curly brackets. */
		if( *pcCnt=='{' )
		{
			/* Skip the opening bracket. */
			++pcCnt;

			/* This is the start of the name. */
			pcName = pcCnt;

			/* The string is enclosed in curly brackets.
			* Get all characters except 0 and the closing bracket.
			*/
			while(1)
			{
				c = *pcCnt;
				if( c==0 || c=='}' )
				{
					break;
				}
				++pcCnt;
			}
			/* The string must be terminated with a closing curly bracket. */
			if( c!='}' )
			{
				m_ptLog->debug("Entry %d name is not terminated with a curly bracket.", sizEntries);
				iResult = -1;
				break;
			}
			++pcCnt;
		}
		else
		{
			/* The string is not enclosed in brackets. */

			/* This is the start of the name. */
			pcName = pcCnt;

			/* The string is enclosed in curly brackets.
			* Get all characters except 0, space and the closing bracket.
			*/
			while(1)
			{
				c = *pcCnt;
				if( c==0 || c==' ' || c=='}' )
				{
					break;
				}
				++pcCnt;
			}
		}
		/* Get the size of the name. */
		sizName = pcCnt - pcName;
		/* The string must not be empty. */
		if( sizName==0 )
		{
			m_ptLog->debug("The name of entry %d is empty.", sizEntries);
			iResult = -1;
			break;
		}
		/* The name must be separated with a space from the index. */
		if( *pcCnt!=' ' )
		{
			m_ptLog->debug("Expected space after the name of entry %d. %d %d", sizEntries, pcCnt-pcBuffer, *pcCnt);
			iResult = -1;
			break;
		}

		/* Parse a number.
			* Note that the "%n" does not increase the number of results.
			*/
		iScanResult = sscanf(pcCnt, " %x %x %s %u%n", &uiVID, &uiPID, acLocation, &uiIndex, &iConsumed);
		if( iScanResult!=4 )
		{
			m_ptLog->debug("Failed to parse the rest of entry %d. Parsed %d entries.", sizEntries, iScanResult);
			iResult = -1;
			break;
		}

		/* Skip the consumed chars. */
		pcCnt += iConsumed;

		/* Skip any trailing spaces. */
		while(*pcCnt==' ')
		{
			++pcCnt;
		}

		/* The entry must end with a closing curly bracket. */
		if( *pcCnt!='}' )
		{
			m_ptLog->debug("Entry %d is not closed by a curly bracket.", sizEntries);
			iResult = -1;
			break;
		}
		++pcCnt;

		ptEntryCnt->pcName = strndup(pcName, sizName);
		ptEntryCnt->pcLocation = strndup(acLocation, sizeof(acLocation));
		ptEntryCnt->uiIndex = uiIndex;
		ptEntryCnt->usVID = (uint16_t)(uiVID & 0xffffU);
		ptEntryCnt->usPID = (uint16_t)(uiPID & 0xffffU);
		printf("Found %s (%04X:%04X) at %s\n", ptEntryCnt->pcName, ptEntryCnt->usVID, ptEntryCnt->usPID, ptEntryCnt->pcLocation);

		/* Move to the next result entry. */
		++sizEntries;
		++ptEntryCnt;
		/* Stop after the result table is full. */
		if( ptEntryCnt>=ptEntryEnd )
		{
			break;
		}
	}

	if( iResult==0 && psizEntries!=NULL )
	{
		*psizEntries = sizEntries;
	}

	return iResult;
}


/*
   Detect interfaces/targets.
   Probe for all known interfaces. For each interface found, probe for all known targets.
   Returns a list of ROMLOADER_JTAG_DETECT_ENTRY_T in pptEntries/psizEntries.
 */
int romloader_jtag_openocd::detect(ROMLOADER_JTAG_DETECT_ENTRY_T **pptEntries, size_t *psizEntries)
{
	int iResult;
	size_t sizLocations;
	ssize_t ssizDevList;
	libusb_device **ptDeviceList;
	libusb_device **ptDevCnt, **ptDevEnd;
	libusb_device *ptDev;
	int iLibUsbResult;
	int iCnt;
	unsigned char ucUsbBusNumber;
	char *pcPathString;
	struct libusb_device_descriptor tDeviceDescriptor;
	ROMLOADER_JTAG_DEVICE_T tDevice;
	ROMLOADER_JTAG_SCAN_USB_RESULT_T *ptUsbScanResultCnt;
	ROMLOADER_JTAG_SCAN_USB_RESULT_T *ptUsbScanResultEnd;
	ROMLOADER_JTAG_SCAN_USB_RESULT_T atUsbScanResults[USB_MAX_DEVICES];


	/* Be pessimistic... */
	iResult = -1;

	/* Clear all locations. */
	memset(atUsbScanResults, 0, sizeof(atUsbScanResults));

	/* Clear any old results. */
	free_detect_entries();

	/* Get a list of all connected USB devices. */
	sizLocations = 0;
	ptDeviceList = NULL;
	ssizDevList = libusb_get_device_list(m_ptLibUsbContext, &ptDeviceList);
	if( ssizDevList<0 )
	{
		/* failed to detect devices */
		m_ptLog->fatal("romloader_jtag_openocd(%p): failed to detect usb devices: %ld:%s", this, ssizDevList, libusb_strerror((libusb_error)ssizDevList));
		iResult = -1;
	}
	else
	{
		/* Create a TCL table with all USB devices in the form VID, PID, location.
		 * A row of the table has the format "{VID PID location}".
		 * VID and PID are hexadecimal numbers with 4 digits each.
		 * "location" is a list of the bus number with all ports separated by colon and commas. The maximum size is 4 + USB_MAX_PATH_ELEMENTS * 4 .
		 * The maximum size of one table row is 1 + 4 + 1 + 4 + 1 + 4 + USB_MAX_PATH_ELEMENTS * 4 + 1 = 16 + USB_MAX_PATH_ELEMENTS * 4 .
		 * Each table row must be separated by a space from the next one: {row1 row2 row3}
		 * The command starts with the function name "scan_usb".
		 * This results in a maximum string length of 8 + 1 + 1 + (16 + USB_MAX_PATH_ELEMENTS * 4 + 1) * USB_MAX_DEVICES + 1.
		 */
		char acTclBuf[8 + 1 + 1 + (16 + USB_MAX_PATH_ELEMENTS * 4 + 1) * USB_MAX_DEVICES + 1];
		unsigned char aucUsbPortNumbers[USB_MAX_PATH_ELEMENTS];
		char *pcCnt;
		char *pcEnd;
		size_t sizTableEntries;
		size_t sizChunk;

		pcCnt = acTclBuf;
		pcEnd = acTclBuf + sizeof(acTclBuf);

		/* Start the "scan_usb" command. */
		sizChunk = sprintf(pcCnt, "scan_usb {");
		pcCnt += sizChunk;

		/* Loop over all detected USB devices and add their VID/PID and location to the TCL table. */
		sizTableEntries = 0;
		ptDevCnt = ptDeviceList;
		ptDevEnd = ptDevCnt + ssizDevList;
		while( ptDevCnt<ptDevEnd )
		{
			ptDev = *ptDevCnt;

			iLibUsbResult = libusb_get_device_descriptor(ptDev, &tDeviceDescriptor);
			if( iLibUsbResult==0 )
			{
				iLibUsbResult = libusb_get_port_numbers(ptDev, aucUsbPortNumbers, sizeof(aucUsbPortNumbers));
				if( iLibUsbResult>0 )
				{
					/* Set the first 2 elements of the new table row. */
					sizChunk = sprintf(pcCnt, "{%04X %04X ", tDeviceDescriptor.idVendor, tDeviceDescriptor.idProduct);
					pcCnt += sizChunk;

					/* Get the bus number. */
					ucUsbBusNumber = libusb_get_bus_number(ptDev);

					/* Build the path string. */
					pcPathString = pcCnt;
					sizChunk = sprintf(pcCnt, "%d:", ucUsbBusNumber);
					pcCnt += sizChunk;
					for(iCnt=0; iCnt<iLibUsbResult; ++iCnt)
					{
						sizChunk = sprintf(pcCnt, "%d,", aucUsbPortNumbers[iCnt]);
						pcCnt += sizChunk;
					}
					/* pcCnt points to the terminating 0 now. Remove the last ":" or ",". */
					--pcCnt;
					*pcCnt = 0;

					/* Close the table row. */
					sizChunk = sprintf(pcCnt, "} ");
					pcCnt += sizChunk;

					++sizTableEntries;
				}
			}

			++ptDevCnt;
		}

		libusb_free_device_list(ptDeviceList, 1);

		/* If there is at least 1 table entry, there is one trailing space at the end. Remove it. */
		if( sizTableEntries>0 )
		{
			--pcCnt;
			*pcCnt = 0;
		}
		/* Close the TCL table and add a newline to terminate the command. */
		sprintf(pcCnt, "}\n");

		/* Open the shared library. */
		iResult = openocd_open(&tDevice);
		if( iResult!=0 )
		{
			/* This is a fatal error. */
			iResult = -1;
		}
		else
		{
			/* Run the "scan_usb" command. */
			iResult = tDevice.tFunctions.tFn.pfnCommandRunLine(tDevice.pvOpenocdContext, acTclBuf);
			if( iResult!=0 )
			{
				m_ptLog->debug("Failed to run the scan_usb command: %d", iResult);
				m_ptLog->debug("Failed command line: %s", acTclBuf);
			}
			else
			{
				/* Get the result of the command. */
				iResult = tDevice.tFunctions.tFn.pfnGetResult(tDevice.pvOpenocdContext, acTclBuf, sizeof(acTclBuf));
				if( iResult!=0 )
				{
					m_ptLog->fatal("Failed to get the result for the scan_usb command.");
				}
			}

			/* Close the shared library. */
			openocd_close(&tDevice);

			if( iResult==0 )
			{
				m_ptLog->debug("scan_usb Result: %s", acTclBuf);

				/* TODO: parse the result.
				 * There should be an empty string for no results, or a table in the form
				 *   "{NXJTAG-4000-USB 7} {NXJTAG-USB 0}"
				 * Entries with spaces are enclosed in curly brackets:
				 *   "{{NXJTAG -4000-USB} 7}"
				 */
				iResult = parse_scan_usb_result(acTclBuf, atUsbScanResults, sizeof(atUsbScanResults)/sizeof(atUsbScanResults[0]), &sizLocations);
				if( iResult!=0 )
				{
					m_ptLog->fatal("Failed to parse the result of scan_usb.");
				}

				/* Initialize the result array. */
				m_sizDetectedCnt = 0;
				m_sizDetectedMax = sizLocations;

				m_ptDetected = (ROMLOADER_JTAG_DETECT_ENTRY_T*)malloc(m_sizDetectedMax*sizeof(ROMLOADER_JTAG_DETECT_ENTRY_T));
				if( m_ptDetected==NULL )
				{
					m_ptLog->fatal("Failed to allocate %zd bytes of memory for the detection results!", m_sizDetectedMax*sizeof(ROMLOADER_JTAG_DETECT_ENTRY_T));
				}
				else
				{
					/* Run the command chunks of all locations to see which interfaces are present. */
					ptUsbScanResultCnt = atUsbScanResults;
					ptUsbScanResultEnd = atUsbScanResults + sizLocations;
					while( ptUsbScanResultCnt<ptUsbScanResultEnd )
					{
						m_ptLog->debug("Detecting interface '%s' on path %s.", ptUsbScanResultCnt->pcName, ptUsbScanResultCnt->pcLocation);

						/* Open the shared library. */
						iResult = openocd_open(&tDevice);
						if( iResult!=0 )
						{
							/* This is a fatal error. */
							iResult = -1;
							break;
						}
						else
						{
							/* Detect the interface. */
							iResult = probe_interface(&tDevice, ptUsbScanResultCnt);

							/* Clean up after the detection. */
							openocd_close(&tDevice);

							if( iResult==0 )
							{
								/* Detect the CPU on this interface. */
								iResult = detect_target(ptUsbScanResultCnt);
							}

							/* Ignore non-fatal errors.
							* They indicate that the current interface could not be detected.
							*/
							if( iResult>0 )
							{
								iResult = 0;
							}
							/* Do not continue with other interfaces if a fatal error occurred. */
							else if( iResult<0 )
							{
								/* This is a fatal error. */
								break;
							}
						}

						/* Move to the next location. */
						++ptUsbScanResultCnt;
					}
				}
			}
		}
	}

	/* Discard all results if something really bad happened (like a fatal error). */
	if( iResult<0 )
	{
		free_detect_entries();
	}

	/* Return the result list. */
	*pptEntries = m_ptDetected;
	*psizEntries = m_sizDetectedCnt;

	return iResult;
}


/* Open a connection to a given target on a given interface. */
/* Open the connection to the device. */
int romloader_jtag_openocd::connect(const char *pcInterfaceName, const char *pcTargetName, const char *pcLocation)
{
	int iResult;

	if( fJtagDeviceIsConnected==true )
	{
		m_ptLog->error("romloader_jtag_openocd::connect: Already connected.");
	}
	else
	{
		/* Open the shared library. */
		iResult = openocd_open(&m_tJtagDevice);
		if( iResult==0 )
		{
			iResult = probe_target(&m_tJtagDevice, pcInterfaceName, pcLocation, pcTargetName);
			if( iResult==0 )
			{
				/* Stop the target. */
				m_ptLog->debug("Running reset code.");
				iResult = m_tJtagDevice.tFunctions.tFn.pfnCommandRunLine(m_tJtagDevice.pvOpenocdContext, "reset_board");
				if( iResult!=0 )
				{
					m_ptLog->error("Failed to run the reset code: %d", iResult);
					iResult = -1;
				}
				else
				{
					fJtagDeviceIsConnected = true;
				}
			}
		}
	}

	return iResult;
}


/*
   Initialize the chip and prepare it for running code.
   (the equivalent of what the 'run' section did in the old OpenOCD configs)
 */

int romloader_jtag_openocd::init_chip(ROMLOADER_CHIPTYP tChiptyp)
{
	int iResult;
	char strCmd[32];

	m_ptLog->debug("Loading chip init script.");
	iResult = m_tJtagDevice.tFunctions.tFn.pfnCommandRunLine(m_tJtagDevice.pvOpenocdContext, "source [find chip_init.tcl]");
	if( iResult!=0 )
	{
		m_ptLog->fatal("Failed to load the chip init script: %d", iResult);
		iResult = -1;
	}
	else
	{
		m_ptLog->debug("Running init_chip script.");
		memset(strCmd, 0, sizeof(strCmd));
		snprintf(strCmd, sizeof(strCmd)-1, "init_chip %d", tChiptyp);
		iResult = m_tJtagDevice.tFunctions.tFn.pfnCommandRunLine(m_tJtagDevice.pvOpenocdContext, strCmd);
		if( iResult!=0 )
		{
			m_ptLog->debug("Failed to run the init_chip script: %d", iResult);
			iResult = -1;
		}
		else
		{
			m_ptLog->debug("Chip init complete.");
			iResult = 0;
		}
	}

	return iResult;
}


/* Close the connection to the device. */
void romloader_jtag_openocd::disconnect(void)
{
	openocd_close(&m_tJtagDevice);
	fJtagDeviceIsConnected = false;
}



/* Read a byte (8bit) from the netX. */
int romloader_jtag_openocd::read_data08(uint32_t ulNetxAddress, uint8_t *pucData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnReadData08!=NULL )
	{
		/* Read memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnReadData08(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pucData);
		if( iResult!=0 )
		{
			m_ptLog->error("read_data08: Failed to read address 0x%08x: %d", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Read a word (16bit) from the netX. */
int romloader_jtag_openocd::read_data16(uint32_t ulNetxAddress, uint16_t *pusData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnReadData16!=NULL )
	{
		/* Read memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnReadData16(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pusData);
		if( iResult!=0 )
		{
			m_ptLog->error("read_data16: Failed to read address 0x%08x: %d", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Read a long (32bit) from the netX. */
int romloader_jtag_openocd::read_data32(uint32_t ulNetxAddress, uint32_t *pulData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnReadData32!=NULL )
	{
		/* Read memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnReadData32(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pulData);
		if( iResult!=0 )
		{
			m_ptLog->error("read_data32: Failed to read address 0x%08x: %d", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Read a byte array from the netX. */
int romloader_jtag_openocd::read_image(uint32_t ulNetxAddress, uint8_t *pucData, uint32_t sizData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnReadImage!=NULL )
	{
		/* Read memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnReadImage(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pucData, sizData);
		if( iResult!=0 )
		{
			m_ptLog->error("read_image: Failed to read address 0x%08x: %d", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Write a byte (8bit) to the netX. */
int romloader_jtag_openocd::write_data08(uint32_t ulNetxAddress, uint8_t ucData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnWriteData08!=NULL )
	{
		/* Write memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnWriteData08(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, ucData);
		if( iResult!=0 )
		{
			m_ptLog->error("write_data08: Failed to write address 0x%08x: %d", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Write a word (16bit) to the netX. */
int romloader_jtag_openocd::write_data16(uint32_t ulNetxAddress, uint16_t usData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnWriteData16!=NULL )
	{
		/* Write memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnWriteData16(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, usData);
		if( iResult!=0 )
		{
			m_ptLog->error("write_data16: Failed to write address 0x%08x: %d", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Write a long (32bit) to the netX. */
int romloader_jtag_openocd::write_data32(uint32_t ulNetxAddress, uint32_t ulData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnWriteData32!=NULL )
	{
		/* Write memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnWriteData32(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, ulData);
		if( iResult!=0 )
		{
			m_ptLog->error("write_data32: Failed to write address 0x%08x: %d", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Write a byte array to the netX. */
int romloader_jtag_openocd::write_image(uint32_t ulNetxAddress, const uint8_t *pucData, uint32_t sizData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnWriteImage!=NULL )
	{
		/* Write memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnWriteImage(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pucData, sizData);
		if( iResult!=0 )
		{
			m_ptLog->error("write_image: Failed to write address 0x%08x: %d", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* int muhkuh_openocd_call(void *pvContext, uint32_t ulNetxAddress, uint32_t ulR0, PFN_MUHKUH_CALL_PRINT_CALLBACK pfnCallback, void *pvCallbackUserData) */
int romloader_jtag_openocd::call(uint32_t ulNetxAddress, uint32_t ulParameterR0, PFN_MUHKUH_CALL_PRINT_CALLBACK pfnCallback, void *pvCallbackUserData)
{
	int iResult;

	/* Be pessimistic. */
	iResult = -1;


	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnCall!=NULL )
	{
		/* Call code on netX. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnCall(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, ulParameterR0, pfnCallback, pvCallbackUserData);
		if( iResult!=0 )
		{
			m_ptLog->error("call: Failed to call code at address 0x%08x: %d", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



uint32_t romloader_jtag_openocd::get_image_chunk_size(void)
{
	/* This is the suggested size of a data chunk before executing the LUA callback. */
	/* TODO: Is a fixed number OK or should this depend on some parameters like the JTAG speed?
	 *       Maybe this could also be one fixed option for every interface/target pair - just like the reset code.
	 */
	return 1024;
}



void romloader_jtag_openocd::openocd_output_handler(void *pvUser, const char *pcLine, size_t sizLine)
{
	romloader_jtag_openocd *ptThis;


	ptThis = (romloader_jtag_openocd*)pvUser;
	ptThis->local_openocd_output_handler(pcLine, sizLine);
}



void romloader_jtag_openocd::local_openocd_output_handler(const char *pcLine, size_t sizLine)
{
	char acFormat[16];


	snprintf(acFormat, sizeof(acFormat), "%%.%zds", sizLine);
	m_ptLog->debug(acFormat, pcLine);
}
