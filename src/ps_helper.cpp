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
#include <unistd.h>

#include <stdexcept>
#include <string>
#include <string.h>

#include "ps_helper.h"

using namespace std;

string PS_ToString(const ps_value_t *v) {
  Pso os(PS_NewStrOStream());

  PS_WriteValue(os(), v);
  
  return string(PS_OStreamContents(os()));
}

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

ps_value_t *Psv::Get(const char *m) const {
  return PS_GetMember(v, m, NULL);
}

ps_value_t *Psv::Get(const char *m1, const char *m2) const {
  return PS_GetMember(PS_GetMember(v, m1, NULL), m2, NULL);
}

ps_value_t *Psv::Get(const char *m1, const char *m2, const char *m3) const {
  return PS_GetMember(PS_GetMember(PS_GetMember(v, m1, NULL), m2, NULL), m3, NULL);
}

ps_value_t *Psv::Get(const char *m1, const char *m2, const char *m3, const char *m4) const {
  return PS_GetMember(PS_GetMember(PS_GetMember(PS_GetMember(v, m1, NULL), m2, NULL), m3, NULL), m4, NULL);
}

ps_value_t *Psv::GetThrow(const char *m1, const char *m2) const {
  if (v == NULL)
    throw invalid_argument(string("ps_value v was null"));
  
  ps_value_t *g = Get(m1, m2);
  
  if (g == NULL)
    throw invalid_argument(string("ps_value g was null"));
  
  return g;
}

void Psv::Set(const char *ext, const char *setting, ps_value_t *val) {
  if (v == NULL)
    throw invalid_argument(string("ps_value was null"));
  
  if (PS_GetMember(v, ext, NULL) == NULL)
    PS_AddMember(v, ext, PS_NewObject());
  
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

Psf::Psf(const char *name, const char *mode) {
  if ((file = fopen(name, mode)) == NULL)
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

/////////////////////////////////////////////////////////////////

Pssa::Pssa(size_t max_entries) {
  max = max_entries;
  used = 0;
  
  arr = new ps_slice_str_t[max];
}

Pssa::~Pssa() {
  for (size_t count = 0; count < used; count++) {
    delete [] arr[count].model_str;
    PS_FreeValue(arr[count].model_settings);
  }
  
  delete [] arr;
}

void Pssa::append(string stl, ps_value_t *val) {
  try {
    if (val == NULL)
      throw invalid_argument("ps_value_t was null");
    
    if (used >= max)
      throw invalid_argument("too many entries in Pssa");
    
    arr[used].model_str = new char[stl.size() + 1];
    memcpy(arr[used].model_str, stl.c_str(), stl.size());
    arr[used].model_str[stl.size()] = '\0';
    arr[used].model_str_len = stl.size();
    arr[used].model_settings = val;
    used++;
  } catch (exception &e) {
    PS_FreeValue(val);
    
    throw e;
  }
}

/////////////////////////////////////////////////////////////////

const char template_base[] = "/tmp/ps_XXXXXX";

Pstemp::Pstemp(string suffix) {
  filename = "";
  file = NULL;
  int fd;
  size_t len = strlen(template_base);
  
  char *name = new char[len + suffix.size() + 1];
  try {
    memcpy(name, template_base, len);
    memcpy(name + len, suffix.c_str(), suffix.size() + 1);
    
    if ((fd = mkstemps(name, suffix.size())) < 0)
      throw invalid_argument("Cannot create temp file");
    
    if ((file = fdopen(fd, "w+")) == NULL) {
      close(fd);
      throw invalid_argument("Cannot reopen temp file");
    }
    
    filename = string(name);
  } catch (exception &e) {
    delete [] name;
    throw e;
  }

  delete [] name;
}

Pstemp::~Pstemp() {
  Close();
  Delete();
}

void Pstemp::Close(void) {
  if (file == NULL)
    return;
  
  fclose(file);
  
  file = NULL;
}

void Pstemp::Delete(void) {
  Close();

  if (filename.size() == 0)
    return;
  
  unlink(filename.c_str());
  
  filename = "";
}
