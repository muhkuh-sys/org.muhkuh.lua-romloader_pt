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


#include "muhkuh_capture_std.h"

#include <wx/txtstrm.h>


capture_std::capture_std(wxString strCommand, wxProcess *ptProcess)
 : wxThread()
 , m_strCommand(strCommand)
 , m_ptProcess(ptProcess)
{
}

void *capture_std::Entry(void)
{
	wxInputStream *ptStreamError;
	wxInputStream *ptStreamInput;
	long lPid;
	wxString strInput;
	bool fGotInput;
	unsigned int uiCnt;


	/* Redirect stdin, stdout and stderr. */
	m_ptProcess->Redirect();

	/* Get stdin and stderr. */
	ptStreamError = m_ptProcess->GetErrorStream();
	if( ptStreamError==NULL )
	{
		wxLogError(wxT("Failed to get error stream!"));
	}
	else
	{
		ptStreamInput = m_ptProcess->GetInputStream();
		if( ptStreamInput==NULL )
		{
			wxLogError(wxT("Failed to get input stream!"));
		}
		else
		{
			wxTextInputStream tTextStreamError(*ptStreamError);
			wxTextInputStream tTextStreamInput(*ptStreamInput);

			/* Execute the command. */
			lPid = wxExecute(m_strCommand, wxEXEC_ASYNC, m_ptProcess);
			if( lPid==0 )
			{
				wxLogError(wxT("Failed to execute the command!"));
			}
			else
			{
				do
				{
					fGotInput = false;

					/* Check the stderr stream for input. */
					uiCnt = m_uiMaxLinesPerLoop;
					if( uiCnt>0 && m_ptProcess->IsErrorAvailable()==true )
					{
						wxLogError(tTextStreamError.ReadLine());
						fGotInput = true;
						--uiCnt;
					}

					/* Check the stdin stream for input. */
					uiCnt = m_uiMaxLinesPerLoop;
					while( uiCnt>0 && m_ptProcess->IsInputAvailable()==true )
					{
						wxLogMessage(tTextStreamInput.ReadLine());
						fGotInput = true;
						--uiCnt;
					}

					/* Processed Input in this round? */
					if( fGotInput==false )
					{
						/* No -> give the rest of the timeslice to the other processes. */
						Yield();
					}
				} while( TestDestroy()==false );
			}
		}
	}

	return NULL;
}

