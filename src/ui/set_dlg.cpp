/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Michael Meeks
    Copyright (C) 2013  martin.dieringer@gmx.de

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

#include "set_dlg.h"

SetDlg::SetDlg(Glib::RefPtr<Gtk::Builder> builder, const Psv *ps) {
  m_ps = (Psv *) ps;
  
  builder->get_widget("set_dlg", m_dlg);
  builder->get_widget("set_search", m_search);
  builder->get_widget("set_tree", m_tree);
  builder->get_widget("set_name", m_name);
  builder->get_widget("set_default", m_default);
  builder->get_widget("set_unit", m_unit);
  builder->get_widget("set_type", m_type);
  builder->get_widget("set_description", m_description);
  builder->get_widget("set_formula", m_formula);
  builder->get_widget("set_new", m_new);

  m_description_buffer = Gtk::TextBuffer::create();
  m_formula_buffer     = Gtk::TextBuffer::create();
  m_default_buffer     = Gtk::TextBuffer::create();
  m_search_buffer      = Gtk::EntryBuffer::create();
  m_new_buffer         = Gtk::EntryBuffer::create();
  
  m_description->set_buffer(m_description_buffer);
  m_formula->set_buffer(m_formula_buffer);
  m_default->set_buffer(m_default_buffer);
  m_search->set_buffer(m_search_buffer);
  m_new->set_buffer(m_new_buffer);
  
  m_cols.add(m_name_col);
  m_store = Gtk::ListStore::create(m_cols);
  m_tree->set_model(m_store);
  m_tree->set_headers_visible(false);
  m_tree->append_column("Name", m_name_col);
  
  m_tree->get_selection()->signal_changed().connect
    (sigc::mem_fun(*this, &SetDlg::SelectionChanged));
  
  m_search->signal_changed().connect
    (sigc::mem_fun(*this, &SetDlg::SearchChanged));
  
  Gtk::Button *b;
  builder->get_widget("set_cancel", b);
  b->signal_clicked().connect
    (sigc::mem_fun(*this, &SetDlg::Cancel));
  
  builder->get_widget("set_set", b);
  b->signal_clicked().connect
    (sigc::mem_fun(*this, &SetDlg::Set));
}

SetDlg::~SetDlg() {
}

void SetDlg::Show(Gtk::Window *parent, string &ext, void *callback) {
  m_callback = callback;
  
  m_ext = ext;
  SearchChanged();
  
  if (parent)
    m_dlg->set_transient_for(*parent);
  m_dlg->show();
  m_dlg->raise();
}

void SetDlg::Cancel(void) {
  m_dlg->hide();
}

void SetDlg::Set(void) {
  vector< Gtk::TreeModel::Path > paths = m_tree->get_selection()->get_selected_rows();
  if (paths.size() == 0) {
    Gtk::MessageDialog dialog(_("No setting selected"), false,
                              Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
    dialog.set_transient_for(*m_dlg);
    dialog.run();
    return;
  }
  Gtk::TreeModel::Row row = *m_store->children()[paths.front().back()];
  Glib::ustring name = row[m_name_col];

  Psv v;
  
  try {
    v.Take(PS_ParseJsonString(m_new_buffer->get_text().c_str()));
    v();
  } catch (exception &e) {
    Gtk::MessageDialog dialog(_("Invalid value entered"), false,
                              Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
    dialog.set_transient_for(*m_dlg);
    dialog.run();
    return;
  }
  
  /* FIXME: Validate type */
  
  // cout << "Entered:" << endl;
  // Pso os(PS_NewFileOStream(stdout));
  // PS_WriteValue(os(), v());
  // fflush(stdout);
  // cout << endl;
  
  m_dlg->hide();
  
  /* FIXME: Execute callback */
}

static void SetLabel(Gtk::Label *label, const char *str) {
  label->set_text(str ? str : "");
}

static void SetBuffer(Glib::RefPtr<Gtk::TextBuffer> buffer, const char *str) {
  buffer->set_text(str ? str : "");
}

void SetDlg::SelectionChanged() {
  vector< Gtk::TreeModel::Path > paths = m_tree->get_selection()->get_selected_rows();
  if (paths.size() == 0)
    return;
  Gtk::TreeModel::Row row = *m_store->children()[paths.front().back()];
  Glib::ustring name = row[m_name_col];
  
  ps_value_t *v = PS_GetMember(m_ps->Get(m_ext.c_str(), "#set"), name.c_str(), NULL);
  SetLabel(m_name,    PS_GetString(PS_GetMember(v, "label", NULL)));
  SetLabel(m_unit,    PS_GetString(PS_GetMember(v, "unit", NULL)));
  SetLabel(m_type,    PS_GetString(PS_GetMember(v, "type", NULL)));
  
  SetBuffer(m_description_buffer, PS_GetString(PS_GetMember(v, "description", NULL)));
  SetBuffer(m_formula_buffer, PS_GetString(PS_GetMember(v, "value", NULL)));
  
  Pso os(PS_NewStrOStream());
  PS_WriteValue(os(), PS_GetMember(v, "default_value", NULL));
  SetBuffer(m_default_buffer, PS_OStreamContents(os()));
}

void SetDlg::SearchChanged() {
  Glib::ustring search = m_search_buffer->get_text();
  
  m_store->clear();
  Psvi vi(m_ps->Get(m_ext.c_str(), "#set"));
  while (vi.Next()) {
    const char *key = vi.Key();
    if (!strstr(key, search.c_str()))
      continue;
    Gtk::TreeModel::Row row = *m_store->append();
    row[m_name_col] = string(key);
  }
}
