/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010  Kulitorum
    Copyright (C) 2011-12  martin.dieringer@gmx.de

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

#include <glibmm.h>
#include <giomm.h>

#include "stdafx.h"
#include "triangle.h"

void save_locales();
void set_locales(const char * loc);
void reset_locales();

enum filetype_t{
    ASCII_STL,
    BINARY_STL,
    NONE_STL,
    VRML,
    SVG,
    UNKNOWN_TYPE
};

class File {
public:
  File(){};
  File(Glib::RefPtr<Gio::File> file);
  virtual ~File(){};

  Glib::RefPtr<Gio::File> _file;
  string _path;
  filetype_t _type;

  static filetype_t getFileType(string path);

  void loadTriangles(vector< vector<Triangle> > &triangles,
		     vector<string> &names,
		     uint max_triangles=0);


  bool load_asciiSTL(vector< vector<Triangle> > &triangles,
		     vector<string> &names,
		     uint max_triangles=0, bool readnormals=false);

  bool load_binarySTL(vector<Triangle> &triangles,
		      uint max_triangles=0, bool readnormals=false);

  bool load_VRML(vector<Triangle> &triangles, uint max_triangles=0);

  static bool parseSTLtriangles_ascii(istream &text,
				      uint max_triangles, bool readnormals,
				      vector<Triangle> &triangles,
				      string &name);

  static bool saveBinarySTL(string filename, const vector<Triangle> &triangles,
			    const Matrix4d &T);
};
