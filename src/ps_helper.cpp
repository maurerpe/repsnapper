/*
    This file is a part of the RepSnapper project.
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

#include <stdexcept>
#include <string>

#include "ps_helper.h"

using namespace std;

Psv::Psv() : v(NULL) {
}

Psv::Psv(ps_value_t *val) : v(val) {
}

Psv::~Psv() {
  PS_FreeValue(v);
}

void Psv::Take(ps_value_t *val) {
  Forget();
  v = val;
}

void Psv::Forget() {
  if (v) {
    PS_FreeValue(v);
    v = NULL;
  }
}

ps_value_t *Psv::operator()(void) const {
  if (v == NULL)
    throw invalid_argument(string("ps_value was null"));
  
  return v;
}

const ps_value_t *Psv::Get(const char *ext, const char *setting) const {
  if (v == NULL)
    throw invalid_argument(string("ps_value was null"));
  
  const ps_value_t *gg = PS_GetMember(PS_GetMember(v, ext, NULL), setting, NULL);
  if (gg == NULL)
    throw invalid_argument(string("Unknown setting ") + string(ext) + "->" + string(setting));
  
  return gg;
}

void Psv::Set(const char *ext, const char *setting, ps_value_t *val) {
  if (v == NULL)
    throw invalid_argument(string("ps_value was null"));
  
  if (PS_AddSetting(v, ext, setting, val) < 0) {
    PS_FreeValue(val);
    throw invalid_argument(string("Could not set ") + string(ext) + "->" + string(setting));
  }
}

void Psv::Set(const char *ext, const char *setting, bool val) {
  Set(ext, setting, PS_NewBoolean(val));
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

void Psv::Set(const char *ext, const char *setting, string str) {
  Set(ext, setting, str.c_str());
}

vector< string > Psv::GetNames(const struct ps_value_t *v) {
  Psvi vi(v);
  vector< string > str;
  
  while (vi.Next())
    str.push_back(string(vi.Key()));
  
  return str;
}

void Psv::MergeActive(const char *active, const struct ps_value_t *merge) {
  if (merge == NULL)
    return;
  
  Psvi ext(merge);
  
  while (ext.Next()) {
    Psvi setting(ext.Data());
    if (string(ext.Key()) == "#active")
      continue;
    
    while (setting.Next())
      Set(ext.Key(), setting.Key(), setting.Data());
  }

  const struct ps_value_t *act = PS_GetMember(merge, "#active", NULL);
  if (act == NULL)
    return;
  
  Psvi setting(act);
  while (setting.Next())
    Set(active, setting.Key(), setting.Data());
}

/////////////////////////////////////////////////////////////////

Pso::Pso(ps_ostream_t *ostream) : os(ostream) {
  if (os == NULL)
    throw invalid_argument(string("ps_ostream was null"));
}

Pso::~Pso() {
  PS_FreeOStream(os);
}

/////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////

Psvi::Psvi(const ps_value_t *v) {
  if ((vi = PS_NewValueIterator(v)) == NULL)
    throw invalid_argument(string("ps_value_iterator was null"));
}

Psvi::~Psvi() {
  PS_FreeValueIterator(vi);
}
