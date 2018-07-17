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
