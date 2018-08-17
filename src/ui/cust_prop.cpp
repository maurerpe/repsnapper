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

#include "cust_prop.h"

CustProp::CustProp(SetDlg *set, const Psv *ps) : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL), m_ps(ps), m_new("New"), m_edit("Edit"), m_delete("Delete"), m_buttonbox(Gtk::ORIENTATION_VERTICAL) {
  m_set = set;
  m_window = NULL;
  m_value = NULL;
  
  m_new.signal_clicked().connect(sigc::mem_fun(*this, &CustProp::New));
  m_edit.signal_clicked().connect(sigc::mem_fun(*this, &CustProp::Edit));
  m_delete.signal_clicked().connect(sigc::mem_fun(*this, &CustProp::Delete));
  
  m_buttonbox.add(m_new);
  m_buttonbox.add(m_edit);
  m_buttonbox.add(m_delete);
  
  m_cols.add(m_name_col);
  m_cols.add(m_value_col);
  m_store = Gtk::ListStore::create(m_cols);
  m_tree.set_model(m_store);
  m_tree.set_headers_visible(true);
  m_tree.append_column("Name", m_name_col);
  m_tree.append_column("Value", m_value_col);
  
  m_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  m_scroll.property_width_request().set_value(400);
  m_scroll.property_height_request().set_value(200);
  m_scroll.add(m_tree);

  add(m_scroll);
  add(m_buttonbox);
  show_all();
}

void CustProp::SetWindow(Gtk::Window *window) {
  m_window = window;
}

void CustProp::SetValue(const char *ext, ps_value_t *value) {
  m_ext = ext;
  m_value = value;

  BuildStore();
}

void CustProp::BuildStore(void) {
  m_store->clear();

  if (m_value == NULL)
    return;
  
  Psvi vi(m_value);
  while (vi.Next()) {
    Gtk::TreeModel::Row row = *m_store->append();
    row[m_name_col] = Glib::ustring(vi.Key());
    row[m_value_col] = PS_ToString(vi.Data());
  }
}

void CustProp::New(void) {
  m_set->signal_set().connect
    (sigc::mem_fun(*this, &CustProp::Set));  
  m_set->Show(m_window, m_ext.c_str());
}

void CustProp::Edit(void) {
  vector< Gtk::TreeModel::Path > paths = m_tree.get_selection()->get_selected_rows();
  if (paths.size() == 0)
    return;
  Gtk::TreeModel::Row row = *m_store->children()[paths.front().back()];
  Glib::ustring name = row[m_name_col];
  
  m_set->signal_set().connect
    (sigc::mem_fun(*this, &CustProp::Set));  
  m_set->Show(m_window, m_ext.c_str(), name.c_str());
}

void CustProp::Delete(void) {
  vector< Gtk::TreeModel::Path > paths = m_tree.get_selection()->get_selected_rows();
  if (paths.size() == 0)
    return;
  Gtk::TreeModel::Row row = *m_store->children()[paths.front().back()];
  Glib::ustring name = row[m_name_col];
  
  PS_RemoveMember(m_value, name.c_str());
  
  BuildStore();
}

void CustProp::Set(Glib::ustring ext, Glib::ustring name, const ps_value_t *v) {
  if (m_value == NULL) {
    if ((m_value = PS_NewObject()) == NULL)
      return;
  }
  
  ps_value_t *c = PS_CopyValue(v);
  if (c == NULL)
    return;
  
  if (PS_AddMember(m_value, name.c_str(), c) < 0) {
    PS_FreeValue(c);
    return;
  }
  
  BuildStore();
}
