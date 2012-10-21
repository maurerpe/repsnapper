/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010  Kulitorum
    Copyright (C) 2011  Michael Meeks <michael.meeks@novell.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "stdafx.h"
#include "printer.h"
#include "model.h"
#include "ui/view.h"
#include "ui/progress.h"

// everything taken out of model2.cpp

bool Printer::SwitchPower(bool on)
{
  if (!connected)
    return false;
  if(printing)
    {
      alert(_("Can't switch power while printing"));
      return false;
    }
  if (on)
    return SendNow("M80");
  else
    return SendNow("M81");
}


bool Printer::Home(string axis)
{
  if (!connected)
    return false;
  if(printing)
    {
      alert(_("Can't go home while printing"));
      return false;
    }
  if(axis == "X" || axis == "Y" || axis == "Z")
    {
      return SendNow("G28 "+axis+"0");
    }
  else if(axis == "ALL")
    {
      return SendNow("G28");
    }

  alert(_("Home called with unknown axis"));
  return false;
}

bool Printer::Move(string axis, double distance)
{
  if (!connected)
    return false;
  assert (m_model != NULL);
  Settings *settings = &m_model->settings;
  if (printing)
    {
      alert(_("Can't move manually while printing"));
      return false;
    }
  if (axis == "X" || axis == "Y" || axis == "Z")
    {
      if (!SendNow("G91")) return false;	// relative positioning
      float speed  = 0;
      if(axis == "Z")
	speed = settings->Hardware.MaxMoveSpeedZ * 60;
      else
	speed = settings->Hardware.MaxMoveSpeedXY * 60;

      std::ostringstream oss;
      // oss << "G1 F" << speed;
      // if (!SendNow(oss.str())) return false;
      oss.str("");
      oss << "G1 " << axis << distance << " F" << speed;
      if (!SendNow(oss.str())) return false;
      return SendNow("G90");	// absolute positioning
    }
  alert (_("Move called with unknown axis"));
  return false;
}

bool Printer::Goto(string axis, double position)
{
  if (!connected)
    return false;
  assert (m_model != NULL);
  Settings *settings = &m_model->settings;
  if (printing)
    {
      alert (_("Can't move manually while printing"));
      return false;
    }
  if(axis == "X" || axis == "Y" || axis == "Z")
    {
      std::stringstream oss;
      float speed  = 0;
      if(axis == "Z")
	speed = settings->Hardware.MaxMoveSpeedZ * 60;
      else
	speed = settings->Hardware.MaxMoveSpeedXY * 60;

      oss << "G1 F" << speed;
      if (!SendNow(oss.str())) return false;
      oss.str("");
      oss << "G1 " << axis << position << " F" << speed;
      return SendNow(oss.str());
    }

  alert (_("Goto called with unknown axis"));
  return false;
}

///////////////////////////////////////////////////////////////////
// these are common printer methods for all types of connections
// from printer.cpp


// we try to change the state of the connection
void Printer::serial_try_connect (bool connect)
{
  assert(m_model != NULL); // Need a model first

  if (connect) {
    // CONNECT:
    signal_serial_state_changed.emit (SERIAL_CONNECTING);
    if (do_connect(connect)) {
      signal_serial_state_changed.emit (SERIAL_CONNECTED);
      connected = true;
      UpdateTemperatureMonitor();
    } else {
      signal_serial_state_changed.emit (SERIAL_DISCONNECTED);
      connected = false;
      error (_("Failed to connect to device"),
             _("an error occured while connecting"));
    }
  } else {
    // DISCONNECT:
    if (printing) {
      error (_("Cannot disconnect"),
	     _("printer is printing"));
      signal_serial_state_changed.emit (SERIAL_CONNECTED);
    }
    else {
      signal_serial_state_changed.emit (SERIAL_DISCONNECTING);
      if (do_connect(connect)) { // do disconnect
	signal_serial_state_changed.emit (SERIAL_DISCONNECTED);
	connected = false;
	Pause();
	temp_timeout.disconnect();
      }	else {
	signal_serial_state_changed.emit (SERIAL_CONNECTED);
	connected = true;
      }
    }
  }
}


void Printer::alert (const char *message)
{
  if (m_view) m_view->err_log(string(message)+"\n");
  signal_alert.emit (Gtk::MESSAGE_INFO, message, NULL);
}

void Printer::error (const char *message, const char *secondary)
{
  if (m_view) m_view->err_log(string(message)  + " - " + string(secondary)+"\n");
  signal_alert.emit (Gtk::MESSAGE_ERROR, message, secondary);
}

