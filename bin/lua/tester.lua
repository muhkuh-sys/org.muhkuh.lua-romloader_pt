
module("tester", package.seeall)

require("select_plugin")

-- prevent images from being gcd
m_iconList = nil


local m_panel = nil
local m_htmlWindow = nil
local m_easyPrint = nil

local m_strTestName = nil
local m_deviceNr = nil
local m_boardCnt = nil
local m_testCnt = nil
local m_testNames = nil
local m_testIconView = nil
local m_spinBoardNr = nil
local m_testCombo = nil
local m_saveReportDlg = nil

local m_testResults = nil

local m_strTestReport = nil

-- prevent updates from eventhandlers
local m_lockUpdates = true

-- the common plugin
local m_commonPlugin = nil

-- the current mode
local eMODE_IDLE	= 0
local eMODE_SINGLE_TEST	= 1
local eMODE_BOARD_TEST	= 2
local m_mode = eMODE_IDLE


-- generate window ids
local m_ID = wx.wxID_HIGHEST
function nextID()
	m_ID = m_ID+1
	return m_ID
end

local ID_SPIN_BOARDNR			= nextID()
local ID_COMBO_TESTS			= nextID()
local ID_LISTCTRL_ICONVIEW		= nextID()
local ID_BUTTON_TESTBOARD		= nextID()
local ID_BUTTON_SAVEREPORT		= nextID()
local ID_BUTTON_QUIT			= nextID()

local TEST_STATUS_NotCompleted		= 0
local TEST_STATUS_Ok			= 1
local TEST_STATUS_Failed		= 2
local TEST_STATUS_FatalError		= 3

_G.__MUHKUH_TEST_RESULT_OK		= 1
_G.__MUHKUH_TEST_RESULT_CANCEL		= 2
_G.__MUHKUH_TEST_RESULT_FAIL		= -1
_G.__MUHKUH_TEST_RESULT_FATALERROR	= -2

---------------------------------------

muhkuh_test_untested_32_xpm = {
"32 32 16 1",
"       c None",
".      c #121212",
"+      c #242424",
"@      c #373737",
"#      c #000000",
"$      c #494949",
"%      c #6D6D6D",
"&      c #808080",
"*      c #5B5B5B",
"=      c #A4A4A4",
"-      c #EDEDED",
";      c #FFFFFF",
">      c #C8C8C8",
",      c #DBDBDB",
"'      c #B6B6B6",
")      c #929292",
"                .               ",
"          .+   @@       #       ",
"          +$+%&$+     +*=%      ",
"          .$-;;;=.  .&&>>>#     ",
"          .,;;;;;,%%'>')*.      ",
"      ##.@';;;;;;;%##           ",
"   $%%'%+@))=)=;')=)            ",
"  *&>'+  =;;;;,&==>;)           ",
"  )>)#  ,;;;=';%,%=;&           ",
"  =)#  %;;;;&=;))==*            ",
"  +#   $;;;;;-&-;--'            ",
"        $='=)=-;;;;;+           ",
"          #';;;;;;;;&           ",
"           >;;;;;;;;-@          ",
"          #-;;;;;;;;;>.         ",
"          $;;;;;;;;;;;'.        ",
"          =;;;;;;;;;;,&+@$$.    ",
"         +,-;;;;;;;>**=,>**)$   ",
"       .@*%*$*)>>)$*>,,,>@%@'#  ",
"     +=,,,,,,>=&&=,,,,,,,>)=,@  ",
"    +>&@@),,,,,,,,,,,,,,,,,,,$  ",
"    &=$=)*,,,,,,,,,,,,,,,,,,,@  ",
"    =)$%@',,,,,,,,,,,,,,,,,,>#  ",
"   #',=)>,,,,,,,,,,,,,,,,,,,$   ",
"    ',,,,,,,,,,,,,,,,,,,,,,%    ",
"    %,,,,,,,,,,,,,,,,>>'=)@     ",
"    .',,,,,,,,,>&$$$$**%&&+     ",
"     .%>,,,,=%$*)>,,,,,,,,$     ",
"       #+@@$%',,,,>'))&%%$      ",
"          &,,,>&$$*&=*          ",
"          $)%$*=-;;;;'          ",
"           $>;;;;;;;;-.         "}


