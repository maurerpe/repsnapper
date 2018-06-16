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

#include <gtkmm.h>

#include "model.h"
#include "progress.h"

ViewProgress::ViewProgress(Gtk::Box *box, Gtk::ProgressBar *bar) :
  m_box (box), m_bar(bar), do_continue(false), to_terminal(true) {
  m_bar_max = 0.0;
  box->hide();
}

void ViewProgress::start(const char *label, double max) {
  do_continue = true;
  m_box->show();
  m_bar_max = max;
  this->label = label;
  m_bar_cur = 0.0;
  m_bar->set_fraction(0.0);
  start_time.assign_current_time();
  Gtk::Main::iteration(false);
}

bool ViewProgress::restart(const char *label, double max) {
  if (!do_continue) return false;
  if (to_terminal) {
    Glib::TimeVal now;
    now.assign_current_time();
    const int time_used = (int) round((now - start_time).as_double()); // seconds
    cerr << this->label << " -- " << _(" done in ") << time_used << _(" seconds") << "       " << endl;
  }
  m_box->show();
  m_bar_max = max;
  this->label = label;
  m_bar_cur = 0.0;
  m_bar->set_fraction(0.0);
  start_time.assign_current_time();
  Gtk::Main::iteration(false);
  return true;
}

void ViewProgress::stop(const char *label) {
  if (to_terminal) {
    Glib::TimeVal now;
    now.assign_current_time();
    const int time_used = (int) round((now - start_time).as_double()); // seconds
    cerr << this->label << " -- " << _(" done in ") << time_used << _(" seconds") << "       " << endl;
  }
  this->label = label;
  m_bar_cur = m_bar_max;
  m_bar->set_fraction(1.0);
  m_box->hide();
  Gtk::Main::iteration(false);
}

string timeleft_str(long seconds) {
  ostringstream ostr;
  int hrs = (int)(seconds/3600);
  if (hrs>0) {
    if (hrs>1) ostr << hrs << _("h ");
    else ostr << hrs << _("h ");
    seconds -= 3600*hrs;
  }
  if (seconds > 60)
    ostr << (int)seconds/60 << _("m ");
  if (hrs == 0 && seconds<300)
    ostr << (int)seconds%60 << _("s");
  return ostr.str();
}


bool ViewProgress::update(const double value, bool take_priority, double time_left) {
  // Don't allow progress to go backward
  if (value < m_bar_cur)
    return do_continue;

  m_bar_cur = CLAMP(value, 0, 1.0);
  m_bar->set_fraction(value / m_bar_max);
  ostringstream o;

  if (value > 0) {
    if (time_left < 0) {
      Glib::TimeVal now;
      now.assign_current_time();
      const double used = (now - start_time).as_double(); // seconds
      const double total = used * m_bar_max  / value;
      time_left = (long)(total-used);
    }
    o << (label+" ("+timeleft_str(time_left)+")");
  } else {
    o << label;
  }

  o << ": " << ((int) round(100.0 * value / m_bar_max)) << '%';
  m_bar->set_text(o.str());
  if (to_terminal) {
    int perc = (int(m_bar->get_fraction()*100));
    cerr << this->label << " " << o.str() << " -- " << perc << "%              \r";
  }
  
  if (take_priority)
    while(gtk_events_pending())
      gtk_main_iteration();
  Gtk::Main::iteration(false);
  return do_continue;
}

const char *Prog::stopText = _("Done");
