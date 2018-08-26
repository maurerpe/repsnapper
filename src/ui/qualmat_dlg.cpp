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

SelectionBox::SelectionBox(Psv *v,
			   Glib::RefPtr<Gtk::Builder> builder,
			   Glib::ustring prefix,
			   Glib::ustring key) {
  m_v = v;
  m_key = key;
  inhibit_spin_changed = false;
  
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
  if (m_v->IsNull())
    return;
  
  const ps_value_t *v = m_v->Get(m_key.c_str());

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
  
  m_v->Set(m_key.c_str(), name.c_str(), PS_CopyValue(m_template()));
  
  BuildStore();
}

void SelectionBox::Copy(void) {
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  Glib::ustring new_name = GetString();
  ps_value_t *v = PS_CopyValue(m_v->Get(m_key.c_str(), name.c_str()));
  m_v->Set(m_key.c_str(), new_name.c_str(), v);
  
  BuildStore();
}

void SelectionBox::Rename(void) {
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;

  Glib::ustring new_name = GetString();
  ps_value_t *v = PS_CopyValue(m_v->Get(m_key.c_str(), name.c_str()));
  m_v->Set(m_key.c_str(), new_name.c_str(), v);
  PS_RemoveMember(m_v->Get(m_key.c_str()), name.c_str());
  
  BuildStore();
}

void SelectionBox::Delete(void) {
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  PS_RemoveMember(m_v->Get(m_key.c_str()), name.c_str());
  
  BuildStore();
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

  PS_AddMember(m_v->Get(m_key.c_str(), name.c_str()), set_name, num);
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

QualDlg::QualDlg(Psv *vv, Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set) : SelectionBox(vv, builder, "qual", "quality"), m_cust(set, settings->GetPs()) {
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
  
  const ps_value_t *v = m_v->Get(m_key.c_str(), name.c_str());
  
  m_height->set_value(PS_AsFloat(PS_GetMember(v, "height/nozzle", NULL)));
  m_width->set_value(PS_AsFloat(PS_GetMember(v, "width/height", NULL)));
  m_speed->set_value(PS_AsFloat(PS_GetMember(v, "speed", NULL)));
  m_wallspeedratio->set_value(PS_AsFloat(PS_GetMember(v, "wall-speed-ratio", NULL)));

  m_cust.SetValue("#global", PS_GetMember(PS_GetMember(v, "settings", NULL), "#global", NULL));
}

//////////////////////////////// MatDlg ///////////////////////////////////

MatDlg::MatDlg(Psv *vv, Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set) : SelectionBox(vv, builder, "mat", "materials"), m_cust_global(set, settings->GetPs()), m_cust_active(set, settings->GetPs()) {
  m_set = set;

  Psv v(PS_ParseJsonString("{\"nozzle-feedrate\": [[0.4, 25]], \"settings\": {}}"));
  SetTemplate(v());
  
  builder->get_widget("mat_description", m_description);
  builder->get_widget("mat_feedrate", m_feedrate);
  builder->get_widget("mat_width", m_width);
  builder->get_widget("mat_width_enable", m_width_enable);
  
  m_buffer = Gtk::TextBuffer::create();
  m_description->set_buffer(m_buffer);
  
  m_buffer->signal_changed().connect
    (sigc::mem_fun(*this, &MatDlg::DescriptionChanged));
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
  
  const ps_value_t *v = m_v->Get(m_key.c_str(), name.c_str());

  const char *str = PS_GetString(PS_GetMember(v, "description", NULL));
  m_buffer->set_text(str ? str : "");
  m_feedrate->set_value(PS_AsFloat(PS_GetItem(PS_GetItem(PS_GetMember(v, "nozzle-feedrate", NULL), 0), 1)));
  m_width->set_value(PS_AsFloat(PS_GetMember(v, "width/height", NULL)));
  m_width_enable->set_active(PS_GetMember(v, "width/height", NULL) != NULL);

  m_cust_global.SetValue("#global", PS_GetMember(PS_GetMember(v, "settings", NULL), "#global", NULL));
  m_cust_active.SetValue("0", PS_GetMember(PS_GetMember(v, "settings", NULL), "#active", NULL));
}

void MatDlg::DescriptionChanged(void) {
  if (inhibit_spin_changed)
    return;
  
  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  ps_value_t *str = PS_NewString(m_buffer->get_text().c_str());
  
  ps_value_t *v = m_v->Get(m_key.c_str(), name.c_str());
  PS_AddMember(v, "description", str);
}

void MatDlg::FeedrateChanged(void) {
  if (inhibit_spin_changed)
    return;

  Glib::ustring name = GetSelectionName();
  if (name == "")
    return;
  
  ps_value_t *num = PS_NewFloat(m_feedrate->get_value());
  
  ps_value_t *v = PS_GetItem(m_v->Get(m_key.c_str(), name.c_str(), "nozzle-feedrate"), 0);
  PS_ResizeList(v, 0, NULL);
  PS_AppendToList(v, num);  
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

  PS_AddMember(m_v->Get(m_key.c_str(), name.c_str()), "width/height", num);
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
  
  PS_RemoveMember(m_v->Get(m_key.c_str(), name.c_str()), "width/height");
}

////////////////////////////// QualMatDlg /////////////////////////////////

QualMatDlg::QualMatDlg(Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set) : m_qual(&m_qualmat, builder, settings, set), m_mat(&m_qualmat, builder, settings, set) {
  m_settings = settings;
  
  builder->get_widget("qualmat_dlg", m_dlg);

  builder->get_widget("qualmat_ok",     m_ok);
  builder->get_widget("qualmat_close",  m_close);
  builder->get_widget("qualmat_save",   m_save);
  builder->get_widget("qualmat_saveas", m_saveas);
  builder->get_widget("qualmat_load",   m_load);

  m_ok->    signal_clicked().connect(sigc::mem_fun(*this, &QualMatDlg::OK));
  m_close-> signal_clicked().connect(sigc::mem_fun(*this, &QualMatDlg::Close));
  m_save->  signal_clicked().connect(sigc::mem_fun(*this, &QualMatDlg::Save));
  m_saveas->signal_clicked().connect(sigc::mem_fun(*this, &QualMatDlg::SaveAs));
  m_load->  signal_clicked().connect(sigc::mem_fun(*this, &QualMatDlg::Load));
}

void QualMatDlg::show(Gtk::Window &trans) {
  m_qualmat.Take(PS_CopyValue((*m_settings->GetQualMat())()));
  m_qual.BuildStore();
  m_mat.BuildStore();
  
  m_dlg->set_transient_for(trans);
  m_dlg->show();
  m_dlg->raise();
}

void QualMatDlg::OK(void) {
  m_settings->SetQualMat(m_qualmat());
  
  m_dlg->hide();
}

void QualMatDlg::Close(void) {
  m_dlg->hide();
}

void QualMatDlg::Save(void) {
  string filename = m_settings->GetConfigPath("qualmat.json");
  
  Psf outfile(filename.c_str(), "w");
  Pso os(PS_NewFileOStream(outfile()));
  PS_WriteValuePretty(os(), m_qualmat());
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
  PS_WriteValuePretty(os(), m_qualmat());
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
  m_qualmat.Take(PS_ParseJsonFile(file()));
}