muhkuh_test_ok_32_xpm = {
"32 32 23 1",
"       c None",
".      c #245B24",
"+      c #122412",
"@      c #244924",
"#      c #498049",
"$      c #5BB65B",
"%      c #000000",
"&      c #80FF80",
"*      c #375B37",
"=      c #376D37",
"-      c #378037",
";      c #243724",
">      c #5BA45B",
",      c #49A449",
"'      c #6DDB6D",
")      c #001200",
"!      c #499249",
"~      c #6DC86D",
"{      c #6DED6D",
"]      c #5BC85B",
"^      c #123712",
"/      c #80ED80",
"(      c #121212",
"                                ",
"             .+  +@             ",
"             +#$$.+             ",
"            %$&&&&*    %@=.     ",
"     @==-=+.$&&&&&&=+;#>,,')    ",
"    *!~,@) +{&&&&&&'@*=!!!.     ",
"    >'=%   +!,$&&],,=           ",
"    -^     {&,$]{>>~&-          ",
"          *&&@>${$#$&=          ",
"           =>,,&&$,,.           ",
"           %~&&&&&&&@           ",
"           )&&&&&&&&=           ",
"           @&&&&&&&&>           ",
"           >&&&&&&&&/+          ",
"          @&&&&&&&&&&!          ",
"         ('&&&&&&&&&&&;         ",
"     %@*==..!/&&&&&&',.%%       ",
"    ($$#>''',.=>$,=@=,$]!#;     ",
"    !!@-@>'''']!!,]''''$;@#^    ",
"   ('.!!@$''''''''''''''-@=#    ",
"   ;'$*=$''''''''''''''''''!    ",
"   @'''''''''''''''''''''''#    ",
"   +'''''''''''''''''''''''@    ",
"    ='''''''''''''''''''''!     ",
"     *~''''''''~]~'''''''#%     ",
"      )@-#-*@.;.-+*.;.*=^       ",
"         )=,.&>$&*&{@$=         ",
"        +,>!=.@@.@.*-!!*        ",
"        ;''''''''''''''$        ",
"         +=>'''''''''>=(        ",
"           ^*.......*#%         ",
"           !&&&&&&&&&&^         "}


muhkuh_test_failed_32_xpm = {
"32 32 23 1",
"       c None",
".      c #5B2424",
"+      c #241212",
"@      c #121212",
"#      c #492424",
"$      c #371212",
"%      c #924949",
"&      c #372424",
"*      c #000000",
"=      c #FF8080",
"-      c #5B3737",
";      c #B65B5B",
">      c #DB6D6D",
",      c #C85B5B",
"'      c #6D3737",
")      c #A44949",
"!      c #ED6D6D",
"~      c #804949",
"{      c #ED8080",
"]      c #C86D6D",
"^      c #803737",
"/      c #A45B5B",
"(      c #120000",
"                                ",
"             .+  @#             ",
"            @$.%%#&             ",
"            *%====-             ",
"         *+.;======.            ",
"       +%>%&%%,====>'.$*        ",
"      -%='@>==)!==~;#$;,'#      ",
"     #~{' %==={)=;>={  #])$     ",
"     )>.  ,==^-%=%^#=-  *..     ",
"     ^#   ,==>,/=;;>=#          ",
"          %===)!==%,^           ",
"           )>);====]~           ",
"           #;!======,           ",
"           %========{@          ",
"          +{========='          ",
"          /==========!@         ",
"         .{===========%         ",
"     +.'''#./======={;%(        ",
"    ^>)%]>>]~#~,>,%.#^);/~.*    ",
"   #]#-'#>>>>>/''^)>>>>>.##%*   ",
"   /%')~#>>>>>>>>>>>>>>>%&#%$   ",
"  (>]##-,>>>>>>>>>>>>>>>>];>.   ",
"  +>>>>>>>>>>>>>>>>>>>>>>>>>#   ",
"  (>>>>>>>>>>>>>>>>>>>>>>>>>+   ",
"   ^>>>>>>>>>>>>>>>>>>>>>>>%    ",
"   *%>>>>>>>>>>>>>>>>>>>>>/(    ",
"     #)>>>>];%^'''~);]>>]^(     ",
"       (+###'~);;;)~'.#&(       ",
"       *;>>>>>>>>>>>>>>>&       ",
"       *,>>,%-####-%;>>>$       ",
"        +#++)>====>/'##&        ",
"           '========={(         "}


---------------------------------------

local commonPlugin = false

function getCommonPlugin()
	if not commonPlugin then
		commonPlugin = select_plugin.SelectPlugin("romloader*.")
		if commonPlugin then
			commonPlugin:connect()
		end
	end
	return commonPlugin
end

function closeCommonPlugin()
	if commonPlugin then
		commonPlugin:disconnect()
		commonPlugin:delete()
		commonPlugin = false
	end
end

---------------------------------------

function getPanel()
	return m_panel
end


local m_stdWriteMax = 0
m_stdWriteProgressDialog = nil

function stdWriteCallback(ulProgress, ulCallbackId)
	local fIsRunning


	if m_stdWriteProgressDialog==nil then
		fIsRunning = false
	else
		fIsRunning = m_stdWriteProgressDialog:Update(ulProgress, "writing...")
	end

	return fIsRunning