bool Printer::temp_timeout_cb()
{
  if (!connected)
    return false;
  if (m_model && m_model->settings.Misc.TempReadingEnabled)
    SendNow("M105");
  UpdateTemperatureMonitor();
  return true;
}

void Printer::UpdateTemperatureMonitor()
{
  if (temp_timeout.connected())
    temp_timeout.disconnect();
  if (m_model && m_model->settings.Misc.TempReadingEnabled) {
    const unsigned int seconds = m_model->settings.Display.TempUpdateSpeed;
    temp_timeout = Glib::signal_timeout().connect_seconds
      (sigc::mem_fun(*this, &Printer::temp_timeout_cb), seconds);
  }
}

void Printer::setModel(Model *model)
{
  m_model = model;
  UpdateTemperatureMonitor();
}

void Printer::ContinuePauseButton(bool paused)
{
  if (!connected)
    return;
  if (paused)
    Pause();
  else
    Continue();
}

// void Printer::Kick()
// {
//   cerr << "Kick" << endl;
//   Continue();
// }

void Printer::ResetButton()
{
  if (!connected)
    return;
  Reset();
}

void Printer::PrintButton()
{
  if (!connected)
    return;
  simulation = false;
  Print();
}


void Printer::Simulate()
{
  simulation = true;
  current_printing_lineno = 0;
  Print();
}

void Printer::StopButton()
{
  if (!connected)
    return;
  Stop();
}

bool Printer::SelectExtruder(int extruder_no)
{
  if (connected && extruder_no >= 0){
    ostringstream os;
    os << "T" << extruder_no;
    return SendNow(os.str());
  }
  return true; // do nothing
}

bool Printer::SetTemp(TempType type, float value, int extruder_no)
{
  if (!connected)
    return false;
  ostringstream os;
  switch (type) {
  case TEMP_NOZZLE:
    os << "M104 S";
    break;
  case TEMP_BED:
    os << "M140 S";
    break;
  default:
    cerr << "No such Temptype: " << type << endl;
    return false;
  }
  os << value << endl;
  if (extruder_no >= 0)
    if (!SelectExtruder(extruder_no)) return false;
  return SendNow(os.str());
}


void Printer::SimplePrint()
{
  if (!connected)
    serial_try_connect (true);
  Print();
}

bool Printer::RunExtruder (double extruder_speed, double extruder_length,
			   int extruder_no, char extruder_char)
{
  if (!connected)
    return false;
  if (printing) {
    alert(_("Can't extrude manually while printing"));
    return false;
  }
  assert(m_model != NULL); // Need a model first

  if (extruder_no >= 0)
    if (!SelectExtruder(extruder_no)) return false;

  std::stringstream oss;
  string command("G1 F");
  oss << extruder_speed;
  command += oss.str();
  if (!SendNow(command)) return false;
  oss.str("");
  oss << "G92 " << extruder_char << "0";
  // set extruder zero
  if (!SendNow(oss.str())) return false;
  oss.str("");
  oss << "G1 " << extruder_char;
  string command2(oss.str());

  // if (reverse)
  //   oss << "-";
  oss <<  extruder_length;
  if (!SendNow(oss.str())) return false;
  if (!SendNow("G1 F1500.0")) return false;
  oss.str("");
  oss << "G92 " << extruder_char << "0";
  return SendNow(oss.str());	// set extruder zero
}

void Printer::set_printing (bool pprinting)
{
  if (printing == pprinting)
    return;
  printing = pprinting;
  if (m_view) {
    if (printing){
      if (gcode_iter) {
	Glib::ustring label = _("Printing");
	if (simulation) label = _("Simulation");
	m_view->get_view_progress()->start (label.c_str(), gcode_iter->m_line_count);
      }
    } else {
      //signal_now_printing.emit(0);
      //m_view->showCurrentPrinting(0);
      m_view->get_view_progress()->stop (_("Done"));
    }
  }

  printing_changed.emit();
}




const Glib::RefPtr<Glib::Regex> templineRE_T =
	  Glib::Regex::create("(?ims)T\\:(?<temp>[\\-\\.\\d]+?)\\s+?");
const Glib::RefPtr<Glib::Regex> templineRE_B =
	  Glib::Regex::create("(?ims)B\\:(?<temp>[\\-\\.\\d]+?)\\s+?");
