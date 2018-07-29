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

#include <cstdlib>
#include <gtkmm.h>
#include "prefs_dlg.h"

void PrefsDlg::handle_response(Gtk::Dialog *dialog) {
  dialog->hide();
}

void PrefsDlg::handle_response_int(int, Gtk::Dialog *dialog) {
  dialog->hide();
}

PrefsDlg::PrefsDlg(Glib::RefPtr<Gtk::Builder> &builder) {
  builder->get_widget("preferences_dlg", m_preferences_dlg);
  m_preferences_dlg->set_icon_name("gtk-convert");
  m_preferences_dlg->signal_response().connect(
	sigc::bind(sigc::mem_fun(*this, &PrefsDlg::handle_response_int), m_preferences_dlg));
  Gtk::Button *close = NULL;
  builder->get_widget("prefs_close", close);
  if (close)
    close->signal_clicked().connect(
        sigc::bind(sigc::mem_fun(*this, &PrefsDlg::handle_response), m_preferences_dlg));
}

PrefsDlg::~PrefsDlg() {
}

bool PrefsDlg::load_settings() {
  return true;
}

void PrefsDlg::show(Gtk::Window &trans) {
  load_settings();
  m_preferences_dlg->set_transient_for(trans);
  m_preferences_dlg->show();
  m_preferences_dlg->raise();
}
