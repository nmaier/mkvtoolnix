//  VirtualDub - Video processing and capture application
//  Copyright (C) 1998-2001 Avery Lee
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

//
// Modified by Julien 'Cyrius' Coloos
// 20-09-2003
//

#ifndef f_AVIINDEX_H
#define f_AVIINDEX_H

#include "common_gdivfw.h"

class AVIIndexChainNode;

class AVIIndexEntry2 {
public:
  sint64 pos;
  union {
    uint32_le  ckid;
    int    fileno;
  };
  sint32  size;
};

class AVIIndexEntry3 {
public:
  uint32  dwOffset;
  uint32  dwSizeKeyframe;
};

class AVIIndexChain {
protected:
  AVIIndexChainNode *head, *tail;

  void delete_chain();
public:
  int total_ents;

  AVIIndexChain();
  ~AVIIndexChain();

  bool add(w32AVIINDEXENTRY *avie);
  bool add(AVIIndexEntry2 *avie2);
  bool add(uint32_le ckid, sint64 pos, long len, bool is_keyframe);
  void put(w32AVIINDEXENTRY *avietbl);
  void put(AVIIndexEntry2 *avie2tbl);
  void put(AVIIndexEntry3 *avie3tbl, sint64 offset);
};

class AVIIndex : public AVIIndexChain {
protected:
  w32AVIINDEXENTRY *index;
  AVIIndexEntry2 *index2;
  AVIIndexEntry3 *index3;
  int index_len;

  w32AVIINDEXENTRY *allocateIndex(int total_entries) {
    return index = new w32AVIINDEXENTRY[index_len = total_entries];
  }

  AVIIndexEntry2 *allocateIndex2(int total_entries) {
    return index2 = new AVIIndexEntry2[index_len = total_entries];
  }

  AVIIndexEntry3 *allocateIndex3(int total_entries) {
    return index3 = new AVIIndexEntry3[index_len = total_entries];
  }

public:
  AVIIndex();
  ~AVIIndex();

  bool makeIndex();
  bool makeIndex2();
  bool makeIndex3(sint64 offset);
  void clear();

  w32AVIINDEXENTRY *indexPtr() {
    return index;
  }

  AVIIndexEntry2 *index2Ptr() {
    return index2;
  }

  AVIIndexEntry3 *index3Ptr() {
    return index3;
  }

  AVIIndexEntry2 *takeIndex2() {
    AVIIndexEntry2 *idx = index2;

    index2 = NULL;
    return idx;
  }

  int size() { return total_ents; }

  int indexLen() {
    return index_len;
  }
};

#endif
