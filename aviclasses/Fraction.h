// VirtualDub - Video processing and capture application
// Copyright (C) 1998-2003 Avery Lee
//
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

#ifndef __TOOLS_FRACTION_H__
#define __TOOLS_FRACTION_H__

#include "ac_common.h"

//
// Idea taken from VirtualDub (c) Avery Lee
//
class TOOLS_DLL_API Fraction {
friend Fraction operator*(sint64 b, const Fraction f);

protected:
  sint64  m_Hi;
  sint64  m_Lo;

public:
  Fraction(void);
  explicit Fraction(sint64 iValue);
  explicit Fraction(sint64 iHi, sint64 iLo);
  explicit Fraction(double fValue);
  Fraction(const Fraction& fraction);

  bool  operator<(Fraction b) const;
  bool  operator<=(Fraction b) const;
  bool  operator>(Fraction b) const;
  bool  operator>=(Fraction b) const;
  bool  operator==(Fraction b) const;
  bool  operator!=(Fraction b) const;

  Fraction operator+(Fraction b) const;
  Fraction operator-(Fraction b) const;
  Fraction operator*(Fraction b) const;
  Fraction operator/(Fraction b) const;

  Fraction operator*(sint64 b) const;
  Fraction operator/(sint64 b) const;

  sint64 scale64t(sint64 b) const;
  sint64 scale64r(sint64 b) const;
  sint64 scale64u(sint64 b) const;
  sint64 scale64it(sint64 b) const;
  sint64 scale64ir(sint64 b) const;
  sint64 scale64iu(sint64 b) const;

  sint64 round64t(void) const;
  sint64 round64r(void) const;
  sint64 round64u(void) const;

  operator sint64(void) const;
  operator uint64(void) const;
  operator double(void) const;

  sint64 getHi(void) { return m_Hi; };
  sint64 getLo(void) { return m_Lo; };

  static Fraction reduce(sint64 iHi, sint64 iLo);
  static inline Fraction reduce64(sint64 iHi, sint64 iLo) { return reduce(iHi, iLo); }
};

inline Fraction operator*(sint64 b, const Fraction f) { return f*b; }

#endif // __TOOLS_FRACTION_H__

//////////////////////////////////////////////////////////////////////
