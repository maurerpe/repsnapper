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

#pragma once

#include <printer_settings.h>
#include <vector>
#include <string>

// Helper classes for printer_settings.h
// These allow raii in c++

using namespace std;

string PS_ToString(const ps_value_t *v);

class Psv {
protected:
  ps_value_t *v;
  
public:
  Psv();
  Psv(ps_value_t *val);
  ~Psv();
  
  void Take(ps_value_t *val);
  void Forget();
  void Disown() {v = NULL;};
  
  ps_value_t *operator()(void) const;
  const ps_value_t *Get(const char *ext, const char *set) const;
  void Set(const char *ext, const char *setting, ps_value_t *val);
  void Set(const char *ext, const char *setting, bool val);
  void Set(const char *ext, const char *setting, int val);
  void Set(const char *ext, const char *setting, double val);
  void Set(const char *ext, const char *setting, const char *val);
  void Set(const char *ext, const char *setting, string str);
  
  vector< string > GetNames() {return GetNames(v);};
  static vector< string > GetNames(const struct ps_value_t *v);

  void MergeActive(const char *active, const struct ps_value_t *merge);
};

class Pso {
protected:
  ps_ostream_t *os;
  
public:
  Pso(ps_ostream_t *ostream);
  ~Pso();

  ps_ostream_t *operator()(void) {return os;};
};

class Psf {
protected:
  FILE *file;
  
public:
  Psf(const char *name);
  ~Psf();

  void close(void);
  FILE *operator()(void) {return file;};
};

class Psvi {
protected:
  ps_value_iterator_t *vi;

public:
  Psvi(const ps_value_t *v);
  ~Psvi();

  int         Next() {return PS_ValueIteratorNext(vi);};
  const char *Key()  {return PS_ValueIteratorKey( vi);};
  ps_value_t *Data() {return PS_ValueIteratorData(vi);};
  
  ps_value_iterator_t *operator()(void) {return vi;};
};

class Pssa {
private:
  size_t max;
  size_t used;
  ps_slice_str_t *arr;

public:
  Pssa(size_t max_entries);
  ~Pssa();
  
  void append(string stl, ps_value_t *val);
  size_t size() {return used;};
  const ps_slice_str_t *operator()() {return arr;};
};
