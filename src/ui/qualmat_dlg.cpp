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

#include "qualmat_dlg.h"
#include "filters.h"

//////////////////////// SelectionBox ///////////////////////////////////

SelectionBox::SelectionBox(Glib::RefPtr<Gtk::Builder> builder,
			   Settings *settings,
			   Glib::ustring prefix,
			   Glib::ustring key) {
  m_settings = settings;
  m_key = key;
  
  builder->get_widget("qualmat_dlg", m_dlg);
  builder->get_widget(prefix + "_tree", m_tree);
  builder->get_widget(prefix + "_new", m_new);
  builder->get_widget(prefix + "_copy", m_copy);
  builder->get_widget(prefix + "_rename", m_rename);
  builder->get_widget(prefix + "_delete", m_delete);

  m_cols.add(m_name_col);
  m_store = Gtk::ListStore::create(m_cols);
  m_tree->set_model(m_store);
  m_tree->set_headers_visible(false);
  m_tree->append_column(_("Name"), m_name_col);
  
  m_new->signal_clicked().connect(sigc::mem_fun(*this, &QualDlg::New));
  m_copy->signal_clicked().connect(sigc::mem_fun(*this, &QualDlg::Copy));
  m_rename->signal_clicked().connect(sigc::mem_fun(*this, &QualDlg::Rename));
  m_delete->signal_clicked().connect(sigc::mem_fun(*this, &QualDlg::Delete));
}

void SelectionBox::SetTemplate(const ps_value_t *v) {
  m_template.Take(PS_CopyValue(v));
}

Glib::ustring SelectionBox::GetSelectionName(void) {
  vector< Gtk::TreeModel::Path > paths = m_tree->get_selection()->get_selected_rows();
  if (paths.size() == 0)
    return "";
  Gtk::TreeModel::Row row = *m_store->children()[paths.front().back()];
  return row[m_name_col];
}

void SelectionBox::SelectFirst(void) {
  Gtk::TreePath path;
  
  path.push_back(0);
  
  m_tree->get_selection()->select(path);
}

void SelectionBox::BuildStore(void) {
  const ps_value_t *v = PS_GetMember((*m_settings->GetQualMat())(), m_key.c_str(), NULL);

  bool at_least_one = false;
  
  m_store->clear();

  Psvi vi(v);
  while (vi.Next()) {
    at_least_one = true;
    Gtk::TreeModel::Row row = *m_store->append();
    row[m_name_col] = Glib::ustring(vi.Key());
  }

  if (at_least_one)
    SelectFirst();
}

void SelectionBox::New(void) {
  Glib::ustring name = GetString();
  if (name == "")
    return;
  
  m_settings->GetQualMat()->Set(m_key.c_str(), name.c_str(), PS_CopyValue(m_template()));
  
  BuildStore();
  m_sig_changed.emit();
}

void SelectionBox::Copy(void) {
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  Glib::ustring new_name = GetString();
  ps_value_t *v = PS_CopyValue(m_settings->GetQualMat()->Get(m_key.c_str(), name.c_str()));
  m_settings->GetQualMat()->Set(m_key.c_str(), new_name.c_str(), v);
  
  BuildStore();
  m_sig_changed.emit();
}

void SelectionBox::Rename(void) {
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;

  Glib::ustring new_name = GetString();
  ps_value_t *v = PS_CopyValue(m_settings->GetQualMat()->Get(m_key.c_str(), name.c_str()));
  m_settings->GetQualMat()->Set(m_key.c_str(), new_name.c_str(), v);
  PS_RemoveMember(PS_GetMember((*m_settings->GetQualMat())(), m_key.c_str(), NULL), name.c_str());
  
  BuildStore();
  m_sig_changed.emit();
}

void SelectionBox::Delete(void) {
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  PS_RemoveMember(PS_GetMember((*m_settings->GetQualMat())(), m_key.c_str(), NULL), name.c_str());
  
  BuildStore();
  m_sig_changed.emit();
}

void SelectionBox::SpinChanged(Gtk::SpinButton *button, const char *set_name) {
  if (inhibit_spin_changed)
    return;
  
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  ps_value_t *num = PS_NewFloat(button->get_value());
  if (num == NULL)
    return;

  ps_value_t *v;
  try {
    v = (ps_value_t *) m_settings->GetQualMat()->Get(m_key.c_str(), name.c_str());
  } catch (exception &e) {
    PS_FreeValue(num);
    return;
  }
  PS_AddMember(v, set_name, num);
  m_sig_changed.emit();
}

Glib::ustring SelectionBox::GetString(void) {
  Gtk::Dialog dlg("Name for new setting", *m_dlg, true);
  dlg.add_button(_("Cancel"), 0);
  dlg.add_button(_("Ok"), 1);
  dlg.set_default_response(1);

  Glib::RefPtr<Gtk::EntryBuffer> buffer = Gtk::EntryBuffer::create();
  Gtk::Entry entry(buffer);
  dlg.get_content_area()->add(entry);
  dlg.show_all();
  int ret = dlg.run();
  dlg.hide();

  if (ret != 1)
    return "";
  
  return buffer->get_text();
}

