/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Michael Meeks

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

#include <string>
#include <gtkmm.h>

class ViewProgress {
  Gtk::Box *m_box;
  Gtk::ProgressBar *m_bar;
  double m_bar_max;
  double m_bar_cur;

  Glib::TimeVal start_time;
  string label;

  bool do_continue;
  bool to_terminal;
 public:
  void start(const char *label, double max);
  bool restart(const char *label, double max);
  void stop(const char *label = "");
  bool update(const double value, bool take_priority=true, double time_left = -1);
  ViewProgress(){};
  ViewProgress(Gtk::Box *box, Gtk::ProgressBar *bar);
  void set_terminal_output(bool terminal) {to_terminal=terminal;};
  void stop_running(){do_continue = false;};
};

class Prog {
 private:
  static const char *stopText;
  
  ViewProgress *vp;
  
 public:
 Prog(ViewProgress *prog, const char *label, double max) : vp(prog) {if (vp) vp->start(label, max);};
  ~Prog() {if (vp) vp->stop(stopText);};

  bool restart(const char *label, double max) {return vp ? vp->restart(label, max) : false;};
  bool update(const double value, bool take_priority=true, double time_left=-1) {return vp ? vp->update(value, take_priority, time_left) : false;};
};
