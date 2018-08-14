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

#include <gtkmm.h>

#include "settings.h"
#include "ps_helper.h"

class SetDlg {
 private:
  Psv *m_ps;
  string m_ext;
  
  Glib::RefPtr< Gtk::ListStore > m_store;
  Gtk::TreeModelColumnRecord m_cols;
  Gtk::TreeModelColumn<Glib::ustring> m_name_col;
  
  void *m_callback;
  
  Gtk::Dialog *m_dlg;
  Gtk::SearchEntry *m_search;
  Gtk::TreeView *m_tree;
  Gtk::Label    *m_name;
  Gtk::Label    *m_unit;
  Gtk::Label    *m_type;
  Gtk::TextView *m_description;
  Gtk::TextView *m_formula;
  Gtk::TextView *m_default;
  Gtk::Entry    *m_new;
  
  Glib::RefPtr<Gtk::TextBuffer> m_description_buffer;
  Glib::RefPtr<Gtk::TextBuffer> m_formula_buffer;
  Glib::RefPtr<Gtk::TextBuffer> m_default_buffer;
  Glib::RefPtr<Gtk::EntryBuffer> m_search_buffer;
  Glib::RefPtr<Gtk::EntryBuffer> m_new_buffer;
  
 public:
  SetDlg(Glib::RefPtr<Gtk::Builder> builder, const Psv *ps);
  ~SetDlg();
  void Show(Gtk::Window *parent, string &ext, void *callback);

 private:
  void Cancel(void);
  void Set(void);
  void SelectionChanged(void);
  void SearchChanged(void);
};
