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
#include "shape.h"

extern string materials[];

class Psv {
protected:
  ps_value_t *v;
  void Set(const char *ext, const char *setting, ps_value_t *val);
public:
  Psv(ps_value_t *val);
  ~Psv();
  
  ps_value_t *operator()(void) {return v;};
  const ps_value_t *Get(const char *ext, const char *set);
  void Set(const char *ext, const char *setting, int val);
  void Set(const char *ext, const char *setting, double val);
  void Set(const char *ext, const char *setting, const char *val);
};
  
Psv::Psv(ps_value_t *val) : v(val) {
  if (val == NULL)
    throw invalid_argument(string("ps_value was null"));
}
  
  Psv::~Psv() {
  PS_FreeValue(v);
}

const ps_value_t *Psv::Get(const char *ext, const char *setting) {
  const ps_value_t *gg = PS_GetMember(PS_GetMember(v, ext, NULL), setting, NULL);
  if (gg == NULL)
    throw invalid_argument(string("Unknown setting ") + string(ext) + "->" + string(setting));
  
  return gg;
}

void Psv::Set(const char *ext, const char *setting, ps_value_t *val) {
  if (PS_AddSetting(v, ext, setting, val) < 0) {
    PS_FreeValue(val);
    throw invalid_argument(string("Could not set ") + string(ext) + "->" + string(setting));
  }
}

void Psv::Set(const char *ext, const char *setting, int val) {
  Set(ext, setting, PS_NewInteger(val));
}

void Psv::Set(const char *ext, const char *setting, double val) {
  Set(ext, setting, PS_NewFloat(val));
}

void Psv::Set(const char *ext, const char *setting, const char *val) {
  Set(ext, setting, PS_NewString(val));
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
  Psv dflt(PS_GetDefaults(ps()));
  
  Psf config_file("/home/maurerpe/.config/repsnapper/cura_settings.json");
  Psv config(PS_ParseJsonFile(config_file()));
  config_file.close();

  const ps_value_t *nn = PS_GetMember(config(), "nozzles", NULL);
  const ps_value_t *xx = PS_GetItem(PS_GetItem(nn, 0), 1);
  const ps_value_t *qual = PS_GetMember(xx, "normal", NULL);
  
  double dia = PS_AsFloat(dflt.Get("#global", "material_diameter"));
  double bedw = PS_AsFloat(dflt.Get("#global", "machine_width"));
  double bedd = PS_AsFloat(dflt.Get("#global", "machine_depth"));
  
  double noz = 0.4;
  double h = PS_AsFloat(PS_GetMember(qual, "layer-height", NULL));
  double wh = PS_AsFloat(PS_GetMember(qual, "width/height", NULL));
  double speed = PS_AsFloat(PS_GetMember(qual, "speed", NULL));
  double ratio = PS_AsFloat(PS_GetMember(qual, "wall-speed-ratio", NULL));
  
  double infill = settings.get_double("Slicing","InfillPercent");
  int shells = settings.get_integer("Slicing","ShellCount");
  int skins = settings.get_integer("Slicing","Skins");
  double marginx = settings.get_double("Hardware", "PrintMargin.X");
  double marginy = settings.get_double("Hardware", "PrintMargin.Y");
  string matname = materials[settings.get_integer("Slicing", "Material")];
  
  const ps_value_t *mat = config.Get("materials", matname.c_str());
  double efeed = PS_AsFloat(PS_GetItem(PS_GetItem(PS_GetMember(mat, "nozzle-feedrate", NULL), 0), 1));
  if (PS_GetMember(mat, "width/height", NULL))
    wh = PS_AsFloat(PS_GetMember(mat, "width/height", NULL));
  
  double espeed = efeed * dia * dia / (h * h * wh);
  if (espeed < speed)
    speed = espeed;
  
  Psv set(PS_BlankSettings(ps()));
  set.Set("#global", "material_diameter", dia);
  set.Set("#global", "machine_nozzle_size", noz);
  
  set.Set("0", "material_diameter", dia);
  set.Set("0", "machine_nozzle_size", noz);
  
  set.Set("#global", "layer_height", h);
  set.Set("#global", "line_width", h * wh);
  set.Set("#global", "speed_print", speed);
  set.Set("#global", "speed_wall", speed * ratio);
  set.Set("#global", "wall_line_count", shells);
  set.Set("#global", "top_layers", skins);
  set.Set("#global", "bottom_layers", skins);
  set.Set("#global", "infill_sparse_density", infill);
  
  PS_MergeSettings(set(), PS_GetMember(qual, "settings", NULL));
  PS_MergeSettings(set(), PS_GetMember(mat,  "settings", NULL));
  
  Pso gcode_stream(PS_NewStrOStream());
  Shape comb = GetCombinedShape();
  comb.move(Vector3d(-bedw / 2.0 + marginx, -bedd / 2.0 + marginy, 0));
  string stl = comb.getSTLsolid();  
  PS_SliceStr(gcode_stream(), ps(), set(), stl.c_str(), stl.length());
  
  istringstream iss(string(PS_OStreamContents(gcode_stream())));
  gcode.Parse(this, settings.get_extruder_letters(), m_progress, iss);
  cout << "Slicing complete" << endl;
}

void Model::SliceToSVG(Glib::RefPtr<Gio::File>, bool) {
}