end

local function stdWriteCloseProgress()
	if m_stdWriteProgressDialog~=nil then
		m_stdWriteProgressDialog:Close()
		m_stdWriteProgressDialog:Destroy()
		m_stdWriteProgressDialog = nil
	end
end

function stdWrite(parent, plugin, ulNetxAddress, strData)
	m_stdWriteMax = string.len(strData)

	m_stdWriteProgressDialog = wx.wxProgressDialog(	"Downloading...",
							"",
							m_stdWriteMax,
							parent,
							wx.wxPD_AUTO_HIDE+wx.wxPD_CAN_ABORT+wx.wxPD_ESTIMATED_TIME+wx.wxPD_REMAINING_TIME+wx.wxPD_ELAPSED_TIME)

	plugin:write_image(ulNetxAddress, strData, tester.stdWriteCallback, 0)

	stdWriteCloseProgress()
end


local m_stdReadMax = 0
m_stdReadProgressDialog = nil

function stdReadCallback(ulProgress, ulCallbackId)
	local fIsRunning


	if m_stdReadProgressDialog==nil then
		fIsRunning = false
	else
		fIsRunning = m_stdReadProgressDialog:Update(ulProgress, "reading...")
	end

	return fIsRunning
end

local function stdReadCloseProgress()
	if m_stdReadProgressDialog~=nil then
		m_stdReadProgressDialog:Close()
		m_stdReadProgressDialog:Destroy()
		m_stdReadProgressDialog = nil
	end
end

function stdRead(parent, plugin, ulNetxAddress, ulLength)
	local strData
	m_stdReadMax = ulLength

	m_stdReadProgressDialog = wx.wxProgressDialog(	"Uploading...",
							"",
							m_stdReadMax,
							parent,
							wx.wxPD_AUTO_HIDE+wx.wxPD_CAN_ABORT+wx.wxPD_ESTIMATED_TIME+wx.wxPD_REMAINING_TIME+wx.wxPD_ELAPSED_TIME)

	strData = plugin:read_image(ulNetxAddress, ulLength, tester.stdReadCallback, 0)

	stdReadCloseProgress()

	return strData
end


m_stdCallProgressDialog = nil

