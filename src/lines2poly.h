/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2018 Paul Maurer

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

#pragma once

#include <list>
#include <map>

#include "stdafx.h"
#include "polygroup.h"

class Lines2Poly {
 public:
  static const double default_epsilon;
  
 private:
  class list_element {
  public:
    list<Vector2d> pts;
    
    list_element *prev;
    list_element *next;

    list_element(const Vector2d &a, const Vector2d &b)
      {pts.push_back(a); pts.push_back(b); prev = NULL; next = NULL;};
  };
  
  class position {
  public:
    list_element *elem;
    bool is_head;

    position() {elem = NULL; is_head = false;};
    position(list_element *elem_, bool is_head_)
      {elem = elem_; is_head = is_head_;};
  };
  
  double epsilon;
  
  PolyGroup closed;

  list_element *head;
  list_element *tail;
  map<double, map<double, position> *> ptmap;
  
  void MapInsert(const Vector2d &pt, const position &pos);
  void MapUpdate(const Vector2d &pt, const position &pos);
  void MapLookupAndRemove(const Vector2d &pt, position &pos);
  void NewOpen(const Vector2d &a, const Vector2d &b);
  void RemoveOpen(list_element *li);
  void AddOpenPt(const position &pos, const Vector2d &pt);
  void JoinOpen(const position &a, const position &b);
  void Close(const position &pos);
  
 public:
  Lines2Poly(double epsilon_ = default_epsilon);
  ~Lines2Poly();

  void AddLine(const Vector2d &a, const Vector2d &b);
  shared_ptr<vector<Triangle>> GetAsTriangles(void) {return closed.GetAsTriangles();};
};
