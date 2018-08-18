/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2011 Michael Meeks

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

#include "filters.h"

void Filters::attach_filters(Gtk::FileChooser &chooser, Filters::FileType type) {
  Glib::RefPtr< Gtk::FileFilter > allfiles = Gtk::FileFilter::create();
  allfiles->set_name(_("All Files"));
  allfiles->add_pattern("*");
  
  Glib::RefPtr< Gtk::FileFilter > spec = Gtk::FileFilter::create();
  switch (type) {
  case MODEL:
    spec->set_name(_("Models"));
    spec->add_pattern("*.stl");
    spec->add_pattern("*.STL");
    spec->add_pattern("*.wrl");
    spec->add_pattern("*.WRL");
    break;
    
  case GCODE:
    spec->set_name(_("GCode"));
    spec->add_pattern("*.g");
    spec->add_pattern("*.G");
    spec->add_pattern("*.gcode");
    spec->add_pattern("*.GCODE");
    break;
    
  case SETTINGS:
    spec->set_name(_("Settings"));
    spec->add_pattern("*.conf");
    break;

  case JSON:
    spec->set_name(_("Json Settings"));
    spec->add_pattern("*.json");
    break;

  default:
    break;
  }  
  
  chooser.add_filter(allfiles);
  chooser.add_filter(spec);
  chooser.set_filter(spec);
}
