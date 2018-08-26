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
#include "set_dlg.h"
#include "cust_prop.h"
#include "ps_helper.h"

class SelectionBox {
 protected:
  Psv *m_v;
  Glib::ustring m_key;
  Psv m_template;
  
  Gtk::Dialog *m_dlg;
  
  Glib::RefPtr< Gtk::ListStore > m_store;
  Gtk::TreeModelColumnRecord m_cols;
  Gtk::TreeModelColumn<Glib::ustring> m_name_col;
  
  Gtk::TreeView *m_tree;
  Gtk::Button   *m_new;
  Gtk::Button   *m_copy;
  Gtk::Button   *m_rename;
  Gtk::Button   *m_delete;
  
  bool inhibit_spin_changed;
  
 public:
  SelectionBox(Psv *v,
	       Glib::RefPtr<Gtk::Builder> builder,
	       Glib::ustring prefix,
	       Glib::ustring key);
  void BuildStore(void);
  
 protected:
  void SetTemplate(const ps_value_t *v);
  Glib::ustring GetSelectionName(void);
  void SelectFirst(void);
  
  void New(void);
  void Copy(void);
  void Rename(void);
  void Delete(void);

  void SpinChanged(Gtk::SpinButton *button, const char *name);
  
  Glib::ustring GetString(void);
};

class QualDlg : public SelectionBox {
 private:
  SetDlg *m_set;
  CustProp m_cust;
  
  Gtk::SpinButton *m_height;
  Gtk::SpinButton *m_width;
  Gtk::SpinButton *m_speed;
  Gtk::SpinButton *m_wallspeedratio;
  
 public:
  QualDlg(Psv *v, Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set);

 private:
  void SelectionChanged(void);  
};

class MatDlg : public SelectionBox {
 private:
  SetDlg *m_set;
  CustProp m_cust_global;
  CustProp m_cust_active;
  
  Gtk::SpinButton  *m_feedrate;
  Gtk::SpinButton  *m_width;
  Gtk::CheckButton *m_width_enable;
  
 public:
  MatDlg(Psv *v, Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set);
  
 private:
  void SelectionChanged(void);
  void FeedrateChanged(void);
  void WidthSpinChanged(void);
  void WidthEnableChanged(void);
};

class QualMatDlg {
 private:
  Settings *m_settings;
  Psv m_qualmat;
  QualDlg m_qual;
  MatDlg m_mat;
  Gtk::Dialog *m_dlg;

  Gtk::Button *m_ok;
  Gtk::Button *m_close;
  Gtk::Button *m_save;
  Gtk::Button *m_saveas;
  Gtk::Button *m_load;
  
  string m_folder;
  
 public:
  QualMatDlg(Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set);
  void show(Gtk::Window &trans);

 private:
  void OK(void);
  void Close(void);
  void Save(void);
  void SaveAs(void);
  void Load(void);
};
