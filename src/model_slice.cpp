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

extern "C" {
#include <printer_settings.h>
}

#include "model.h"

class Psv {
protected:
  ps_value_t *v;
public:
  Psv(ps_value_t *val);
  ~Psv();

  ps_value_t *operator()(void) {return v;};
};

Psv::Psv(ps_value_t *val) : v(val) {
  if (val == NULL)
    throw invalid_argument(string("ps_value was null"));
}

Psv::~Psv() {
  PS_FreeValue(v);
}

class Pso {
protected:
  ps_ostream_t *os;
public:
  Pso(ps_ostream_t *ostream);
  ~Pso();

  ps_ostream_t *operator()(void) {return os;};
};

Pso::Pso(ps_ostream_t *ostream) : os(ostream) {
  if (os == NULL)
    throw invalid_argument(string("ps_ostream was null"));
}

Pso::~Pso() {
  PS_FreeOStream(os);
}

class Psf {
protected:
  FILE *file;
public:
  Psf(const char *name);
  ~Psf();

  void close(void);
  FILE *operator()(void) {return file;};
};

Psf::Psf(const char *name) {
  if ((file = fopen(name, "r")) == NULL)
    throw invalid_argument(string("Could not open file \"") + string(name) + string("\""));
}

Psf::~Psf() {  
  close();
}

void Psf::close(void) {
  if (file)
    fclose(file);
  file = NULL;
}

void Model::ConvertToGCode() {
  Psv search(PS_NewList());
  PS_AppendToList(search(), PS_NewString("/usr/share/cura/resources/definitions"));
  PS_AppendToList(search(), PS_NewString("/usr/share/cura/resources/extruders"));  
  Psv ps(PS_New("/home/maurerpe/.config/repsnapper/cr10mini.def.json", search()));

  Psf config_file("/home/maurerpe/.config/repsnapper/cura_settings.json");
  Psv config(PS_ParseJsonFile(config_file()));
  config_file.close();

  const ps_value_t *nn = PS_GetMember(config(), "nozzles", NULL);
  const ps_value_t *xx = PS_GetItem(PS_GetItem(nn, 0), 1);
  const ps_value_t *qual = PS_GetMember(xx, "normal", NULL);
  
  double dia = 1.75;
  double noz = 0.4;
  double h = PS_AsFloat(PS_GetMember(qual, "layer-height", NULL));
  double wh = PS_AsFloat(PS_GetMember(qual, "width/height", NULL));
  double speed = PS_AsFloat(PS_GetMember(qual, "speed", NULL));
  double ratio = PS_AsFloat(PS_GetMember(qual, "wall-speed-ratio", NULL));
  double infill = 50.0;
  int shells = 3;
  int skins = 3;
  
  const ps_value_t *mat = PS_GetMember(PS_GetMember(config(), "materials", NULL), "pla", NULL);
  double efeed = PS_AsFloat(PS_GetItem(PS_GetItem(PS_GetMember(mat, "nozzle-feedrate", NULL), 0), 1));
  if (PS_GetMember(mat, "width/height", NULL))
    wh = PS_AsFloat(PS_GetMember(mat, "width/height", NULL));
  
  double espeed = efeed * dia * dia / (h * h * wh);
  if (espeed < speed)
    speed = espeed;
  
  Psv set(PS_BlankSettings(ps()));
  PS_AddSetting(set(), "#global", "material_diameter", PS_NewFloat(dia));
  PS_AddSetting(set(), "#global", "machine_nozzle_size", PS_NewFloat(noz));
  
  PS_AddSetting(set(), "0", "material_diameter", PS_NewFloat(dia));
  PS_AddSetting(set(), "0", "machine_nozzle_size", PS_NewFloat(noz));
  
  PS_AddSetting(set(), "#global", "layer_height", PS_NewFloat(h));
  PS_AddSetting(set(), "#global", "line_width", PS_NewFloat(h * wh));
  PS_AddSetting(set(), "#global", "speed_print", PS_NewFloat(speed));
  PS_AddSetting(set(), "#global", "speed_wall", PS_NewFloat(speed * ratio));
  PS_AddSetting(set(), "#global", "wall_line_count", PS_NewInteger(shells));
  PS_AddSetting(set(), "#global", "top_layers", PS_NewInteger(skins));
  PS_AddSetting(set(), "#global", "bottom_layers", PS_NewInteger(skins));
  PS_AddSetting(set(), "#global", "infill_sparse_density", PS_NewFloat(infill));
  
  PS_MergeSettings(set(), PS_GetMember(qual, "settings", NULL));
  PS_MergeSettings(set(), PS_GetMember(mat,  "settings", NULL));
  
  Pso gcode_stream(PS_NewStrOStream());
  string stl = GetCombinedShape().getSTLsolid();  
  PS_SliceStr(gcode_stream(), ps(), set(), stl.c_str());
  
  istringstream iss(string(PS_OStreamContents(gcode_stream())));
  gcode.Parse(this, settings.get_extruder_letters(), m_progress, iss);
}

void Model::SliceToSVG(Glib::RefPtr<Gio::File>, bool) {
}
