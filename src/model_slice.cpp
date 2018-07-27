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

#include <sstream>

#include <printer_settings.h>

#include "model.h"
#include "ps_helper.h"
#include "shape.h"
#include "ui/progress.h"

extern string materials[];

void Model::ConvertToGCode() {
  Prog prog(m_progress, _("Slicing Model"), 100.0);
  prog.update(0);
  
  const Psv *ps = settings.GetPs();
  const Psv *dflt = settings.GetDflt();
  const Psv *config = settings.GetConfig();
  
  const ps_value_t *nn = PS_GetMember((*config)(), "nozzles", NULL);
  const ps_value_t *xx = PS_GetItem(PS_GetItem(nn, 0), 1);
  const ps_value_t *qual = PS_GetMember(xx, "normal", NULL);
  
  double dia = PS_AsFloat(dflt->Get("#global", "material_diameter"));
  double bedw = PS_AsFloat(dflt->Get("#global", "machine_width"));
  double bedd = PS_AsFloat(dflt->Get("#global", "machine_depth"));
  
  double noz = 0.4;
  //double h = PS_AsFloat(PS_GetMember(qual, "layer-height", NULL));
  double wh = PS_AsFloat(PS_GetMember(qual, "width/height", NULL));
  double speed = PS_AsFloat(PS_GetMember(qual, "speed", NULL));
  double ratio = PS_AsFloat(PS_GetMember(qual, "wall-speed-ratio", NULL));

  double h = settings.get_double("Slicing", "LayerHeight");
  bool support = settings.get_boolean("Slicing", "Support");
  double infill = settings.get_double("Slicing", "InfillPercent");
  int shells = settings.get_integer("Slicing", "ShellCount");
  int skins = settings.get_integer("Slicing", "Skins");
  double marginx = settings.get_double("Hardware", "PrintMargin.X");
  double marginy = settings.get_double("Hardware", "PrintMargin.Y");
  string matname = materials[settings.get_integer("Slicing", "Material")];
  bool spiralize = settings.get_boolean("Slicing","Spiralize");
  
  const ps_value_t *mat = config->Get("materials", matname.c_str());
  double efeed = PS_AsFloat(PS_GetItem(PS_GetItem(PS_GetMember(mat, "nozzle-feedrate", NULL), 0), 1));
  if (PS_GetMember(mat, "width/height", NULL))
    wh = PS_AsFloat(PS_GetMember(mat, "width/height", NULL));
  
  double espeed = efeed * dia * dia / (h * h * wh);
  if (espeed < speed)
    speed = espeed;
  
  Psv set(PS_BlankSettings((*ps)()));
  set.Set("#global", "material_diameter", dia);
  set.Set("#global", "machine_nozzle_size", noz);
  
  set.Set("0", "material_diameter", dia);
  set.Set("0", "machine_nozzle_size", noz);
  
  set.Set("#global", "layer_height", h);
  set.Set("#global", "line_width", h * wh);
  set.Set("#global", "speed_print", speed);
  set.Set("#global", "speed_wall", speed * ratio);
  set.Set("#global", "wall_line_count", spiralize ? 1 : shells);
  set.Set("#global", "top_layers", skins);
  set.Set("#global", "bottom_layers", skins);
  set.Set("#global", "infill_sparse_density", infill);
  set.Set("#global", "support_enable", support);
  set.Set("#global", "magic_spiralize", spiralize);
  
  PS_MergeSettings(set(), PS_GetMember(qual, "settings", NULL));
  PS_MergeSettings(set(), PS_GetMember(mat,  "settings", NULL));
  
  Pso gcode_stream(PS_NewStrOStream());
  Shape comb = GetCombinedShape();
  comb.move(Vector3d(-bedw / 2.0 + marginx, -bedd / 2.0 + marginy, 0));
  string stl = comb.getSTLsolid();  
  if (PS_SliceStr(gcode_stream(), (*ps)(), set(), stl.c_str(), stl.length()) < 0) {
    Gtk::MessageDialog dialog(_("Error Slicing Model"), false,
                              Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
    dialog.run();
    return;
  }
  
  istringstream iss(string(PS_OStreamContents(gcode_stream())));
  gcode.Parse(this, settings.get_extruder_letters(), m_progress, iss);
  cout << "Slicing complete" << endl;
  
  prog.update(100);
}
