/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2011 Michael Meeks
    Copyright (C) 2011-12 martin.dieringer@gmx.de

    This program is free software; you can redistribute it and/or modify
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


#include <libreprap/util.h>


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
  device = NULL;
}

Printer::~Printer()
{
  temp_timeout.disconnect();
  Stop();
  serial_try_connect(false);
  if (device==NULL) return;
  rr_dev_free (device);
  device = NULL;
}

// direct output
void Printer::err_log(string s)
{
  m_view->err_log(s);
}
void Printer::echo_log(string s)
{
  m_view->echo_log(s);
}
void Printer::comm_log(string s)
{
  m_view->comm_log(s);
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
  if (device==NULL) return;
  rr_dev_set_paused (device, RR_PRIO_NORMAL, 1);
}


void Printer::Continue()
{
  set_printing (true);
  if (device==NULL) return;
  rr_dev_set_paused (device, RR_PRIO_NORMAL, 0);
}

void Printer::Reset()
{
  if (device==NULL) return;
  Stop();
  rr_dev_reset_device(device);
}

bool Printer::IsConnected()
{
  if (device==NULL) return false;
  return rr_dev_is_connected(device);
}


// we try to change the state of the connection
bool Printer::do_connect (bool connect)
{
  int result;
  assert(m_model != NULL); // Need a model first

  if (connect) {
    void *cl = static_cast<void *>(this);
    // TODO: Configurable protocol, cache size
    device = rr_dev_create (RR_PROTO_FIVED,
			    m_model->settings.Hardware.ReceivingBufferSize,
			    rr_reply_fn, cl,
			    rr_more_fn, cl,
			    rr_error_fn, cl,
			    rr_wait_wr_fn, cl,
			    rr_log_fn, cl);

    result = rr_dev_open (device,
			  m_model->settings.Hardware.PortName.c_str(),
			  m_model->settings.Hardware.SerialSpeed);

    if(result < 0) {
      return false;
    } else {
      rr_dev_reset (device);
      return true;
    }
  } else {
    devconn.disconnect();
    if (device)
      rr_dev_close (device);
    devconn.disconnect();
    if (device)
      rr_dev_free (device);
    device = NULL;
    return true;
  }
}


void Printer::Print()
{
  if (device==NULL) return;
  if (!rr_dev_is_connected (device)) {
    alert (_("Not connected to printer.\nCannot start printing"));
    return;
  }

  assert(m_model != NULL);

  delete (gcode_iter);
  gcode_iter = m_model->gcode.get_iter();
  set_printing (true);

  rr_dev_reset (device);
  rr_dev_set_paused (device, RR_PRIO_NORMAL, 0);
}


long Printer::get_next_line(string &line)
{
  return -1;
}



bool Printer::SendNow(string str, long lineno)
{
  //if (str.length() < 1) return true;
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);

  if (device==NULL) return false;
  if (rr_dev_is_connected (device)) {
    rr_dev_enqueue_cmd (device, RR_PRIO_HIGH, str.data(), str.size());
  } else {
    error (_("Can't send command"), _("You must first connect to a device!"));
    return false;
  }
  return true;
}

void Printer::Stop()
{
  if (device==NULL) return;

  set_printing (false);
  //assert(m_model != NULL);

  if (!rr_dev_is_connected (device)) {
    alert (_("Not connected to printer.\nCannot stop printing"));
    return;
  }
  rr_dev_reset (device);
}


vector<string> Printer::find_ports() const
{

  vector<string> ports;

  char **rr_ports = rr_enumerate_ports();
  if (rr_ports == NULL) {
    return ports;
  }
  for(size_t i = 0; rr_ports[i] != NULL; ++i) {
    ports.push_back((string)rr_ports[i]);
    free(rr_ports[i]);
  }
  free(rr_ports);

  return ports;
}




// ------------------------------ libreprap integration ------------------------------

bool Printer::handle_dev_fd (Glib::IOCondition cond)
{
  if (device==NULL) return false;
  int result;
  if (cond & Glib::IO_IN) {
    result = rr_dev_handle_readable (device);
    if (result < 0)
      error (_("Error reading from device!"), std::strerror (errno));
  }
  // try to avoid reading and writing at exactly the same time
  else if (cond & Glib::IO_OUT) {
    result = rr_dev_handle_writable (device);
    if (result < 0)
      error (_("Error writing to device!"), std::strerror (errno));
  }
  if (cond & (Glib::IO_ERR | Glib::IO_HUP))
    serial_try_connect (false);

  return true;
}

