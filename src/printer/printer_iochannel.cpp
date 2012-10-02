/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2012 martin.dieringer@gmx.de

    This program is frpree software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "reprap_serial.h"


#ifdef WIN32
#define DEV_PATH ""
#define DEV_PREFIXES {"COM"}
#else
#define DEV_PATH "/dev/"
#define DEV_PREFIXES {"ttyUSB", "ttyACM", "cuaU"}
#endif

// everything taken out of model.cpp

Printer::Printer(View *view) :
  printing (false),
  lastdonelines(0),
  lasttimeshown(0),
  debug_output(false),
  m_model(NULL),
  inhibit_print (false)
{
  gcode_iter = NULL;
  m_view = view;

  rr_serial = new RRSerial(this);
  // rr_serial->signal_logmessage.connect
  //   (sigc::mem_fun(*this, &Printer::log));
  commlog_buffer = "";
  error_buffer   = "";
  echo_buffer    = "";
}

Printer::~Printer()
{
  temp_timeout.disconnect();
  Stop();
  serial_try_connect(false);
  delete rr_serial;
}

// buffered output
void Printer::log(string s, RR_logtype type)
{
  switch (type) {
  case LOG_ERROR: error_buffer  +=s; break;
  case LOG_ECHO:  echo_buffer   +=s; break;
  default:
  case LOG_COMM:  commlog_buffer+=s; break;
  }
}

bool Printer::log_timeout_cb()
{
  // signal_logmessage.emit(s,type);
  if (commlog_buffer != "") m_view->comm_log(commlog_buffer); commlog_buffer  = "";
  if (error_buffer != "")   m_view->err_log (error_buffer);   error_buffer  = "";
  if (echo_buffer != "")    m_view->echo_log(echo_buffer);    echo_buffer = "";
  return true;
}

// void Printer::update_core_settings ()
// {
//   if (m_model) {
// #if IOCHANNEL
//     debug_output = m_model->settings.Display.CommsDebug;
// #else
//     if (device==NULL) return;
//     rr_dev_enable_debugging(device, m_model->settings.Display.CommsDebug);
// #endif
//   }
// }


void Printer::Pause()
{
  set_printing (false);
  if (!IsConnected()) return;
}

void Printer::Continue()
{
  set_printing (true);
  if (!IsConnected()) return;
  rr_serial->start_printing();
}

void Printer::Reset()
{
  //if (!IsConnected()) return;
  Stop();
  rr_serial->reset_printer();
}

bool Printer::IsConnected()
{
  return rr_serial->connected();
}


// we try to change the state of the connection
bool Printer::do_connect (bool connect)
{
  if (connect) {
    const char * serialname = m_model->settings.Hardware.PortName.c_str();
    bool connected = rr_serial->connect(serialname);
    if (connected) {
      log_timeout = Glib::signal_timeout().connect
        (sigc::mem_fun(*this, &Printer::log_timeout_cb), 500);
    }
    return connected;
  } else {
    bool disconnected  = rr_serial->disconnect();
    if (disconnected && log_timeout.connected())
      log_timeout.disconnect();
    return disconnected;
  }
}


void Printer::Print()
{
  if (!IsConnected()) {
    alert (_("Not connected to printer.\nCannot start printing"));
    return;
  }

  assert(m_model != NULL);

  delete (gcode_iter);
  gcode_iter = m_model->gcode.get_iter();
  set_printing (true);

  // reset printer line number
  rr_serial->start_printing(true, gcode_iter->m_line_count);
}


bool Printer::watchprint_timeout_cb()
{
  if (!IsConnected()) return true;
  int cur_line = gcode_iter->m_cur_line;
  //cerr << "watch "<< cur_line << endl;
  signal_now_printing.emit(cur_line);
  return true;
}

unsigned long Printer::set_resend(unsigned long print_lineno)
{
  return rr_serial->set_resend(print_lineno);
}


long Printer::get_next_line(string &line)
{
  if (gcode_iter && printing && !gcode_iter->finished()) {
    int cur_line = gcode_iter->m_cur_line;
    line = gcode_iter->next_line();
    if (line.length() > 0 && line[0] != ';' && line != "\n") {
      return cur_line; // return gcode line number
    } else
      return -1; // don't print this line
  }
  else {
    set_printing(false);
    if ( !gcode_iter || (gcode_iter && gcode_iter->finished()) ){
      signal_now_printing.emit(0);
    }
    return -1;
  }
}


bool Printer::SendNow(string str, long lineno)
{
  //if (str.length() < 1) return true;
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);
  if (rr_serial) {
    RR_response resp = rr_serial->send(str, lineno);
    bool ok = (resp == SEND_OK);
    //if (!ok) error (_("Can't send command"), _("You must first connect to a device!"));
    return ok;
  }
  return false;
}

void Printer::Stop()
{
  if (!IsConnected()) return;
  set_printing (false);
}
