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
#include <unordered_set>

#include "ps_helper.h"
#include "set_dlg.h"

class CustProp : public Gtk::Box {
 private:
  Gtk::Window *m_window;
  SetDlg *m_set;
  const Psv *m_ps;
  Glib::ustring m_ext;
  ps_value_t *m_value;
  unordered_set< string > mask;
  
  int m_inhibit_extruder;
  Glib::RefPtr< Gtk::ListStore > m_store;
  Gtk::TreeModelColumnRecord m_cols;
  Gtk::TreeModelColumn<Glib::ustring> m_name_col;
  Gtk::TreeModelColumn<Glib::ustring> m_value_col;
  
  Gtk::TreeView m_tree;
  Gtk::Button   m_new;
  Gtk::Button   m_edit;
  Gtk::Button   m_delete;
  
  Gtk::Box m_buttonbox;
  Gtk::ScrolledWindow m_scroll;
  
 public:
  CustProp(SetDlg *set, const Psv *ps);

  void SetWindow(Gtk::Window *window);
  void SetValue(const char *ext, ps_value_t *value);
  ps_value_t *GetValue() {return m_value;};
  void AddToMask(string setting);

 private:
  void BuildStore(void);
  
  void New(void);
  void Edit(void);
  void Delete(void);
  
  void Set(Glib::ustring ext, Glib::ustring name, const ps_value_t *v);
};