//////////////////////////////// QualDlg ///////////////////////////////////

QualDlg::QualDlg(Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set) : SelectionBox(builder, settings, "qual", "quality"), m_cust(set, settings->GetPs()) {
  m_set = set;

  Psv v(PS_ParseJsonString("{\"height/nozzle\": 0.5, \"width/height\": 1.8, \"speed\": 60, \"wall-speed-ratio\": 0.5, \"settings\": {}}"));
  SetTemplate(v());
  
  builder->get_widget("qual_height", m_height);
  builder->get_widget("qual_width", m_width);
  builder->get_widget("qual_speed", m_speed);
  builder->get_widget("qual_wallspeedratio", m_wallspeedratio);
  
  m_height->signal_value_changed().connect
    (sigc::bind(sigc::bind(sigc::mem_fun(*this, &QualDlg::SpinChanged), "height/nozzle"), m_height));
  m_width->signal_value_changed().connect
    (sigc::bind(sigc::bind(sigc::mem_fun(*this, &QualDlg::SpinChanged), "width/height"), m_width));
  m_speed->signal_value_changed().connect
    (sigc::bind(sigc::bind(sigc::mem_fun(*this, &QualDlg::SpinChanged), "speed"), m_speed));
  m_wallspeedratio->signal_value_changed().connect
    (sigc::bind(sigc::bind(sigc::mem_fun(*this, &QualDlg::SpinChanged), "wall-speed-ratio"), m_wallspeedratio));
  
  m_tree->get_selection()->signal_changed().connect
    (sigc::mem_fun(*this, &QualDlg::SelectionChanged));
  
  m_cust.SetWindow(m_dlg);
  
  Gtk::Box *box;
  builder->get_widget("qual_cust", box);
  box->add(m_cust);
  
  BuildStore();
}

void QualDlg::SelectionChanged(void) {
  Inhibitor inhibit(&inhibit_spin_changed);
  
  Glib::ustring name = GetSelectionName();
  
  const ps_value_t *v = PS_GetMember(PS_GetMember((*m_settings->GetQualMat())(), m_key.c_str(), NULL), name.c_str(), NULL);
  
  m_height->set_value(PS_AsFloat(PS_GetMember(v, "height/nozzle", NULL)));
  m_width->set_value(PS_AsFloat(PS_GetMember(v, "width/height", NULL)));
  m_speed->set_value(PS_AsFloat(PS_GetMember(v, "speed", NULL)));
  m_wallspeedratio->set_value(PS_AsFloat(PS_GetMember(v, "wall-speed-ratio", NULL)));

  m_cust.SetValue("#global", PS_GetMember(PS_GetMember(v, "settings", NULL), "#global", NULL));
}

//////////////////////////////// MatDlg ///////////////////////////////////

MatDlg::MatDlg(Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set) : SelectionBox(builder, settings, "mat", "materials"), m_cust_global(set, settings->GetPs()), m_cust_active(set, settings->GetPs()) {
  m_set = set;

  Psv v(PS_ParseJsonString("{\"nozzle-feedrate\": [[0.4, 25]], \"settings\": {}}"));
  SetTemplate(v());
  
  builder->get_widget("mat_feedrate", m_feedrate);
  builder->get_widget("mat_width", m_width);
  builder->get_widget("mat_width_enable", m_width_enable);
  
  m_feedrate->signal_value_changed().connect
    (sigc::mem_fun(*this, &MatDlg::FeedrateChanged));
  m_width->signal_value_changed().connect
    (sigc::mem_fun(*this, &MatDlg::WidthSpinChanged));
  m_width_enable->signal_toggled().connect
    (sigc::mem_fun(*this, &MatDlg::WidthSpinChanged));
  
  m_tree->get_selection()->signal_changed().connect
    (sigc::mem_fun(*this, &MatDlg::SelectionChanged));
  
  m_cust_global.SetWindow(m_dlg);
  m_cust_active.SetWindow(m_dlg);
  
  Gtk::Box *box;
  builder->get_widget("mat_cust_global", box);
  box->add(m_cust_global);
  builder->get_widget("mat_cust_active", box);
  box->add(m_cust_active);
  
  BuildStore();
}

void MatDlg::SelectionChanged(void) {
  Inhibitor inhibit(&inhibit_spin_changed);
  
  Glib::ustring name = GetSelectionName();
  
  const ps_value_t *v = PS_GetMember(PS_GetMember((*m_settings->GetQualMat())(), m_key.c_str(), NULL), name.c_str(), NULL);
  
  m_feedrate->set_value(PS_AsFloat(PS_GetItem(PS_GetItem(PS_GetMember(v, "nozzle-feedrate", NULL), 0), 1)));
  m_width->set_value(PS_AsFloat(PS_GetMember(v, "width/height", NULL)));
  m_width_enable->set_active(PS_GetMember(v, "width/height", NULL) != NULL);

  m_cust_global.SetValue("#global", PS_GetMember(PS_GetMember(v, "settings", NULL), "#global", NULL));
  m_cust_active.SetValue("0", PS_GetMember(PS_GetMember(v, "settings", NULL), "#active", NULL));
}

