/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2012 martin.dieringer@gmx.de
    Copyright (C) 2018 Paul Maurer

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this program; if not, see <http://www.gnu.org/licenses/>.

*/

#include "rsfilechooser.h"

#include "view.h"
#include "model.h"

RSFilechooser::RSFilechooser(View * view_)
  : view(view_) {
  builder = view->getBuilder();
  builder->get_widget("filechooser", chooser);
  if (!chooser) {
    cerr << "no 'filechooser' in GUI!" << endl;
    return;
  }
  
  chooser->signal_update_preview().connect_notify
    ( sigc::bind(sigc::mem_fun
		 (*this, &RSFilechooser::on_filechooser_preview), chooser) );
  
  Filters::attach_filters(*chooser, Filters::MODEL);
  
  view->connect_button("load_button",
		       sigc::mem_fun(*this, &RSFilechooser::do_action));

  chooser->signal_file_activated().connect
    (sigc::mem_fun(*this, &RSFilechooser::do_action));
}

RSFilechooser::~RSFilechooser() {
  chooser = NULL;
}

void RSFilechooser::do_action() {
  view->do_load_stl();
}

bool RSFilechooser::on_filechooser_key(GdkEventKey* event) {
  if (event->state & GDK_SHIFT_MASK) {
    switch (event->keyval) {
    case GDK_KEY_Return: do_action();
    }
    return true;
  }
  return false;
}

void RSFilechooser::on_filechooser_preview(Gtk::FileChooserWidget *chooser) {
  if (!chooser) return;
  Glib::RefPtr< Gio::File > pfile = chooser->get_preview_file();
  if (!pfile) return;
  Gio::FileType ftype = pfile->query_file_type();
  if (ftype != Gio::FILE_TYPE_NOT_KNOWN)
    view->preview_file(pfile);
}