void RR_CALL Printer::rr_reply_fn (rr_dev dev, int type, float value,
				 void *expansion, void *closure)
{
  Printer *printer = static_cast<Printer *>(closure);
  printer->handle_rr_reply(dev, type, value, expansion);
}

void
Printer::handle_rr_reply(rr_dev dev, int type, float value, void *expansion)
{
  switch (type) {
  case RR_NOZZLE_TEMP:
    temps[TEMP_NOZZLE] = value;
    signal_temp_changed.emit();
    break;
  case RR_BED_TEMP:
    temps[TEMP_BED] = value;
    signal_temp_changed.emit();
    break;
  default:
    break;
  }
}

void RR_CALL Printer::rr_more_fn (rr_dev dev, void *closure)
{
  Printer *printer = static_cast<Printer*>(closure);
  printer->handle_rr_more(dev);
}


void Printer::handle_rr_more (rr_dev dev)
{
  if (printing && gcode_iter) {
    time_t time_used = time(NULL) - gcode_iter->time_started;
    if (time_used != lasttimeshown) { // show once a second
      int n_buffered = rr_dev_buffered_lines(device);
      int donelines = gcode_iter->m_cur_line - n_buffered;
      if (donelines < 100) gcode_iter->time_started = time(NULL);
      //int tot_lines = gcode_iter->m_line_count;
      // done by view
      // if (tot_lines>0) {
      // 	if (donelines > 30) {
      // 	  m_view->get_view_progress()->update (donelines, false);
      // 	}
      // }
      if (lastdonelines > 0) // don't stop the progress bar
	m_view->showCurrentPrinting(lastdonelines);
      lastdonelines = donelines;
      lasttimeshown = time_used;
    }
    while (rr_dev_write_more (device) && !gcode_iter->finished()) {
      std::string line = gcode_iter->next_line_stripped();
      if (line.length() > 0 && line[0] != ';') {
	rr_dev_enqueue_cmd (device, RR_PRIO_NORMAL, line.data(), line.size());
      }
    }

    if (gcode_iter->finished())
    {
      set_printing (false);
      m_view->showCurrentPrinting(0);
      //m_view->get_view_progress()->stop (_("Printed"));
    }
  }
}


void RR_CALL Printer::rr_error_fn (rr_dev dev, int error_code,
				 const char *msg, size_t len,
				 void *closure)
{
  Printer *printer = static_cast<Printer*>(closure);

  printer->handle_rr_error (dev, error_code, msg, len);
}


void
Printer::handle_rr_error (rr_dev dev, int error_code,
    const char *msg, size_t len)
{
  err_log(msg);
  // char *str = g_strndup (msg, len);
  // g_warning (_("Error (%d) '%s' - user popup ?"), error_code, str);
  // g_free (str);
}


void RR_CALL Printer::rr_wait_wr_fn (rr_dev dev, int wait_write, void *closure)
{
  Printer *printer = static_cast<Printer*>(closure);

  printer->handle_rr_wait_wr (dev, wait_write);
}

void
Printer::handle_rr_wait_wr (rr_dev dev, int wait_write)
{
#ifndef WIN32
  Glib::IOCondition cond = (Glib::IO_ERR | Glib::IO_HUP |
			    Glib::IO_PRI | Glib::IO_IN);
  if (wait_write)
    cond |= Glib::IO_OUT;

  // FIXME: it'd be way nicer to change the existing poll record
  devconn.disconnect();
  devconn = Glib::signal_io().connect
    (sigc::mem_fun (*this, &Printer::handle_dev_fd), rr_dev_fd (dev), cond);
#else
  /* FIXME: Handle the win32 case */
#endif
}

void RR_CALL Printer::rr_log_fn (rr_dev dev, int type,
			       const char *buffer,
			       size_t len, void *closure)
{
  Printer *printer = static_cast<Printer*>(closure);
  printer->handle_rr_log(dev, type, buffer, len);
}

void
Printer::handle_rr_log (rr_dev dev, int type, const char *buffer, size_t len)
{
  bool recvsend = ( m_model && m_model->settings.Printer.Logging) ;

  string str;

  switch (type) {
  case RR_LOG_RECV:
    if (!recvsend) return;
    str = "<-- ";
    break;
  case RR_LOG_SEND:
    if (!recvsend) return;
    str = "--> ";
    break;
  case RR_LOG_MSG:
  default:
    str = "; ";
    break;
  }
  str += string (buffer, len);
  comm_log(str);
}



// ------------------------------ libreprap integration above ------------------------------

// never called in libreprap version
void Printer::parse_response (string line)
{
}
