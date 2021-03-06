/*
 Copyright (c) 2011 Mathieu Laurendeau <mat.lau@laposte.net>
 License: GPLv3
 */

#include "wx_pch.h"
#include "serialMain.h"
#include <wx/msgdlg.h>

//(*InternalHeaders(serialFrame)
#include <wx/string.h>
#include <wx/intl.h>
//*)

#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#else
#include <pwd.h>
#endif

#include <wx/aboutdlg.h>
#include <wx/dir.h>
#include "serial.h"

#include "../directories.h"
#include "../shared/updater/updater.h"
#include "../shared/configupdater/configupdater.h"
#include <ConfigurationFile.h>

#include <wx/arrstr.h>
#include <wx/stdpaths.h>
#include <wx/busyinfo.h>

using namespace std;

#ifdef WIN32
#define MAX_PORT_ID 32
#endif

wxString userConfigDir;
wxString userDataDir;

//(*IdInit(serialFrame)
const long serialFrame::ID_STATICTEXT4 = wxNewId();
const long serialFrame::ID_CHOICE1 = wxNewId();
const long serialFrame::ID_STATICTEXT3 = wxNewId();
const long serialFrame::ID_COMBOBOX1 = wxNewId();
const long serialFrame::ID_CHECKBOX5 = wxNewId();
const long serialFrame::ID_CHECKBOX6 = wxNewId();
const long serialFrame::ID_CHECKBOX1 = wxNewId();
const long serialFrame::ID_CHECKBOX2 = wxNewId();
const long serialFrame::ID_CHECKBOX3 = wxNewId();
const long serialFrame::ID_CHOICE4 = wxNewId();
const long serialFrame::ID_BUTTON1 = wxNewId();
const long serialFrame::ID_BUTTON3 = wxNewId();
const long serialFrame::ID_PANEL1 = wxNewId();
const long serialFrame::ID_MENUITEM1 = wxNewId();
const long serialFrame::ID_MENUITEM2 = wxNewId();
const long serialFrame::ID_MENUITEM8 = wxNewId();
const long serialFrame::ID_MENUITEM7 = wxNewId();
const long serialFrame::ID_MENUITEM3 = wxNewId();
const long serialFrame::idMenuQuit = wxNewId();
const long serialFrame::ID_MENUITEM6 = wxNewId();
const long serialFrame::ID_MENUITEM4 = wxNewId();
const long serialFrame::ID_MENUITEM5 = wxNewId();
const long serialFrame::idMenuAbout = wxNewId();
const long serialFrame::ID_STATUSBAR1 = wxNewId();
//*)

BEGIN_EVENT_TABLE(serialFrame,wxFrame)
    //(*EventTable(serialFrame)
    //*)
END_EVENT_TABLE()

#ifdef WIN32
static void read_devices(wxComboBox* choice)
{
  HANDLE hSerial;
  DWORD accessdirection = /*GENERIC_READ |*/GENERIC_WRITE;
  char portname[16];
  wchar_t szCOM[16];
  int i;
  wxString previous = choice->GetStringSelection();

  choice->Clear();

  for(i=0; i<MAX_PORT_ID; ++i)
  {
    wsprintf(szCOM, L"\\\\.\\COM%d", i);
    hSerial = CreateFile(szCOM, accessdirection, 0, 0, OPEN_EXISTING, 0, 0);
    if (hSerial != INVALID_HANDLE_VALUE)
    {
      DCB oldDcbSerialParams = { 0 };
      oldDcbSerialParams.DCBlength = sizeof(oldDcbSerialParams);
      if (GetCommState(hSerial, &oldDcbSerialParams))
      {
        DCB newDcbSerialParams;
        memcpy(&newDcbSerialParams, &oldDcbSerialParams, sizeof(newDcbSerialParams));
        newDcbSerialParams.BaudRate = 500000;
        newDcbSerialParams.ByteSize = 8;
        newDcbSerialParams.StopBits = ONESTOPBIT;
        newDcbSerialParams.Parity = NOPARITY;
        if (SetCommState(hSerial, &newDcbSerialParams))//test port parameters
        {
          snprintf(portname, sizeof(portname), "COM%d", i);
          choice->SetSelection(choice->Append(wxString(portname, wxConvUTF8)));
        }
        SetCommState(hSerial, &oldDcbSerialParams);//restore port parameters, do not care about any error
      }
    }
    CloseHandle(hSerial);
  }

  if(previous != wxEmptyString)
  {
    choice->SetSelection(choice->FindString(previous));
  }
  if(choice->GetSelection() < 0)
  {
    choice->SetSelection(0);
  }
}
#else
static void read_devices(wxComboBox* choice)
{
  string filename;
  string line = "";
  wxString previous = choice->GetStringSelection();

  filename = string(userDataDir.mb_str(wxConvUTF8));
  filename.append("/config");
  ifstream infile (filename.c_str());
  if ( infile.is_open() )
  {
      if( infile.good() )
      {
          getline (infile,line);
      }
      infile.close();
  }

  choice->Clear();

  string ds = "/dev";

  wxDir dir(wxString(ds.c_str(), wxConvUTF8));

  if(!dir.IsOpened())
  {
    cout << "Warning: can't open " << ds << endl;
    return;
  }

  wxString file;
  wxString filespec = wxT("*");

  for (bool cont = dir.GetFirst(&file, filespec, wxDIR_FILES); cont;  cont = dir.GetNext(&file))
  {
    if(file.StartsWith(wxT("ttyUSB")) || file.StartsWith(wxT("ttyACM")))
    {
      if(!line.empty() && wxString(line.c_str(), wxConvUTF8) == file)
      {
        choice->SetSelection(choice->Append(file));
      }
      else
      {
        choice->Append(file);
      }
    }
  }

  if(previous != wxEmptyString)
  {
    choice->SetSelection(choice->FindString(previous));
  }
  if(choice->GetSelection() < 0)
  {
    choice->SetSelection(0);
  }
}
#endif

