/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2012 martin.dieringer@gmx.de

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

#pragma once

#include "stdafx.h"
#include "python2.7/Python.h"


class Printer
{

  PyObject *pyPrintCore, *pyName, *pyModule, *pyDict, *pyFunc, *pyValue, *pyArgs, *pyCall;
  PyObject * p_get_attr(const char * name) const;
  PyThreadState * pyThreadState;

  Glib::Thread * listen_thread;
  void listen_thread_run ();
  bool run_listen;
  Glib::Mutex print_mutex;

  bool printing;
  void run_print ();

  unsigned long unconfirmed_lines;

  unsigned long lastdonelines; // for live view
  time_t lasttimeshown;

  bool debug_output;

  SerialState serial_state;
  void set_printing (bool printing);
  void update_core_settings();

  double temps[TEMP_LAST];
  sigc::connection temp_timeout;
  bool temp_timeout_cb();

  Glib::TimeVal print_started_time;
  Glib::TimeVal total_time_to_print ;

  View *m_view;
  Model *m_model;


  /*#if IOCHANNEL*/
  sigc::connection watchprint_timeout;
  bool watchprint_timeout_cb();

  sigc::connection log_timeout;
  bool log_timeout_cb();

  //RRSerial *rr_serial;
  /* #endif */

 public:
  Printer(View *view);
  ~Printer();


  vector<string> find_ports() const;


  sigc::signal< void, unsigned long > signal_now_printing;

  bool inhibit_print;
  sigc::signal< void > signal_inhibit_changed;

  sigc::signal< void, Gtk::MessageType, const char *, const char * > signal_alert;
  void alert (const char *message);
  void error (const char *message, const char *secondary);

  SerialState get_serial_state () { return serial_state; }
  bool do_connect (bool connect);
  void serial_try_connect (bool connect);
  sigc::signal< void, SerialState > signal_serial_state_changed;

  double get_temp (TempType t) { return temps[(int)t]; }
  sigc::signal< void > signal_temp_changed;

  sigc::signal< void > printing_changed;

  sigc::signal< void > get_signal_inhibit_changed() { return signal_inhibit_changed; }
  bool get_inhibit_print() { return inhibit_print; }

  bool IsPrinting() { return printing; }

  void setModel(Model *model);

  bool IsConnected();
  void SimplePrint();

  void Print();
  void Pause();
  void Stop();
  void Continue();
  //void Kick();
  void Reset();

  bool SetTemp(TempType type, float value, int extruder_no=-1);

  bool SelectExtruder(int extruder_no=-1);

  long current_printing_lineno;

  /* double getCurrentPrintingZ(); */

  bool RunExtruder(double extruder_speed, double extruder_length, bool reverse,
		   int extruder_no=-1, char extruder_char='E');
  bool Send(string str);
  bool SendNow(string str);

  void parse_response (string line);

  void EnableTempReading(bool on);
  void SetLogFileClear(bool on);
  bool SwitchPower(bool on);
  void UpdateTemperatureMonitor();

  bool Home(string axis);
  bool Move(string axis, double distance);
  bool Goto(string axis, double position);

  sigc::signal< void, string, RR_logtype > signal_logmessage;

  string commlog_buffer, error_buffer, echo_buffer;

  void log(string s, RR_logtype type);

  void PrintButton();
  void StopButton();
  void ContinuePauseButton(bool paused);
  void ResetButton();

  unsigned long set_resend(unsigned long print_lineno);

  long get_next_line(string &line);

  const Model * getModel() const {return m_model; };

  static bool test_port(const string serialname);

 private:
  GCodeIter *gcode_iter;
};
