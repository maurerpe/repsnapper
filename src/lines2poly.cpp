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

#include "lines2poly.h"

const double Lines2Poly::default_epsilon = 1e-6;

Lines2Poly::Lines2Poly(double epsilon_) {
  epsilon = epsilon_;

  head = NULL;
  tail = NULL;
}

Lines2Poly::~Lines2Poly() {
  for (auto it = ptmap.begin(); it != ptmap.end(); ++it)
    delete it->second;
  
  list_element *cur = head;
  while (cur != NULL) {
    list_element *next = cur->next;
    delete cur;
    cur = next;
  }
}

void Lines2Poly::MapInsert(const Vector2d &pt, const position &pos) {
  map<double, position> *mm;
  
  if (ptmap.count(pt[0]) == 0) {
    mm = new map<double, position>();
    ptmap[pt[0]] = mm;
  } else {
    mm = ptmap[pt[0]];
  }
  
  (*mm)[pt[1]] = pos;
}

void Lines2Poly::MapUpdate(const Vector2d &pt, const position &pos) {
  auto mm = ptmap[pt[0]];
  if (mm == NULL) {
    cout << "Updating non-existant point" << endl;
    return;
  }
  
  (*mm)[pt[1]] = pos;
}

void Lines2Poly::MapLookupAndRemove(const Vector2d &pt, position &pos) {
  pos.elem = NULL;
  pos.is_head = false;

  double stop0 = pt[0] + epsilon;
  double stop1 = pt[1] + epsilon;
  for (auto lower0 = ptmap.lower_bound(pt[0] - epsilon); lower0 != ptmap.end() && lower0->first <= stop0; lower0++) {
    auto mm = lower0->second;
    auto lower1 = mm->lower_bound(pt[1] - epsilon);

    if (lower1 != mm->end() && lower1->first <= stop1) {
      pos = lower1->second;
      mm->erase(lower1);
      if (mm->empty()) {
	ptmap.erase(lower0->first);
	delete mm;
      }
      return;
    }
  }
}

void Lines2Poly::NewOpen(const Vector2d &a, const Vector2d &b) {
  list_element *li = new list_element(a, b);

  if (tail == NULL) {
    head = tail = li;
  } else {
    li->prev = tail;
    tail->next = li;
    tail = li;
  }

  position pos(li, true);
  MapInsert(a, pos);

  pos.is_head = false;
  MapInsert(b, pos);
}

void Lines2Poly::RemoveOpen(list_element *li) {
  if (li->prev == NULL)
    head = li->next;
  else
    li->prev->next = li->next;

  if (li->next == NULL)
    tail = li->prev;
  else
    li->next->prev = li->prev;
  
  delete li;
}

void Lines2Poly::AddOpenPt(const position &pos, const Vector2d &pt) {
  if (pos.is_head)
    pos.elem->pts.push_front(pt);
  else
    pos.elem->pts.push_back(pt);
  
  MapInsert(pt, pos);
}

void Lines2Poly::JoinOpen(const position &a, const position &b) {
  if (a.is_head != b.is_head) {
    if (b.is_head) {
      JoinOpen(b, a);
      return;
    }
    
    a.elem->pts.splice(a.elem->pts.begin(), b.elem->pts);
    position pos(a.elem, true);
    MapUpdate(a.elem->pts.front(), pos);
    RemoveOpen(b.elem);
    return;
  }

  if (a.is_head) {
    b.elem->pts.reverse();
    a.elem->pts.splice(a.elem->pts.begin(), b.elem->pts);
    position pos(a.elem, true);
    MapUpdate(a.elem->pts.front(), pos);
    RemoveOpen(b.elem);
    return;
  }
  
  b.elem->pts.reverse();
  a.elem->pts.splice(a.elem->pts.end(), b.elem->pts);
  position pos(a.elem, false);
  MapUpdate(a.elem->pts.back(), pos);
  RemoveOpen(b.elem);
}

void Lines2Poly::Close(const position &pos) {
  shared_ptr<vector<Vector2d>> vec(new vector<Vector2d>);

  auto pts = &pos.elem->pts;
  for (auto it = pts->begin(); it != pts->end(); it++)
    vec->push_back(*it);
  
  closed.AddPoly(vec);
  
  RemoveOpen(pos.elem);
}

void Lines2Poly::AddLine(const Vector2d &a, const Vector2d &b) {
  position ap, bp;
  
  MapLookupAndRemove(a, ap);
  MapLookupAndRemove(b, bp);
  
  if (ap.elem == NULL) {
    if (bp.elem == NULL) {
      NewOpen(a, b);
      return;
    }
    
    AddOpenPt(bp, a);
    return;
  }

  if (bp.elem == NULL) {
    AddOpenPt(ap, b);
    return;
  }
  
  if (ap.elem == bp.elem) {
    Close(ap);
    return;
  }
  
  JoinOpen(ap, bp);
}
