/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010  Kulitorum
    Copyright (C) 2011  Michael Meeks <michael.meeks@novell.com>
    Copyright (C) 2012  martin.dieringer@gmx.de
    Copyright (C) 2018  Paul Maurer

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <glib/gi18n.h>

#include <sstream>

#include <printer_settings.h>

#include "model.h"
#include "ps_helper.h"
#include "shape.h"
#include "ui/progress.h"

void Model::ConvertToGCode() {
  Prog prog(m_progress, _("Slicing Model"), 100.0);
  prog.update(0);
  
  const Psv *ps = settings.GetPs();
  
  Psv set(settings.FullSettings());
  Vector3d bed = settings.getPrintVolume();
  Vector3d margin = settings.getPrintMargin();
  
  int num = settings.getNumExtruders();
  Pssa strs(num);
  for (int count = 0; count < num; count++) {
    Shape comb = GetCombinedShape(count);
    if (comb.size() == 0)
      continue;
    
    comb.move(Vector3d(-bed.x() / 2.0 + margin.x(),
		       -bed.y() / 2.0 + margin.y(),
		       0));
    strs.append(comb.getSTLsolid(), settings.FullSettings(count));
  }
  
  Pstemp temp(".def.json");
  settings.WriteTempPrinter(temp(), set.GetNames());
  temp.Close();
  Psv ps2(PS_CopyValue((*ps)()));
  ps2.Set("#global", "#filename", temp.Name());
  
  Pso gcode_stream(PS_NewStrOStream());
  if (PS_SliceStrs(gcode_stream(), ps2(), set(), strs(), strs.size()) < 0) {
    Gtk::MessageDialog dialog(_("Error Slicing Model"), false,
                              Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
    dialog.run();
    return;
  }
  
  temp.Delete();
  
  istringstream iss(string(PS_OStreamContents(gcode_stream())));
  gcode.Parse(this, m_progress, iss);
  cout << "Slicing complete" << endl;
  
  prog.update(100);
}
