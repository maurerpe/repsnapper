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
#include "cust_prop.h"

class PrinterWidget {
 protected:
  Gtk::Widget *m_widget;
  string m_section;
  string m_ext;
  string m_name;
  ps_value_t **m_printer;
  ps_value_t **m_dflt;
  bool m_inhibit_change;
  
 public:
  PrinterWidget(Gtk::Widget *widget, ps_value_t **dflt,
		ps_value_t **printer, string section, string ext, string name);
  virtual ~PrinterWidget();
  
  virtual void LoadValue(void);
  virtual int IsSet(void);
  virtual void SetActive(bool active);
  virtual void ChangeExtruder(string ext);
  
 protected:
  virtual ps_value_t *GetValue();
  virtual void SetValue(ps_value_t *v);
  virtual void Changed(void);
};

class PrinterSpin : public PrinterWidget {
 public:
  PrinterSpin(Gtk::Widget *widget, ps_value_t **dflt,
	      ps_value_t **printer, string section, string ext, string name);
  
  virtual void LoadValue(void);

 protected:
  void Changed(void);  
};

class PrinterSwitch : public PrinterWidget {
 public:
  PrinterSwitch(Gtk::Widget *widget, ps_value_t **dflt,
		ps_value_t **printer, string section, string ext, string name);
  
  virtual void LoadValue(void);
  
 protected:
  void Changed(void);
};

class PrinterCombo : public PrinterWidget {
 protected:
  const Psv *m_ps;
  
 public:
  PrinterCombo(Gtk::Widget *widget, ps_value_t **dflt,
	       const Psv *ps,
	       ps_value_t **printer, string section, string ext, string name);
  
  virtual void LoadValue(void);

 protected:
  void Changed(void);
  void SetupCombo(void);
};

class PrinterEntry : public PrinterWidget {
 protected:
  Glib::RefPtr< Gtk::EntryBuffer > m_buffer;

 public:
  PrinterEntry(Gtk::Widget *widget, ps_value_t **dflt,
	       ps_value_t **printer, string section, string ext, string name);
  
  virtual void LoadValue(void);

 protected:
  void Changed(void);
};

class PrinterText : public PrinterWidget {
 protected:
  Glib::RefPtr< Gtk::TextBuffer > m_buffer;

 public:
  PrinterText(Gtk::Widget *widget, ps_value_t **dflt,
	      ps_value_t **printer, string section, string ext, string name);
  
  virtual void LoadValue(void);

 protected:
  void Changed(void);
  virtual ps_value_t *Encode(Glib::ustring str);
  virtual Glib::ustring Decode(const ps_value_t *v);
};

class PrinterJson : public PrinterText {
 public:
  PrinterJson(Gtk::Widget *widget, ps_value_t **dflt,
	      ps_value_t **printer, string section, string ext, string name);
  
 protected:
  virtual ps_value_t *Encode(Glib::ustring str);
  virtual Glib::ustring Decode(const ps_value_t *v);
};

class PrinterCheck : public PrinterWidget {
 protected:
  vector< PrinterWidget* > m_sub;
  
 public:
  PrinterCheck(Gtk::Widget *widget, ps_value_t **dflt,
	       ps_value_t **printer, string section, string ext, string name);

  virtual void AddWidget(PrinterWidget *widget);
  virtual void LoadValue(void);
  virtual void SetActive(bool active);
  
 protected:
  void Changed(void);
};

class PrinterDlg {
 protected:
  Gtk::Dialog *m_dlg;
  Settings *m_settings;
  ps_value_t *m_printer;
  ps_value_t *m_dflt;
  vector< PrinterWidget* > m_widgets;
  vector< PrinterWidget* > m_ext_widgets;
  string m_folder;
  string m_import_folder;
  CustProp m_cust_global;
  CustProp m_cust_extruder;
  int m_inhibit_extruder;
  
  Glib::RefPtr< Gtk::ListStore > m_store;
  Gtk::TreeModelColumnRecord m_cols;
  Gtk::TreeModelColumn<Glib::ustring> m_name_col;
  
  Gtk::Button *m_new;
  Gtk::Button *m_remove;
  Gtk::TreeView *m_tree;
 public:
  PrinterDlg(Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set);
  ~PrinterDlg();
  
  void Show(Gtk::Window &trans);
  
 protected:
  void Add(Glib::RefPtr<Gtk::Builder> builder, string id, string section, string ext, string name);
  void AddJson(Glib::RefPtr<Gtk::Builder> builder, string id, string section, string ext, string name);
  void AddCheck(Glib::RefPtr<Gtk::Builder> builder, string id, string section, string ext, string name, size_t num);
  void AddExt(Glib::RefPtr<Gtk::Builder> builder, string id, string section, string ext, string name);
  
  void LoadAll(void);

  void BuildExtruderList(void);
  void New(void);
  void Remove(void);
  void ExtruderSelected(void);
  
  void OK();
  void Cancel();
  void Save();
  void SaveAs();
  void Load();
  void Import();
};