void MatDlg::FeedrateChanged(void) {
  if (inhibit_spin_changed)
    return;

  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  ps_value_t *num = PS_NewFloat(m_feedrate->get_value());
  
  ps_value_t *v;
  try {
    v = PS_GetItem(PS_GetMember(m_settings->GetQualMat()->Get(m_key.c_str(), name.c_str()), "nozzle-feedrate", NULL), 0);
  } catch (exception &e) {
    PS_FreeValue(num);
    return;
  }
  PS_ResizeList(v, 0, NULL);
  PS_AppendToList(v, num);  
  m_sig_changed.emit();
}

void MatDlg::WidthSpinChanged(void) {
  if (inhibit_spin_changed)
    return;

  if (!m_width_enable->get_active())
    return;
  
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  ps_value_t *num = PS_NewFloat(m_width->get_value());
  if (num == NULL)
    return;

  ps_value_t *v;
  try {
    v = (ps_value_t *) m_settings->GetQualMat()->Get(m_key.c_str(), name.c_str());
  } catch (exception &e) {
    PS_FreeValue(num);
    return;
  }
  PS_AddMember(v, "width/height", num);
  m_sig_changed.emit();
}

void MatDlg::WidthEnableChanged(void) {
  if (inhibit_spin_changed)
    return;
  
  if (m_width_enable->get_active()) {
    WidthSpinChanged();
    return;
  }
  
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  ps_value_t *num = PS_NewFloat(m_width->get_value());
  if (num == NULL)
    return;

  ps_value_t *v;
  try {
    v = (ps_value_t *) m_settings->GetQualMat()->Get(m_key.c_str(), name.c_str());
  } catch (exception &e) {
    PS_FreeValue(num);
    return;
  }
  PS_RemoveMember(v, "width/height");
  m_sig_changed.emit();
}

////////////////////////////// QualMatDlg /////////////////////////////////

QualMatDlg::QualMatDlg(Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set) : qual(builder, settings, set), mat(builder, settings, set) {
  m_settings = settings;
  
  builder->get_widget("qualmat_dlg", m_dlg);

  builder->get_widget("qualmat_close",  m_close);
  builder->get_widget("qualmat_save",   m_save);
  builder->get_widget("qualmat_saveas", m_saveas);
  builder->get_widget("qualmat_load",   m_load);

  m_close-> signal_clicked().connect(sigc::mem_fun(*this, &QualMatDlg::Close));
  m_save->  signal_clicked().connect(sigc::mem_fun(*this, &QualMatDlg::Save));
  m_saveas->signal_clicked().connect(sigc::mem_fun(*this, &QualMatDlg::SaveAs));
  m_load->  signal_clicked().connect(sigc::mem_fun(*this, &QualMatDlg::Load));
  
  qual.signal_changed().connect(sigc::mem_fun(*this, &QualMatDlg::Changed));
  mat. signal_changed().connect(sigc::mem_fun(*this, &QualMatDlg::Changed));
}

void QualMatDlg::show(Gtk::Window &trans) {
  m_dlg->set_transient_for(trans);
  m_dlg->show();
  m_dlg->raise();
}

void QualMatDlg::Close(void) {
  m_dlg->hide();
}

void QualMatDlg::Save(void) {
  string filename = m_settings->GetConfigPath("qualmat.json");
  
  Psf outfile(filename.c_str(), "w");
  Pso os(PS_NewFileOStream(outfile()));
  PS_WriteValuePretty(os(), (*m_settings->GetQualMat())());
}

void QualMatDlg::SaveAs(void) {
  Gtk::FileChooserDialog dlg(*m_dlg, _("Save Qualmat"), Gtk::FILE_CHOOSER_ACTION_SAVE);
  dlg.add_button(_("Cancel"), 0);
  dlg.add_button(_("Save"), 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_folder);
  dlg.set_do_overwrite_confirmation(true);
  
  Filters::attach_filters(dlg, Filters::JSON);
  
  if (dlg.run() != 1)
    return;
  
  string filename = dlg.get_filename();  
  m_folder = dlg.get_current_folder();
  
  Psf outfile(filename.c_str(), "w");
  Pso os(PS_NewFileOStream(outfile()));
  PS_WriteValuePretty(os(), (*m_settings->GetQualMat())());
}

void QualMatDlg::Load(void) {
  Gtk::FileChooserDialog dlg(*m_dlg, _("Load Qualmat"), Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button(_("Cancel"), 0);
  dlg.add_button(_("Load"), 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_folder);
  
  Filters::attach_filters(dlg, Filters::JSON);

  if (dlg.run() != 1)
    return;
  
  string filename = dlg.get_filename();  
  m_folder = dlg.get_current_folder();
  
  Psf file(filename.c_str(), "r");
  m_settings->GetQualMat()->Take(PS_ParseJsonFile(file()));
  
  Changed();
}

void QualMatDlg::Changed(void) {
  m_sig_changed.emit();
}