function stdCallCallback(ulProgress, ulCallbackId)
	local fIsRunning
	local strMsg


	if m_stdCallProgressDialog==nil then
		fIsRunning = false
	else
		if ulProgress==0 then
			strMsg = "executing function..."
		else
			strMsg = string.format("read %d bytes", ulProgress)
		end
		-- NOTE: wxLua does not bind the "Pulse" method yet :(
		fIsRunning = m_stdCallProgressDialog:Update(0,strMsg)
	end

	return fIsRunning
end

local function stdCallCloseProgress()
	if m_stdCallProgressDialog~=nil then
		m_stdCallProgressDialog:Close()
		m_stdCallProgressDialog:Destroy()
		m_stdCallProgressDialog = nil
	end
end

function stdCall(parent, plugin, ulNetxAddress, ulParameterR0)
	m_stdCallProgressDialog = wx.wxProgressDialog(	"Executing function...",
							"",
							100,
							parent,
							wx.wxPD_AUTO_HIDE+wx.wxPD_CAN_ABORT)
	plugin:call(ulNetxAddress, ulParameterR0, tester.stdCallCallback, 0)
	stdCallCloseProgress()
end

---------------------------------------

local function changeMode(eNewMode)
	-- leave old mode
	if m_mode==eMODE_IDLE then
		
	elseif m_mode==eMODE_SINGLE_TEST then
		
	elseif m_mode==eMODE_BOARD_TEST then
		
	end

	-- accept new mode
	m_mode = eNewMode

	-- enter new mode
	if m_mode==eMODE_IDLE then
		-- enable board choice
		m_spinBoardNr:Enable(true)
		m_testCombo:Enable(true)
		m_testIconView:Enable(true)
		
	elseif m_mode==eMODE_SINGLE_TEST then
		-- disable board choice
		m_spinBoardNr:Enable(true)
		m_testCombo:Enable(true)
		m_testIconView:Enable(true)
		
	elseif m_mode==eMODE_BOARD_TEST then
		-- disable board choice
		m_spinBoardNr:Enable(true)
		m_testCombo:Enable(true)
		m_testIconView:Enable(true)
		
	end
end


local function createControls()
	local style = nil
	local size = nil
	local mainSizer = nil
	local controlSizer = nil
	local infoSizer = nil
	local devNoLabel = nil
	local devNoText = nil
	local boardLabel = nil
	local boardSizer = nil
	local boardText = nil
	local testLabel = nil
	local buttonSizer = nil
	local buttonRunTest = nil
	local buttonSaveReport = nil
	local buttonQuit = nil


	-- the save report dialog
	m_saveReportDlg = wx.wxFileDialog(m_panel, "Choose a file", "", "report.html", "Html Files (*.htm;*.html)|*.htm;*.html|All Files (*.*)|*.*", wx.wxFD_SAVE+wx.wxFD_OVERWRITE_PROMPT)

	-- the main sizer
	mainSizer = wx.wxBoxSizer(wx.wxVERTICAL)
	m_panel:SetSizer(mainSizer)

	-- set sizer for both windows
	controlSizer = wx.wxStaticBoxSizer(wx.wxVERTICAL, m_panel, m_strTestName)
	mainSizer:Add(controlSizer, 0, wx.wxEXPAND)

	-- create the info sizer
	infoSizer = wx.wxFlexGridSizer(3, 2, 4, 4)
	infoSizer:AddGrowableCol(1)
	controlSizer:Add(infoSizer, 0, wx.wxEXPAND)
	-- device number
	devNoLabel = wx.wxStaticText(m_panel, wx.wxID_ANY, "Device Nr")
	infoSizer:Add(devNoLabel, 0, wx.wxALIGN_CENTER_VERTICAL)
	if m_deviceNr==nil then
		devNoText  = wx.wxStaticText(m_panel, wx.wxID_ANY, "none")
	else
		devNoText  = wx.wxStaticText(m_panel, wx.wxID_ANY, m_deviceNr)
	end
	infoSizer:Add(devNoText, 0, wx.wxALIGN_CENTER_VERTICAL)
	-- board number
	boardLabel = wx.wxStaticText(m_panel, wx.wxID_ANY, "Board")
	infoSizer:Add(boardLabel, 0, wx.wxALIGN_CENTER_VERTICAL)
	boardSizer = wx.wxBoxSizer(wx.wxHORIZONTAL)
	m_spinBoardNr = wx.wxSpinCtrl(m_panel, ID_SPIN_BOARDNR, "1", wx.wxDefaultPosition, wx.wxDefaultSize, wx.wxSP_ARROW_KEYS, 1, m_boardCnt, 1)
	boardSizer:Add(m_spinBoardNr, 0, wx.wxEXPAND)
	boardText  = wx.wxStaticText(m_panel, wx.wxID_ANY, " of "..m_boardCnt)
	boardSizer:Add(boardText, 0, wx.wxALIGN_CENTER_VERTICAL)
	infoSizer:Add(boardSizer, 0, wx.wxEXPAND)
	-- test selector
	testLabel  = wx.wxStaticText(m_panel, wx.wxID_ANY, "Test")
	infoSizer:Add(testLabel, 0, wx.wxALIGN_CENTER_VERTICAL)
	m_testCombo   = wx.wxComboBox(m_panel, ID_COMBO_TESTS, "", wx.wxDefaultPosition, wx.wxDefaultSize, m_testNames, wx.wxCB_DROPDOWN+wx.wxCB_READONLY)
	m_testCombo:Select(0)
	infoSizer:Add(m_testCombo, 0, wx.wxEXPAND)

	-- add the icon list
	m_iconList = wx.wxImageList(32, 32, true, 3)
	m_iconList:Add(wx.wxBitmap(muhkuh_test_untested_32_xpm))
	m_iconList:Add(wx.wxBitmap(muhkuh_test_ok_32_xpm))
	m_iconList:Add(wx.wxBitmap(muhkuh_test_failed_32_xpm))

	m_testIconView = wx.wxListCtrl(m_panel, ID_LISTCTRL_ICONVIEW, wx.wxDefaultPosition, wx.wxDefaultSize, wx.wxLC_ICON+wx.wxLC_ALIGN_LEFT+wx.wxLC_SINGLE_SEL+wx.wxBORDER_SUNKEN)
	m_testIconView:SetImageList(m_iconList, wx.wxIMAGE_LIST_NORMAL)
	item = wx.wxListItem()
	for i=0,m_testCnt-1 do
		item:Clear()
		item:SetMask(wx.wxLIST_MASK_IMAGE+wx.wxLIST_MASK_DATA)
		item:SetImage(0)
		item:SetId(i)
		item:SetData(i)
		m_testIconView:InsertItem(item)
		m_testIconView:SetItemPosition(i, wx.wxPoint(i*40, 0))
	end
	-- show first item
	m_testIconView:SetItemState(0, wx.wxLIST_STATE_FOCUSED+wx.wxLIST_STATE_SELECTED, wx.wxLIST_STATE_FOCUSED+wx.wxLIST_STATE_SELECTED)
	m_testIconView:EnsureVisible(0)

	controlSizer:Add(m_testIconView, 0, wx.wxEXPAND)

	-- create the button sizer
	buttonSizer = wx.wxBoxSizer(wx.wxHORIZONTAL)
	buttonSizer:AddStretchSpacer(1)
	buttonRunTest = wx.wxButton(m_panel, ID_BUTTON_TESTBOARD, "Test Board")
	buttonSizer:Add(buttonRunTest)
	buttonSizer:AddSpacer(8)
	buttonSaveReport = wx.wxButton(m_panel, ID_BUTTON_SAVEREPORT, "Save Report")
	buttonSizer:Add(buttonSaveReport)
	buttonSizer:AddSpacer(8)
	buttonQuit = wx.wxButton(m_panel, ID_BUTTON_QUIT, "Quit")
	buttonSizer:Add(buttonQuit)
	buttonSizer:AddStretchSpacer(1)
	controlSizer:Add(buttonSizer, 0, wx.wxEXPAND)

	style = wx.wxHW_SCROLLBAR_AUTO+wx.wxBORDER_SUNKEN
	m_htmlWindow = wx.wxHtmlWindow(m_panel, wx.wxID_ANY, wx.wxDefaultPosition, wx.wxDefaultSize, style)
	mainSizer:Add(m_htmlWindow, 1, wx.wxEXPAND)

	m_panel:Layout()
end


local function clearTestResult(iBoardIdx)
	m_testResults[iBoardIdx] = { results={}, logs={}, eResult=TEST_STATUS_NotCompleted, iSerialNr=-1, modified=true, report="" }
	for i=1,m_testCnt do
		m_testResults[iBoardIdx].results[i] = 0
		m_testResults[iBoardIdx].logs[i] = ""
	end
end


local function initTestResults()
	-- clear the whole table
	m_testResults = {}
	for i=1,m_boardCnt do
		clearTestResult(i)
	end
end


local function showTestReport(iBoardIdx, iTestIdx)
	local strAnchor


	strAnchor = "#"
	if iBoardIdx>0 then
		strAnchor = strAnchor.."B"..iBoardIdx
		if iTestIdx>0 then
			strAnchor = strAnchor.."T"..iTestIdx
		end
	end

	m_htmlWindow:LoadPage(strAnchor)
end


local function report_test_genHeader(testindex, test)
	-- rebuild report for this entry
	local iOk = 0
	local iFailed = 0
	local iFatal = 0
	local iNotCompleted = 0
	local strReportHeader = ""

	-- loop over the result table and count all results
	for j=1,m_testCnt do
		if test.results[j]==TEST_STATUS_NotCompleted then
			iNotCompleted = iNotCompleted + 1
		elseif test.results[j]==TEST_STATUS_Ok then
			iOk = iOk + 1
		elseif test.results[j]==TEST_STATUS_Failed then
			iFailed = iFailed + 1
		else
			iFatal = iFatal + 1
		end
	end
	-- update the result cache
	if iFatal>0 then
		test.eResult = TEST_STATUS_FatalError
	elseif iFailed>0 then
		test.eResult = TEST_STATUS_Failed
	elseif iNotCompleted>0 then
		test.eResult = TEST_STATUS_NotCompleted
	else
		test.eResult = TEST_STATUS_Ok
	end

	strReportHeader = strReportHeader .. "<h2>Board #" .. testindex .. " Testdetails</h2>\n"
	strReportHeader = strReportHeader .. "<table border=\"0\"><tbody>\n"

	strReportHeader = strReportHeader .. "<tr><td>Status:</td><td>"
	if test.eResult==TEST_STATUS_Ok then
		strReportHeader = strReportHeader .. "ok"
	elseif test.eResult==TEST_STATUS_NotCompleted then
		strReportHeader = strReportHeader .. "incomplete"
	elseif test.eResult==TEST_STATUS_Failed then
		strReportHeader = strReportHeader .. "error"
	else
		strReportHeader = strReportHeader .. "fatal error"
	end
	strReportHeader = strReportHeader .. "</td></tr>\n"
	strReportHeader = strReportHeader .. "<tr><td>Serial Number:</td><td>"
	if test.iSerialNr<0 then
		strReportHeader = strReportHeader .. "none"
	else
		strReportHeader = strReportHeader .. test.iSerialNr
	end
	strReportHeader = strReportHeader .. "</td></tr>\n"
	strReportHeader = strReportHeader .. "</tbody></table>\n"
	strReportHeader = strReportHeader .. "<p>\n"

	return strReportHeader
end


local function report_test_genDetails(testindex, test)
	local strReportDetails = ""


	-- loop over all tests
	strReportDetails = strReportDetails .. "<table border=\"1\"><tbody>\n"
	strReportDetails = strReportDetails .. "<tr><th>Name</th><th>Status</th></tr>\n"
	for j=1,m_testCnt do
		strReportDetails = strReportDetails .. "<tr><td><a href=\"#B" .. testindex .. "T" .. j .. "\">" .. m_testNames:Item(j-1) .. "</a></td><td>"
		if test.results[j]==TEST_STATUS_NotCompleted then
			strReportDetails = strReportDetails .. "untested"
		elseif test.results[j]==TEST_STATUS_Ok then
			strReportDetails = strReportDetails .. "ok"
		elseif test.results[j]==TEST_STATUS_Failed then
			strReportDetails = strReportDetails .. "failed"
		else
			strReportDetails = strReportDetails .. "fatal error"
		end
		strReportDetails = strReportDetails .. "</td></tr>\n"
	end
	strReportDetails = strReportDetails .. "</tbody></table>\n"
	strReportDetails = strReportDetails .. "<p>\n"

	for j=1,m_testCnt do
		strReportDetails = strReportDetails .. "<a name=\"B" .. testindex .. "T" .. j .. "\"></a>\n"
		strReportDetails = strReportDetails .. "<h3>Log for " .. m_testNames:Item(j-1) .. "</h3>\n"
		strReportDetails = strReportDetails .. "<table border=\"1\" width=\"100%\"><tbody><tr><td><tt>\n"
		strReportDetails = strReportDetails .. test.logs[j]
		strReportDetails = strReportDetails .. "</tt></td></tr></tbody></table>\n"
	end

	return strReportDetails
end


local function updateTestReport()
	local report = ""
	local iBoardsOk = 0
	local iBoardsFailed = 0
	local iBoardsFatalError = 0
	local iBoardsUntested = 0


	-- update all report snipplets and collect test summary
	for i,t in ipairs(m_testResults) do
		if t.modified==true then
			local strReport

			strReport = report_test_genHeader(i, t)
			strReport = strReport .. report_test_genDetails(i, t)

			-- cache is up to date now
			t.report = strReport
			t.modified = false
		end

		if t.eResult==TEST_STATUS_Ok then
			iBoardsOk = iBoardsOk + 1
		elseif t.eResult==TEST_STATUS_NotCompleted then
			iBoardsUntested = iBoardsUntested + 1
		elseif t.eResult==TEST_STATUS_Failed then
			iBoardsFailed = iBoardsFailed + 1
		else
			iBoardsFatalError = iBoardsFatalError + 1
		end
	end

	-- show header
	report = "<html><body>\n"
	report = report.."<h1>Testreport for "..m_strTestName.."</h1>\n"
	report = report.."<h2>Test summary</h2>\n"
	report = report.."<table border=\"0\"><tbody>\n"
	report = report.."<tr><td>Test started:</td><td>2007.10.01, 12:15CET</td></tr>\n"
	report = report.."<tr><td>Test finished:</td><td>2007.10.01, 12:27CET</td></tr>\n"
	report = report.."</tbody></table>\n"
	report = report.."<p>\n"
	report = report.."<table border=\"0\"><tbody>\n"
	report = report.."<tr><td>Total number of Boards:</td><td>"..m_boardCnt.."</td></tr>\n"
	report = report.."<tr><td>Boards ok:</td><td>"..iBoardsOk.."</td></tr>\n"
	report = report.."<tr><td>Boards failed:</td><td>"..iBoardsFailed.."</td></tr>\n"
	report = report.."<tr><td>Boards untested:</td><td>"..iBoardsUntested.."</td></tr>\n"
	report = report.."<tr><td>Fatal Errors:</td><td>"..iBoardsFatalError.."</td></tr>\n"
	report = report.."</tbody></table>\n"
	report = report.."<p>\n"
	report = report.."<table border=\"1\"><tbody>\n"
	report = report.."<tr><th>Board #</th><th>Status</th><th>Serial Nr</th></tr>\n"
	for i,t in ipairs(m_testResults) do
		report = report.."<tr><td><a href=\"#B"..i.."\">"..i.."</a></td><td>"
		if t.eResult==TEST_STATUS_Ok then
			report = report.."ok"
		elseif t.eResult==TEST_STATUS_NotCompleted then
			report = report.."incomplete"
		elseif t.eResult==TEST_STATUS_Failed then
			report = report.."failed"
		else
			report = report.."fatal error"
		end
		report = report.."</td><td>"
		if t.iSerialNr<0 then
			report = report.."none"
		else
			report = report..t.iSerialNr
		end
		report = report.."</td></tr>\n"
	end
	report = report.."</tbody></table>\n"
	report = report.."<p>\n"
	report = report.."<hr width=\"90%\">\n"

	-- append all reports
	for i,t in ipairs(m_testResults) do
		report = report.."<a name=\"B"..i.."\"></a>\n"
		report = report..t.report
		report = report.."<hr width=\"90%\">\n"
	end

	report = report.."Generated by "..__MUHKUH_VERSION..", using "..wx.wxVERSION_STRING..", "..wxlua.wxLUA_VERSION_STRING.." and ".._VERSION.."\n"
	report = report.."</body></html>\n"

	m_strTestReport = report
	m_htmlWindow:SetPage(m_strTestReport)
end


local function moveToTest(iBoardIdx, iTestIdx)
	local testresult
	local iconidx
	local iSelectTest


	if iBoardIdx>0 and iBoardIdx<=m_boardCnt and iTestIdx>=0 and iTestIdx<=m_testCnt then
		-- update the icons
		for i=0,m_testCnt-1 do
			testresult = m_testResults[iBoardIdx].results[i+1]
			if testresult==TEST_STATUS_NotCompleted then
				iconidx = 0
			elseif testresult==TEST_STATUS_Ok then
				iconidx = 1
			elseif testresult==TEST_STATUS_Failed then
				iconidx = 2
			else
				-- no special icon for fatal error, use the 'failed' icon
				iconidx = 2
			end
			m_testIconView:SetItemImage(i, iconidx)
		end
		if iTestIdx==0 then
			-- show summary, select the first test
			iSelectTest = 1
		else
			iSelectTest = iTestIdx
		end

		-- select the test
		m_testCombo:Select(iSelectTest-1)

		-- select the icon
		m_testIconView:SetItemState(iSelectTest-1, wx.wxLIST_STATE_FOCUSED+wx.wxLIST_STATE_SELECTED, wx.wxLIST_STATE_FOCUSED+wx.wxLIST_STATE_SELECTED)
		m_testIconView:EnsureVisible(iSelectTest-1)

		-- scroll to the test summary
		showTestReport(iBoardIdx, iTestIdx)
	end
end


local function html_escape(text)
	-- NOTE: The order of the elements in the replace table is important.
	-- NOTE: "&" must be replaced first, as it occurs in all other replacements.
	local replace =
	{
		{ s="&",	r="&amp;" },
		{ s="<",	r="&lt;" },
		{ s=">",	r="&gt;" },
		{ s="\"",	r="&quot;" },
		{ s="\t",	r="        " },
		{ s=" ",	r="&nbsp;" },
		{ s="\n",	r="<br>" }
	}

	if not text then
		text = ""
	else
		for i,p in pairs(replace) do
			text = string.gsub(text, p.s, p.r)
		end
	end

	return text
end


local function runTest(iBoardIdx, iTestIdx)
	local test
	local testfn
	local luaresult
	local testresult
	local result
	local results
	local strLogCapture


	while iTestIdx<=m_testCnt do
		-- show the test
		moveToTest(iBoardIdx, iTestIdx)

		-- get the test
		test = __MUHKUH_ALL_TESTS[iTestIdx+1]
		-- get the results
		results = m_testResults[iBoardIdx]

		-- TODO: merge the parameters

		-- capture the log
		muhkuh:setLogMarker()

		-- execute the testcode
		print("running test '"..test.name.."'")
		testfn,luaresult = loadstring(test.code)
		if not testfn then
			print("failed to compile test code:", luaresult)
			testresult = __MUHKUH_TEST_RESULT_FATALERROR
		else
			luaresult,testresult = pcall(testfn)
			if not luaresult then
				print("failed to execute code:", testresult)
				testresult = __MUHKUH_TEST_RESULT_FATALERROR
			end
		end
		print("finished test '"..test.name.."'")

		-- close any stray progress dialogs
		stdWriteCloseProgress()
		stdReadCloseProgress()
		stdCallCloseProgress()

		-- set the test result
		if testresult==__MUHKUH_TEST_RESULT_OK then
			result = TEST_STATUS_Ok
		elseif testresult==__MUHKUH_TEST_RESULT_CANCEL then
			result = TEST_STATUS_NotCompleted
		elseif testresult==__MUHKUH_TEST_RESULT_FAIL then
			result = TEST_STATUS_Failed
		elseif testresult==__MUHKUH_TEST_RESULT_FATALERROR then
			result = TEST_STATUS_FatalError
		else
			print("test returned strange result:", testresult)
			result = TEST_STATUS_FatalError
		end
		results.results[iTestIdx] = result

		-- is this the last test?
		if result==TEST_STATUS_FatalError or eMODE_BOARD_TEST==eMODE_SINGLE_TEST or iTestIdx==m_testCnt then
			-- this is the last test -> close common plugin
			closeCommonPlugin()
		end

		strLogCapture = muhkuh:getMarkedLog()

		-- escape all special chars for html
		results.logs[iTestIdx] = html_escape(strLogCapture)
		results.modified = true

		-- show the result
		updateTestReport()
		-- the report update set the page back to the top, move to the board result again
		moveToTest(iBoardIdx, iTestIdx)

		-- don't continue for fatal errors or single tests
		if result==TEST_STATUS_FatalError or eMODE_BOARD_TEST==eMODE_SINGLE_TEST then
			break
		end

		-- next test
		iTestIdx = iTestIdx + 1
	end
end


local function OnBoardSpin(event)
	local iBoardIdx


	if not m_lockUpdates then
		m_lockUpdates = true

		-- get the selected board
		iBoardIdx = event:GetInt()
		-- scroll to the test summary
		moveToTest(iBoardIdx, 0)

		m_lockUpdates = false
	end
end


local function OnTestNameSelected(event)
	local iBoardIdx
	local iTestIdx


	if not m_lockUpdates then
		m_lockUpdates = true

		-- get the selected board
		iBoardIdx = m_spinBoardNr:GetValue()
		-- get the selected test
		iTestIdx = event:GetSelection() + 1
		-- show the test
		moveToTest(iBoardIdx, iTestIdx)

		m_lockUpdates = false
	end
end


local function OnTestIconSelected(event)
	local iBoardIdx
	local iTestIdx


	if not m_lockUpdates then
		m_lockUpdates = true

		-- get the selected board
		iBoardIdx = m_spinBoardNr:GetValue()
		-- get the selected test
		iTestIdx = event:GetIndex() + 1
		-- show the test
		moveToTest(iBoardIdx, iTestIdx)

		m_lockUpdates = false
	end
end


local function OnClose(event)
	-- notify muhkuh server
	muhkuh.TestHasFinished()
end


local function OnButtonTestBoard()
	local iBoardIdx


	changeMode(eMODE_BOARD_TEST)

	-- get the selected board
	iBoardIdx = m_spinBoardNr:GetValue()
	runTest(iBoardIdx, 1)
end


local function OnButtonSaveReport()
	local iResult
	local strFileName
	local outputFile


	iResult = m_saveReportDlg:ShowModal()
	if iResult==wx.wxID_OK then
		strFileName = m_saveReportDlg:GetPath()
		print("saving report to file "..strFileName)
		outputFile = wx.wxFile(strFileName, wx.wxFile.write)
		outputFile:Write(m_strTestReport)
		outputFile:Close()
	end
end


function run()
	local plugin


	-- get the test name
	m_strTestName = __MUHKUH_ALL_TESTS[1].name
	-- get the number of tests
	m_testCnt = #__MUHKUH_ALL_TESTS - 1

	-- get the device number
	m_deviceNr = __MUHKUH_PARAMETERS.DeviceNumber
	-- get the number of boards to test
	if __MUHKUH_PARAMETERS.BoardCount==nil then
		m_boardCnt = 1
	else
		m_boardCnt = __MUHKUH_PARAMETERS.BoardCount
	end

	-- get all test names
	m_testNames = wx.wxArrayString()
	for i,t in ipairs(__MUHKUH_ALL_TESTS) do
		-- skip the first entry, that's the test description
		if i>1 then
			m_testNames:Add(t.name)
		end
	end

	-- clear the result table
	initTestResults()

	-- create dialog
	m_panel = __MUHKUH_PANEL

	-- create the controls
	createControls()
	-- connect some controls
	m_panel:Connect(ID_SPIN_BOARDNR,		wx.wxEVT_COMMAND_SPINCTRL_UPDATED,		OnBoardSpin)
	m_panel:Connect(ID_COMBO_TESTS,			wx.wxEVT_COMMAND_COMBOBOX_SELECTED,		OnTestNameSelected)
	m_panel:Connect(ID_LISTCTRL_ICONVIEW,		wx.wxEVT_COMMAND_LIST_ITEM_SELECTED,		OnTestIconSelected)
	m_panel:Connect(ID_BUTTON_TESTBOARD,		wx.wxEVT_COMMAND_BUTTON_CLICKED,		OnButtonTestBoard)
	m_panel:Connect(ID_BUTTON_SAVEREPORT,		wx.wxEVT_COMMAND_BUTTON_CLICKED,		OnButtonSaveReport)
	m_panel:Connect(ID_BUTTON_QUIT,			wx.wxEVT_COMMAND_BUTTON_CLICKED,		OnClose)

	-- init report
	updateTestReport()

	-- run a single test?
	if __MUHKUH_TEST_INDEX>0 then
		moveToTest(1, __MUHKUH_TEST_INDEX)
	end

	-- ready to get user inputs
	m_lockUpdates = false
end