const Glib::RefPtr<Glib::Regex> numRE =
	  Glib::Regex::create("(?ims)\\:(?<num>[\\-\\.\\d]+)");


void Printer::parse_response (string line)
{
  string line_lower = line;
  std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(), ::tolower);

  if (line_lower.find("resend:") != string::npos
      || line.find("rs:") != string::npos ) {
    Glib::MatchInfo match_info;
    vector<string> matches;
    if (numRE->match(line, match_info)) {
      std::istringstream iss(match_info.fetch_named("num").c_str());
      unsigned long lineno; iss >> lineno;
      unsigned long gcodeline  = set_resend(lineno);
      cerr << "RESEND line " << lineno << " is code line " << gcodeline << endl;
      return;
    }
    log(line, LOG_ERROR);
  }
  if (line_lower.find("ok") == 0) { // ok at beginning
    log(line, LOG_COMM);
    // cerr << "-- OK --" <<endl;
  }
  if (line.find("T:") != string::npos) {
    //cerr << line << " - " << status << endl;
    Glib::MatchInfo match_info;
    vector<string> matches;
    string name;
    if (templineRE_T->match(line, match_info)) {
      std::istringstream iss(match_info.fetch_named("temp").c_str());
      double temp; iss >> temp;
      //cerr << "nozzle: " << temp << endl;
      temps[TEMP_NOZZLE] = temp;
    }
    if (templineRE_B->match(line, match_info)) {
      std::istringstream iss(match_info.fetch_named("temp").c_str());
      double temp; iss >> temp;
      temps[TEMP_BED] = temp;
      //cerr << "bed: " << temp << endl;
    }
    signal_temp_changed.emit();
  }
  else if (line_lower.find("echo:") != string::npos) {
    log(line, LOG_ECHO);
    //cerr << line << endl;
  }
  else if (line_lower.find("error") != string::npos) {
    log(line, LOG_ERROR);
    //cerr << line << endl;
  }
  else {
    if (line.length()>0)
      log(line, LOG_COMM);
    //cerr  << line << endl;
  }
}


double Printer::get_temp (TempType t)
{
  return temps[(int)t];
}




#include <fcntl.h>

bool Printer::test_port(const string serialname)
{
    // try {
    //   Glib::RefPtr<Glib::IOChannel> ioc =
    // 	Glib::IOChannel::create_from_fd(ports[i], "w+");
    //   ioc->close(false);
    //   cerr << "device " << ports[i] << " is connectable" << endl;
    // }
    // catch (FileError e) {
    //   ports.erase(ports.begin()+i);
    // }
  int device_fd = open(serialname.c_str(), O_RDWR );
  if (device_fd < 0) {
    return false;
  }
  close(device_fd);
  return true;
}


#ifdef WIN32
#define DEV_PATH ""
#define DEV_PREFIXES {"COM"}
#else
#define DEV_PATH "/dev/"
#define DEV_PREFIXES {"ttyUSB", "ttyACM", "cuaU"}
#endif


// not for libreprap
#if PYSERIAL || PRINTRUN || IOCHANNEL

vector<string> Printer::find_ports() const
{

  vector<string> ports;


#ifdef WIN32
  // FIXME how to find ports on windows?
  ports.push_back("COM1");
  ports.push_back("COM2");
  ports.push_back("COM3");
  ports.push_back("COM4");
  ports.push_back("COM5");
  ports.push_back("COM6");
  ports.push_back("COM7");
  ports.push_back("COM8");
  ports.push_back("COM9");
#else

  Glib::Dir dir(DEV_PATH);
  while (true) {
    const string dev_prefixes[] = DEV_PREFIXES;
    const size_t npref = sizeof(dev_prefixes)/sizeof(string);
    string name = dir.read_name();
    if (name == "") break;
    for(size_t i = 0; i < npref; i++) {
      if (name.find(dev_prefixes[i]) == 0) {
	//cerr << i << " found port " << name << endl;
	ports.push_back(DEV_PATH+name);
	break;
      }
    }
  }
  for (int i = ports.size()-1; i >= 0; i--) {
    if (!test_port(ports[i]))
      ports.erase(ports.begin()+i);
    else
      cerr << "Found some device at " << ports[i] << endl;
  }
#endif

  return ports;
}

#endif

// double Printer::getCurrentPrintingZ() {
//   if (gcode_iter){
//     Command command = gcode_iter->getCurrentCommand(Vector3d(0,0,0));
//     return command.where.z();
//   }
//   return 0;
// }