static void read_filenames(wxChoice* choice)
{
  string filename;
  string line = "";
  wxString previous = choice->GetStringSelection();

  /* Read the last config used so as to auto-select it. */
  filename = string(userDataDir.mb_str(wxConvUTF8));
  filename.append("/default");
  ifstream infile (filename.c_str());
  if ( infile.is_open() )
  {
    if( infile.good() )
    {
      getline (infile,line);
    }
    infile.close();
  }

  choice->Clear();

  /* Read all config file names. */
  wxDir dir(userConfigDir);

  if(!dir.IsOpened())
  {
    return;
  }

  wxString file;
  wxString filespec = wxT("*.xml");

  for (bool cont = dir.GetFirst(&file, filespec, wxDIR_FILES); cont;  cont = dir.GetNext(&file))
  {
    if(!line.empty() && wxString(line.c_str(), wxConvUTF8) == file)
    {
      previous = file;
    }
    choice->Append(file);
  }

  if(previous != wxEmptyString)
  {
    choice->SetSelection(choice->FindString(previous));
  }
  if(choice->GetSelection() < 0)
  {
    choice->SetSelection(0);
  }
}

static void read_controller_type(wxChoice* choice)
{
  string filename;
  string line = "";

  filename = string(userDataDir.mb_str(wxConvUTF8));
  filename.append("/controller");
  ifstream infile (filename.c_str());
  if ( infile.is_open() )
  {
    if( infile.good() )
    {
      getline (infile,line);
      choice->SetSelection(choice->FindString(wxString(line.c_str(), wxConvUTF8)));
    }
    infile.close();
  }
}

static void readStartUpdates(wxMenuItem* menuItem)
{
  string filename;
  string line = "";

  filename = string(userDataDir.mb_str(wxConvUTF8));
  filename.append("/startUpdates");
  ifstream infile (filename.c_str());
  if ( infile.is_open() )
  {
    if( infile.good() )
    {
      getline (infile,line);
      if(line == "yes")
      {
        menuItem->Check(true);
      }
    }
    infile.close();
  }
}

