// Some common tools
// Copyright (C) 2003  Julien 'Cyrius' Coloos
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
// or visit http://www.gnu.org/licenses/gpl.html

#ifndef __TOOLS_COMMON_H__
#define __TOOLS_COMMON_H__

#include <stdarg.h>
#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>
#include <list>

// Provide the storage class specifier (extern for an .exe file, null
// for DLL) and the __declspec specifier (dllimport for an .exe file,
// dllexport for DLL).
// You must define TOOLS_DLL_EXPORTS when compiling the DLL.
// You can now use this header file in both the .exe file and DLL - a
// much safer means of using common declarations than two different
// header files.
#ifdef TOOLS_DLL
#  ifdef TOOLS_DLL_EXPORTS
#    define TOOLS_DLL_API __declspec(dllexport)
#    define TOOLS_EXPIMP_TEMPLATE
#  else
#    define TOOLS_DLL_API __declspec(dllimport)
#    define TOOLS_EXPIMP_TEMPLATE extern
#  endif
#else
#  define TOOLS_DLL_API
#  define TOOLS_EXPIMP_TEMPLATE
#endif

#ifndef _LL
  #ifdef _MSC_VER
    #define _LL(i)  i ## i64
  #else
    #define _LL(i)  i ## ll
  #endif
#endif

// Fix taken from VirtualDub (c) Avery Lee
///////////////////////////////////////////////////////////////////////////
//
//  STL fixes
//
///////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#if _MSC_VER < 1300
  namespace std {
    template<typename T>
    inline const T& min(const T& x, const T& y) {
      return _cpp_min(x, y);
    }

    template<class T>
    inline const T& max(const T& x, const T& y) {
      return _cpp_max(x, y);
    }
  };
#endif
#endif

#ifdef _MSC_VER
typedef signed __int8    sint8;
typedef signed __int16    sint16;
typedef signed __int32    sint32;
typedef signed __int64    sint64;
typedef unsigned __int8    uint8;
typedef unsigned __int16  uint16;
typedef unsigned __int32  uint32;
typedef unsigned __int64  uint64;
#else
typedef signed char      sint8;
typedef signed short int  sint16;
typedef signed long      sint32;
typedef signed long long  sint64;
typedef unsigned char    uint8;
typedef unsigned short int  uint16;
typedef unsigned int     uint32;
typedef unsigned long long  uint64;
#endif


#define RoundToLong(f)  (sint32)((f)+0.5)
#define RoundToULong(f)  (uint32)((f)+0.5)


typedef enum {
  kMsg,
  kMsgWarning,
  kMsgError
} eMsgType;


#ifdef _DEBUG
#define DEBUG_MESSAGE  PrintMessage
#else
#define DEBUG_MESSAGE
#endif

template<class _T>
void Duplicatelist(std::list<_T *> &dst, std::list<_T *> &src) {
  int i;
  for(i = 0; i < src.size(); i++) {
    _T *pNewNode = new _T(*(src[i]));
    dst.push_back(pNewNode);
  }
}

template<class _T>
void Duplicatevector(std::vector<_T *> &dst, std::vector<_T *> &src) {
  int i;
  for(i = 0; i < src.size(); i++) {
    _T *pNewNode = new _T(*(src[i]));
    dst.push_back(pNewNode);
  }
}

template<class _T>
void Clearlist(std::list<_T *>& cont) {
  int i;
  for (i = 0; i < cont.size(); i++)
    delete cont[i];
  cont.clear();
}

template<class _T>
void Clearvector(std::vector<_T *>& cont) {
  int i;
  for (i = 0; i < cont.size(); i++)
    delete cont[i];
  cont.clear();
}


#define CONTAINER_DUPLICATE_0(container, _T, dst, src)  \
  Duplicate##container<_T>(dst, src);

#define CONTAINER_CLEAR_0(container, _T, cont)  \
  Clear##container<_T>(cont);

#define LIST_DUPLICATE(_T, src, dst)      \
  CONTAINER_DUPLICATE_0(list, _T, src, dst)
#define LIST_CLEAR(_T, cont)      \
  CONTAINER_CLEAR_0(list, _T, cont)
#define VECTOR_DUPLICATE(_T, src, dst)      \
  CONTAINER_DUPLICATE_0(vector, _T, src, dst)
#define VECTOR_CLEAR(_T, cont)      \
  CONTAINER_CLEAR_0(vector, _T, cont)


template class std::vector<uint8>;

#endif // __TOOLS_COMMON_H__
