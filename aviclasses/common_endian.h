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

#ifndef __TOOLS_COMMON_ENDIAN_H__
#define __TOOLS_COMMON_ENDIAN_H__

#include "os.h"

#include "ac_common.h"

#undef LIL_ENDIAN
#undef BIG_ENDIAN
#define LIL_ENDIAN 0x0102
#define BIG_ENDIAN 0x0201

#if WORDS_BIGENDIAN == 1
# define AVI_BYTE_ORDER BIG_ENDIAN
#else
# define AVI_BYTE_ORDER LIL_ENDIAN
#endif

typedef enum {
  kBigEndian,
  kLittleEndian
} eEndianness;

template<class _T, eEndianness _ENDIANNESS>
class EndianValue {
protected:
  _T  m_Value;

public:
  // Defining ctors cause troubles in unions
  //EndianValue<_T, _ENDIANNESS>(void) { };
  //EndianValue<_T, _ENDIANNESS>(const _T& value) {
  //  *this = value;
  //};
  inline operator _T(void) const {
#if AVI_BYTE_ORDER == LIL_ENDIAN
    if(_ENDIANNESS == kLittleEndian)
      return m_Value;
    else {
      _T value = m_Value;
      std::reverse(reinterpret_cast<uint8 *>(&value), reinterpret_cast<uint8 *>((&value)+1));
      return value;
    }
#else
    if(_ENDIANNESS == kBigEndian)
      return m_Value;
    else {
      _T value = m_Value;
      std::reverse(reinterpret_cast<uint8 *>(&value), reinterpret_cast<uint8 *>((&value)+1));
      return value;
    }
#endif
  };
  inline EndianValue<_T, _ENDIANNESS>& operator =(const _T& value) {
    m_Value = value;
#if AVI_BYTE_ORDER == LIL_ENDIAN
    if(_ENDIANNESS == kBigEndian)
      std::reverse(reinterpret_cast<uint8 *>(&m_Value), reinterpret_cast<uint8 *>((&m_Value)+1));
#else
    if(_ENDIANNESS == kLittleEndian)
      std::reverse(reinterpret_cast<uint8 *>(&m_Value), reinterpret_cast<uint8 *>((&m_Value)+1));
#endif
    return *this;
  };
  // If this is defined, we get an error C2622 because union members can't have
  // an assignment operator
  //inline EndianValue<_T, _ENDIANNESS>& operator =(const EndianValue<_T, _ENDIANNESS>& value) {
  //  m_Value = value.m_Value;
  //  return *this;
  //};
  // The 'conventional' binary operators mustn't be defined (at least
  // with MSVC) because a proper conversion is already done by the
  // compiler (overloading again the operator generates problems)
#define DEFINE_ENDIANVALUE_BINARY_OPERATOR_1(op)            \
  inline EndianValue<_T, _ENDIANNESS>& operator op(const _T& value) {  \
    *this = _T(*this) op value;                    \
    return *this;                          \
  };
#define DEFINE_ENDIANVALUE_BINARY_OPERATOR_2(op)              \
  inline EndianValue<_T, _ENDIANNESS>& operator op##=(const _T& value) {  \
    *this = _T(*this) op value;                      \
    return *this;                            \
  };
#define DEFINE_ENDIANVALUE_COMPARISON_OPERATOR(op)                \
  inline bool operator op(const EndianValue<_T, _ENDIANNESS>& value) const {  \
    return (m_Value op value.m_Value);                    \
  };
#define DEFINE_ENDIANVALUE_BINARY_OPERATOR(op)            \
DEFINE_ENDIANVALUE_BINARY_OPERATOR_2(op)
  DEFINE_ENDIANVALUE_BINARY_OPERATOR(+)
  DEFINE_ENDIANVALUE_BINARY_OPERATOR(-)
  DEFINE_ENDIANVALUE_BINARY_OPERATOR(*)
  DEFINE_ENDIANVALUE_BINARY_OPERATOR(/)
  DEFINE_ENDIANVALUE_BINARY_OPERATOR(%)
  DEFINE_ENDIANVALUE_BINARY_OPERATOR(&)
  DEFINE_ENDIANVALUE_BINARY_OPERATOR(|)
  DEFINE_ENDIANVALUE_BINARY_OPERATOR(<<)
  DEFINE_ENDIANVALUE_BINARY_OPERATOR(>>)
  DEFINE_ENDIANVALUE_COMPARISON_OPERATOR(==)
  DEFINE_ENDIANVALUE_COMPARISON_OPERATOR(!=)
  DEFINE_ENDIANVALUE_COMPARISON_OPERATOR(<)
  DEFINE_ENDIANVALUE_COMPARISON_OPERATOR(<=)
  DEFINE_ENDIANVALUE_COMPARISON_OPERATOR(>)
  DEFINE_ENDIANVALUE_COMPARISON_OPERATOR(>=)
  inline EndianValue<_T, _ENDIANNESS>& operator ++(void) {
    *this = _T(*this) + 1;
    return *this;
  };
  inline _T operator ++(int) {
    _T value = _T(*this);
    ++*this;
    return value;
  };
  inline EndianValue<_T, _ENDIANNESS>& operator --(void) {
    *this = _T(*this) - 1;
    return *this;
  };
  inline _T operator --(int) {
    _T value = _T(*this);
    --*this;
    return value;
  };
};


// Instantiate some EndianValue<...> classes
// This does not create an object. It only forces the generation of all of the members
// of the class. It exports them from the DLL and imports them into the .exe file.
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<uint16, kLittleEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<uint32, kLittleEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<uint64, kLittleEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<sint16, kLittleEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<sint32, kLittleEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<sint64, kLittleEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<uint16, kBigEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<uint32, kBigEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<uint64, kBigEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<sint16, kBigEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<sint32, kBigEndian>;
TOOLS_EXPIMP_TEMPLATE template class TOOLS_DLL_API EndianValue<sint64, kBigEndian>;

#if AVI_BYTE_ORDER == BIG_ENDIAN
typedef EndianValue<uint16, kLittleEndian>  uint16_le;
typedef EndianValue<uint32, kLittleEndian>  uint32_le;
typedef EndianValue<uint64, kLittleEndian>  uint64_le;
typedef EndianValue<sint16, kLittleEndian>  sint16_le;
typedef EndianValue<sint32, kLittleEndian>  sint32_le;
typedef EndianValue<sint64, kLittleEndian>  sint64_le;
typedef uint16  uint16_be;
typedef uint32  uint32_be;
typedef uint64  uint64_be;
typedef sint16  sint16_be;
typedef sint32  sint32_be;
typedef sint64  sint64_be;
#elif AVI_BYTE_ORDER == LIL_ENDIAN
typedef uint16  uint16_le;
typedef uint32  uint32_le;
typedef uint64  uint64_le;
typedef sint16  sint16_le;
typedef sint32  sint32_le;
typedef sint64  sint64_le;
typedef EndianValue<uint16, kBigEndian>    uint16_be;
typedef EndianValue<uint32, kBigEndian>    uint32_be;
typedef EndianValue<uint64, kBigEndian>    uint64_be;
typedef EndianValue<sint16, kBigEndian>    sint16_be;
typedef EndianValue<sint32, kBigEndian>    sint32_be;
typedef EndianValue<sint64, kBigEndian>    sint64_be;
#endif


#endif // __TOOLS_COMMON_ENDIAN_H__