serialFrame::serialFrame(wxWindow* parent,wxWindowID id)
{
    locale = new wxLocale(wxLANGUAGE_DEFAULT);
#ifdef WIN32
    locale->AddCatalogLookupPathPrefix(wxT("share/locale"));
#endif
    locale->AddCatalog(wxT("gimx"));

    setlocale( LC_NUMERIC, "C" ); /* Make sure we use '.' to write doubles. */

    //(*Initialize(serialFrame)
    wxMenuItem* MenuItem2;
    wxMenuItem* MenuItem1;
    wxFlexGridSizer* FlexGridSizer8;
    wxFlexGridSizer* FlexGridSizer2;
    wxMenu* Menu1;
    wxStaticBoxSizer* StaticBoxSizer5;
    wxFlexGridSizer* FlexGridSizer11;
    wxFlexGridSizer* FlexGridSizer7;
    wxFlexGridSizer* FlexGridSizer4;
    wxFlexGridSizer* FlexGridSizer9;
    wxStaticBoxSizer* StaticBoxSizer8;
    wxStaticBoxSizer* StaticBoxSizer6;
    wxFlexGridSizer* FlexGridSizer10;
    wxFlexGridSizer* FlexGridSizer13;
    wxMenuBar* MenuBar1;
    wxFlexGridSizer* FlexGridSizer12;
    wxMenu* Menu2;

    Create(parent, wxID_ANY, _("Gimx-serial"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, _T("wxID_ANY"));
    Panel1 = new wxPanel(this, ID_PANEL1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("ID_PANEL1"));
    FlexGridSizer1 = new wxFlexGridSizer(3, 1, 0, 0);
    FlexGridSizer2 = new wxFlexGridSizer(2, 1, 0, 0);
    FlexGridSizer12 = new wxFlexGridSizer(2, 2, 0, 0);
    StaticText4 = new wxStaticText(Panel1, ID_STATICTEXT4, _("Adapter"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT4"));
    FlexGridSizer12->Add(StaticText4, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    ControllerType = new wxChoice(Panel1, ID_CHOICE1, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE1"));
    ControllerType->SetSelection( ControllerType->Append(_("DIY USB adapter")) );
    ControllerType->Append(_("GPP/Cronus"));
    FlexGridSizer12->Add(ControllerType, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    StaticText3 = new wxStaticText(Panel1, ID_STATICTEXT3, _("Port"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT3"));
    FlexGridSizer12->Add(StaticText3, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    ComboBoxDevice = new wxComboBox(Panel1, ID_COMBOBOX1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_COMBOBOX1"));
    FlexGridSizer12->Add(ComboBoxDevice, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer2->Add(FlexGridSizer12, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer13 = new wxFlexGridSizer(1, 2, 0, 0);
    CheckBoxForceUpdates = new wxCheckBox(Panel1, ID_CHECKBOX5, _("Force updates"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX5"));
    CheckBoxForceUpdates->SetValue(true);
    FlexGridSizer13->Add(CheckBoxForceUpdates, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    CheckBoxSubpositions = new wxCheckBox(Panel1, ID_CHECKBOX6, _("Subposition"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX6"));
    CheckBoxSubpositions->SetValue(true);
    FlexGridSizer13->Add(CheckBoxSubpositions, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer2->Add(FlexGridSizer13, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer1->Add(FlexGridSizer2, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer8 = new wxFlexGridSizer(1, 2, 0, 0);
    StaticBoxSizer5 = new wxStaticBoxSizer(wxVERTICAL, Panel1, _("Mouse"));
    FlexGridSizer10 = new wxFlexGridSizer(1, 1, 0, 0);
    CheckBoxGrab = new wxCheckBox(Panel1, ID_CHECKBOX1, _("grab"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX1"));
    CheckBoxGrab->SetValue(true);
    FlexGridSizer10->Add(CheckBoxGrab, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    StaticBoxSizer5->Add(FlexGridSizer10, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer8->Add(StaticBoxSizer5, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    StaticBoxSizer6 = new wxStaticBoxSizer(wxHORIZONTAL, Panel1, _("Output"));
    FlexGridSizer11 = new wxFlexGridSizer(0, 3, 0, 0);
    CheckBoxGui = new wxCheckBox(Panel1, ID_CHECKBOX2, _("gui"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX2"));
    CheckBoxGui->SetValue(false);
    FlexGridSizer11->Add(CheckBoxGui, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    CheckBoxTerminal = new wxCheckBox(Panel1, ID_CHECKBOX3, _("terminal"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX3"));
    CheckBoxTerminal->SetValue(false);
    FlexGridSizer11->Add(CheckBoxTerminal, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    StaticBoxSizer6->Add(FlexGridSizer11, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer8->Add(StaticBoxSizer6, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer1->Add(FlexGridSizer8, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer9 = new wxFlexGridSizer(1, 2, 0, 0);
    StaticBoxSizer8 = new wxStaticBoxSizer(wxHORIZONTAL, Panel1, _("Config"));
    FlexGridSizer4 = new wxFlexGridSizer(1, 1, 0, 0);
    ChoiceConfig = new wxChoice(Panel1, ID_CHOICE4, wxDefaultPosition, wxDefaultSize, 0, 0, wxCB_SORT, wxDefaultValidator, _T("ID_CHOICE4"));
    FlexGridSizer4->Add(ChoiceConfig, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    StaticBoxSizer8->Add(FlexGridSizer4, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer9->Add(StaticBoxSizer8, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer7 = new wxFlexGridSizer(2, 1, 0, 0);
    ButtonCheck = new wxButton(Panel1, ID_BUTTON1, _("Check"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON1"));
    FlexGridSizer7->Add(ButtonCheck, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    ButtonStart = new wxButton(Panel1, ID_BUTTON3, _("Start"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON3"));
    FlexGridSizer7->Add(ButtonStart, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer9->Add(FlexGridSizer7, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer1->Add(FlexGridSizer9, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    Panel1->SetSizer(FlexGridSizer1);
    FlexGridSizer1->Fit(Panel1);
    FlexGridSizer1->SetSizeHints(Panel1);
    MenuBar1 = new wxMenuBar();
    Menu1 = new wxMenu();
    MenuEditConfig = new wxMenuItem(Menu1, ID_MENUITEM1, _("Edit config"), wxEmptyString, wxITEM_NORMAL);
    Menu1->Append(MenuEditConfig);
    MenuEditFpsConfig = new wxMenuItem(Menu1, ID_MENUITEM2, _("Edit fps config"), wxEmptyString, wxITEM_NORMAL);
    Menu1->Append(MenuEditFpsConfig);
    MenuItem3 = new wxMenuItem(Menu1, ID_MENUITEM8, _("Open config directory"), wxEmptyString, wxITEM_NORMAL);
    Menu1->Append(MenuItem3);
    MenuAutoBindControls = new wxMenuItem(Menu1, ID_MENUITEM7, _("Auto-bind and convert"), wxEmptyString, wxITEM_NORMAL);
    Menu1->Append(MenuAutoBindControls);
    MenuRefresh = new wxMenuItem(Menu1, ID_MENUITEM3, _("Refresh\tF5"), wxEmptyString, wxITEM_NORMAL);
    Menu1->Append(MenuRefresh);
    MenuItem1 = new wxMenuItem(Menu1, idMenuQuit, _("Quit\tAlt-F4"), _("Quit the application"), wxITEM_NORMAL);
    Menu1->Append(MenuItem1);
    MenuBar1->Append(Menu1, _("&File"));
    Menu2 = new wxMenu();
    MenuGetConfigs = new wxMenuItem(Menu2, ID_MENUITEM6, _("Get configs"), wxEmptyString, wxITEM_NORMAL);
    Menu2->Append(MenuGetConfigs);
    MenuUpdate = new wxMenuItem(Menu2, ID_MENUITEM4, _("Update"), wxEmptyString, wxITEM_NORMAL);
    Menu2->Append(MenuUpdate);
    MenuStartupUpdates = new wxMenuItem(Menu2, ID_MENUITEM5, _("Check updates at startup"), wxEmptyString, wxITEM_CHECK);
    Menu2->Append(MenuStartupUpdates);
    MenuItem2 = new wxMenuItem(Menu2, idMenuAbout, _("About\tF1"), _("Show info about this application"), wxITEM_NORMAL);
    Menu2->Append(MenuItem2);
    MenuBar1->Append(Menu2, _("Help"));
    SetMenuBar(MenuBar1);
    StatusBar1 = new wxStatusBar(this, ID_STATUSBAR1, 0, _T("ID_STATUSBAR1"));
    int __wxStatusBarWidths_1[2] = { -1, 20 };
    int __wxStatusBarStyles_1[2] = { wxSB_NORMAL, wxSB_NORMAL };
    StatusBar1->SetFieldsCount(2,__wxStatusBarWidths_1);
    StatusBar1->SetStatusStyles(2,__wxStatusBarStyles_1);
    SetStatusBar(StatusBar1);
    SingleInstanceChecker1.Create(_T("gimx-serial_") + wxGetUserId() + _T("_Guard"));

    Connect(ID_CHOICE1,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&serialFrame::OnControllerTypeSelect);
    Connect(ID_CHECKBOX2,wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&serialFrame::OnCheckBoxGuiClick);
    Connect(ID_CHECKBOX3,wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&serialFrame::OnCheckBoxTerminalClick);
    Connect(ID_BUTTON1,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&serialFrame::OnButtonCheckClick1);
    Connect(ID_BUTTON3,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&serialFrame::OnButtonStartClick);
    Connect(ID_MENUITEM1,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnMenuEditConfig);
    Connect(ID_MENUITEM2,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnMenuEditFpsConfig);
    Connect(ID_MENUITEM8,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnMenuOpenConfigDirectory);
    Connect(ID_MENUITEM7,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnMenuAutoBindControls);
    Connect(ID_MENUITEM3,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnMenuRefresh);
    Connect(idMenuQuit,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnQuit);
    Connect(ID_MENUITEM6,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnMenuGetConfigs);
    Connect(ID_MENUITEM4,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnMenuUpdate);
    Connect(ID_MENUITEM5,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnMenuStartupUpdates);
    Connect(idMenuAbout,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&serialFrame::OnAbout);
    //*)

    if(SingleInstanceChecker1.IsAnotherRunning())
    {
        wxMessageBox( _("gimx-serial is already running!"), _("Error"), wxICON_ERROR);
        exit(-1);
    }

    userDataDir = wxStandardPaths::Get().GetUserDataDir();
    if(!wxDir::Exists(userDataDir))
    {
      if(!wxMkdir(userDataDir))
      {
        wxMessageBox( _("Can't init directory: ") + userDataDir, _("Error"), wxICON_ERROR);
        exit(-1);
      }
    }

    userConfigDir = wxStandardPaths::Get().GetUserConfigDir();
    userConfigDir.Append(wxT(APP_DIR));
    if(!wxDir::Exists(userConfigDir))
    {
      if(!wxMkdir(userConfigDir))
      {
        wxMessageBox( _("Can't init directory: ") + userConfigDir, _("Error"), wxICON_ERROR);
        exit(-1);
      }
    }
    userConfigDir.Append(wxT(CONFIG_DIR));
    if(!wxDir::Exists(userConfigDir))
    {
      if(!wxMkdir(userConfigDir))
      {
        wxMessageBox( _("Can't init directory: ") + userConfigDir, _("Error"), wxICON_ERROR);
        exit(-1);
      }
    }

    started = false;

    read_controller_type(ControllerType);
    refresh();
    
    wxCommandEvent event;
    OnControllerTypeSelect(event);

    readStartUpdates(MenuStartupUpdates);
    if(MenuStartupUpdates->IsChecked())
    {
      OnMenuUpdate(event);
    }

    started = true;

    if(ChoiceConfig->IsEmpty())
    {
      int answer = wxMessageBox(_("No config found! Download configs?"), _("Confirm"), wxYES_NO);
      if (answer == wxYES)
      {
        wxCommandEvent event;
        OnMenuGetConfigs(event);
      }
    }

    Panel1->Fit();
    Fit();
}

serialFrame::~serialFrame()
{
    //(*Destroy(serialFrame)
    //*)
}

void serialFrame::OnQuit(wxCommandEvent& event)
{
  Close();
}

void serialFrame::OnAbout(wxCommandEvent& event)
{
  wxAboutDialogInfo info;
  info.SetName(wxTheApp->GetAppName());
  info.SetVersion(wxT(INFO_VERSION));
  wxString text = wxString(wxT(INFO_DESCR)) + wxString(wxT("\n")) + wxString(wxT(INFO_YEAR)) + wxString(wxT(" ")) + wxString(wxT(INFO_DEV)) + wxString(wxT(" ")) + wxString(wxT(INFO_LICENCE));
  info.SetDescription(text);
  info.SetWebSite(wxT(INFO_WEB));

  wxAboutBox(info);
}

class MyProcess : public wxProcess
{
public:
    MyProcess(serialFrame *parent, const wxString& cmd)
        : wxProcess(parent), m_cmd(cmd)
    {
        m_parent = parent;
    }

    void OnTerminate(int pid, int status);

protected:
    serialFrame *m_parent;
    wxString m_cmd;
};

void MyProcess::OnTerminate(int pid, int status)
{
    m_parent->OnProcessTerminated(this, status);
}

void serialFrame::OnButtonStartClick(wxCommandEvent& event)
{
    wxString command;
    wxArrayString output, errors;
    string filename;
    wxString dpi;

    if(ChoiceConfig->GetStringSelection().IsEmpty())
    {
      wxMessageBox( _("No config selected!"), _("Error"), wxICON_ERROR);
      return;
    }

    if(ControllerType->GetStringSelection() != _("GPP/Cronus"))
    {
      if(ComboBoxDevice->GetValue().IsEmpty())
      {
        wxMessageBox( _("No USB to serial device selected!"), _("Error"), wxICON_ERROR);
        return;
      }
    }

#ifndef WIN32
    command.Append(wxT("xterm -e "));
#endif
    command.Append(wxT("gimx"));
    if(ControllerType->GetStringSelection() == _("GPP/Cronus"))
    {
      command.Append(wxT(" --type GPP"));
    }
    else
    {
      command.Append(wxT(" --port "));
#ifndef WIN32
      command.Append(wxT("/dev/"));
#endif
      command.Append(ComboBoxDevice->GetValue());
    }
    if(!CheckBoxGrab->IsChecked())
    {
        command.Append(wxT(" --nograb"));
    }
    command.Append(wxT(" --config \""));
    command.Append(ChoiceConfig->GetStringSelection());
    command.Append(wxT("\""));
    if(CheckBoxForceUpdates->IsChecked())
    {
        command.Append(wxT(" --force-updates"));
    }
    if(CheckBoxSubpositions->IsChecked())
    {
        command.Append(wxT(" --subpos"));
    }

    //cout << command.c_str() << endl;

    filename = userDataDir.mb_str(wxConvUTF8);
    filename.append("/default");
    ofstream outfile (filename.c_str(), ios_base::trunc);
    if(outfile.is_open())
    {
        outfile << ChoiceConfig->GetStringSelection().mb_str(wxConvUTF8) << endl;
        outfile.close();
    }
    filename = userDataDir.mb_str(wxConvUTF8);
    filename.append("/config");
    ofstream outfile3 (filename.c_str(), ios_base::trunc);
    if(outfile3.is_open())
    {
        outfile3 << ComboBoxDevice->GetValue().mb_str(wxConvUTF8) << endl;
        outfile3.close();
    }
    filename = userDataDir.mb_str(wxConvUTF8);
    filename.append("/controller");
    ofstream outfile4 (filename.c_str(), ios_base::trunc);
    if(outfile4.is_open())
    {
        outfile4 << ControllerType->GetStringSelection().mb_str(wxConvUTF8) << endl;
        outfile4.close();
    }

    if(CheckBoxTerminal->IsChecked())
    {
        command.Append(wxT(" --status"));
    }
    else if(CheckBoxGui->IsChecked())
    {
      command.Append(wxT(" --curses"));
    }

    StatusBar1->SetStatusText(_("Press Shift+Esc to exit."));

    ButtonStart->Enable(false);

    MyProcess *process = new MyProcess(this, command);

    if(!wxExecute(command, wxEXEC_ASYNC | wxEXEC_NOHIDE, process))
    {
      wxMessageBox( _("can't start emuclient!"), _("Error"), wxICON_ERROR);
    }
}

void serialFrame::OnProcessTerminated(wxProcess *process, int status)
{
    ButtonStart->Enable(true);
    StatusBar1->SetStatusText(wxEmptyString);

    if(status)
    {
      wxMessageBox( _("emuclient error"), _("Error"), wxICON_ERROR);
    }

    SetFocus();
}

void serialFrame::OnCheckBoxGuiClick(wxCommandEvent& event)
{
    CheckBoxTerminal->SetValue(false);
}

void serialFrame::OnCheckBoxTerminalClick(wxCommandEvent& event)
{
    CheckBoxGui->SetValue(false);
}

void serialFrame::OnButtonCheckClick1(wxCommandEvent& event)
{
    if(ChoiceConfig->GetStringSelection().IsEmpty())
    {
      wxMessageBox( _("No config selected!"), _("Error"), wxICON_ERROR);
      return;
    }

    string file = string(userConfigDir.mb_str(wxConvUTF8));
    file.append(ChoiceConfig->GetStringSelection().mb_str(wxConvUTF8));

    ConfigurationFile configFile;
    int ret = configFile.ReadConfigFile(file);

    if(ret < 0)
    {
      wxMessageBox(wxString(configFile.GetError().c_str(), wxConvUTF8), _("Error"), wxICON_ERROR);
    }
    else if(ret > 0)
    {
      wxMessageBox(wxString(configFile.GetInfo().c_str(), wxConvUTF8), _("Info"), wxICON_INFORMATION);
    }
    else
    {
      wxMessageBox( _("This config seems OK!\n"), _("Info"), wxICON_INFORMATION);
    }
}

void serialFrame::OnMenuEditConfig(wxCommandEvent& event)
{
  if(ChoiceConfig->GetStringSelection().IsEmpty())
  {
    wxMessageBox( _("No config selected!"), _("Error"), wxICON_ERROR);
    return;
  }

  wxString command = wxT("gimx-config -f \"");
  command.Append(ChoiceConfig->GetStringSelection());
  command.Append(wxT("\""));

  if (!wxExecute(command, wxEXEC_ASYNC))
  {
    wxMessageBox(_("Error editing the config file!"), _("Error"),
        wxICON_ERROR);
  }
}

void serialFrame::OnMenuEditFpsConfig(wxCommandEvent& event)
{
  if(ChoiceConfig->GetStringSelection().IsEmpty())
  {
    wxMessageBox( _("No config selected!"), _("Error"), wxICON_ERROR);
    return;
  }

  wxString command = wxT("gimx-fpsconfig -f \"");
  command.Append(ChoiceConfig->GetStringSelection());
  command.Append(wxT("\""));

  if (!wxExecute(command, wxEXEC_ASYNC))
  {
    wxMessageBox(_("Error editing the config file!"), _("Error"),
        wxICON_ERROR);
  }
}

void serialFrame::refresh()
{
    read_filenames(ChoiceConfig);
    read_devices(ComboBoxDevice);
    if(ComboBoxDevice->GetCount() == 0 && ControllerType->GetStringSelection() != _("GPP/Cronus"))
    {
        wxMessageBox( _("No Serial Port Detected!\n"), _("Error"), wxICON_ERROR);
    }
}

void serialFrame::OnMenuRefresh(wxCommandEvent& event)
{
    refresh();
}

void serialFrame::OnControllerTypeSelect(wxCommandEvent& event)
{
    if(ControllerType->GetStringSelection() == _("GPP/Cronus"))
    {
      ComboBoxDevice->Enable(false);
    }
    else
    {
      ComboBoxDevice->Enable(true);
    }
}

void serialFrame::OnMenuUpdate(wxCommandEvent& event)
{
  int ret;

  updater* u = updater::getInstance();
  u->SetParams(VERSION_URL, VERSION_FILE, INFO_VERSION, DOWNLOAD_URL, DOWNLOAD_FILE);

  ret = u->CheckVersion();

  if (ret > 0)
  {
    int answer = wxMessageBox(_("Update available.\nStart installation?"), _("Confirm"), wxYES_NO);
    if (answer == wxNO)
    {
     return;
    }
    wxBusyInfo wait(_("Downloading update..."));
    if (u->Update() < 0)
    {
      wxMessageBox(_("Can't retrieve update file!"), _("Error"), wxICON_ERROR);
    }
    else
    {
      exit(0);
    }
  }
  else if (ret < 0)
  {
    wxMessageBox(_("Can't check version!"), _("Error"), wxICON_ERROR);
  }
  else if(started)
  {
    wxMessageBox(_("GIMX is up-to-date!"), _("Info"), wxICON_INFORMATION);
  }
}

void serialFrame::OnMenuStartupUpdates(wxCommandEvent& event)
{
  string filename = string(userDataDir.mb_str(wxConvUTF8));
  filename.append("/startUpdates");
  ofstream outfile (filename.c_str(), ios_base::trunc);
  if(outfile.is_open())
  {
    if(MenuStartupUpdates->IsChecked())
    {
      outfile << "yes" << endl;
    }
    else
    {
      outfile << "no" << endl;
    }
    outfile.close();
  }
}

void serialFrame::OnMenuGetConfigs(wxCommandEvent& event)
{
  string dir = string(userConfigDir.mb_str(wxConvUTF8));
  
  configupdater* u = configupdater::getInstance();
  u->SetParams(CONFIGS_URL, CONFIGS_FILE, dir);

  list<string>* cl;
  list<string> cl_sel;

  {
    wxBusyInfo wait(_("Downloading config list..."));
    cl = u->getconfiglist();
  }
  
  if(cl && !cl->empty())
  {
    wxArrayString choices;

    for(list<string>::iterator it = cl->begin(); it != cl->end(); ++it)
    {
      choices.Add(wxString(it->c_str(), wxConvUTF8));
    }

    wxMultiChoiceDialog dialog(this, _("Select the files to download."), _("Config download"), choices);

    if (dialog.ShowModal() == wxID_OK)
    {
      wxArrayInt selections = dialog.GetSelections();
      wxArrayString configs;

      for ( size_t n = 0; n < selections.GetCount(); n++ )
      {
        string sel = string(choices[selections[n]].mb_str(wxConvUTF8));
        wxString wxfile = wxString(dir.c_str(), wxConvUTF8) + choices[selections[n]];
        if (::wxFileExists(wxfile))
        {
          int answer = wxMessageBox(_("Overwrite local file: ") + choices[selections[n]] + _("?"), _("Confirm"), wxYES_NO);
          if (answer == wxNO)
          {
            continue;
          }
        }
        cl_sel.push_back(sel);
        configs.Add(choices[selections[n]]);
      }
      
      {
        wxBusyInfo wait(_("Downloading configs..."));
        if(u->getconfigs(&cl_sel) < 0)
        {
          wxMessageBox(_("Can't retrieve configs!"), _("Error"), wxICON_ERROR);
          return;
        }
      }

      if(!cl_sel.empty())
	    {
	      wxMessageBox(_("Download is complete!"), _("Info"), wxICON_INFORMATION);
	      if(!ChoiceConfig->IsEmpty())
	      {
	        int answer = wxMessageBox(_("Auto-bind and convert?"), _("Confirm"), wxYES_NO);
          if (answer == wxYES)
          {
            autoBindControls(configs);
          }
	      }
        read_filenames(ChoiceConfig);
        ChoiceConfig->SetSelection(ChoiceConfig->FindString(wxString(cl_sel.front().c_str(), wxConvUTF8)));
      }
    }
  }
  else
  {
    wxMessageBox(_("Can't retrieve config list!"), _("Error"), wxICON_ERROR);
    return;
  }
}

void serialFrame::autoBindControls(wxArrayString configs)
{
  string dir = string(userConfigDir.mb_str(wxConvUTF8));

  wxString mod_config;

  wxArrayString ref_configs;
  for(unsigned int i=0; i<ChoiceConfig->GetCount(); i++)
  {
    ref_configs.Add(ChoiceConfig->GetString(i));
  }

  wxSingleChoiceDialog dialog(this, _("Select the reference config."), _("Auto-bind and convert"), ref_configs);

  if (dialog.ShowModal() == wxID_OK)
  {
    for(unsigned int j=0; j<configs.GetCount(); ++j)
    {
      ConfigurationFile configFile;
      mod_config = configs[j];

      int ret = configFile.ReadConfigFile(dir + string(mod_config.mb_str(wxConvUTF8)));

      if(ret < 0)
      {
        wxMessageBox(_("Can't read config: ") + mod_config + wxString(configFile.GetError().c_str(), wxConvUTF8), _("Error"), wxICON_ERROR);
        return;
      }

      if(configFile.AutoBind(dir + string(dialog.GetStringSelection().mb_str(wxConvUTF8))) < 0)
      {
        wxMessageBox(_("Can't auto-bind controls for config: ") + mod_config, _("Error"), wxICON_ERROR);
      }
      else
      {
        configFile.ConvertSensitivity(dir + string(dialog.GetStringSelection().mb_str(wxConvUTF8)));
        if(configFile.WriteConfigFile() < 0)
        {
          wxMessageBox(_("Can't write config: ") + mod_config, _("Error"), wxICON_ERROR);
        }
        else
        {
          wxMessageBox(_("Auto bind done for ") + mod_config, _("Info"), wxICON_INFORMATION);
        }
      }
    }
  }
}

void serialFrame::OnMenuAutoBindControls(wxCommandEvent& event)
{
  if(ChoiceConfig->GetStringSelection().IsEmpty())
  {
    wxMessageBox( _("No config selected!"), _("Error"), wxICON_ERROR);
    return;
  }

  wxArrayString configs;
  configs.Add(ChoiceConfig->GetStringSelection());

  autoBindControls(configs);
}

void serialFrame::OnMenuOpenConfigDirectory(wxCommandEvent& event)
{
#ifdef WIN32
  userConfigDir.Replace(wxT("/"), wxT("\\"));
  wxExecute(wxT("explorer ") + userConfigDir, wxEXEC_ASYNC, NULL);
#else
  wxExecute(wxT("xdg-open ") + userConfigDir, wxEXEC_ASYNC, NULL);
#endif
}
