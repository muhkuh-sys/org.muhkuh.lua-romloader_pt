/***************************************************************************
 *   Copyright (C) 2007 by Christoph Thelen                                *
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


#include <wx/xml/xml.h>

#ifndef __muhkuh_wrap_xml_h__
#define __muhkuh_wrap_xml_h__


typedef struct
{
	wxString strName;
	wxString strVersion;			// version string
} tTesterXml_Test;

typedef struct
{
	wxString strName;			// name of the testdescription
	wxString strVersion;			// version string
	unsigned int uiTests;			// number of tests
	tTesterXml_Test *ptTests;
} tTesterXml_TestDescription;


class muhkuh_wrap_xml
{
public:
	muhkuh_wrap_xml(void);
	~muhkuh_wrap_xml(void);

	bool initialize(wxInputStream *pStream, wxString strMtdPath);

	wxString getMtdPath(void) const;
	wxXmlDocument *getXmlDocument(void);

	wxString testDescription_getName(void) const;
	wxString testDescription_getVersion(void) const;
	unsigned int testDescription_getTestCnt(void) const;
	bool testDescription_setTest(unsigned int uiTestIdx);

	wxString test_getName(void) const;
	wxString test_getVersion(void) const;

private:
	bool readTestDescription(wxXmlDocument *xmldoc);
	bool readTest(wxXmlNode *xml_parent, tTesterXml_TestDescription *ptTestDesc);

	wxXmlDocument *m_xml_doc;

	tTesterXml_TestDescription *m_ptTestDescription;
	tTesterXml_Test *m_ptTest;

	// path to the mtd directory, includes any vfs prefixes like "#zip:"
	wxString m_strMtdPath;
};


#endif	// __muhkuh_wrap_xml_h__
