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

#include "Fraction.h"
#include "math.h"


Fraction Fraction::reduce(sint64 iHi, sint64 iLo) {
  if(iLo == 0)
    return Fraction(0,0);

  if(iHi == 0)
    return Fraction(0,1);

  if(iLo == iHi)
    return Fraction(1,1);

  while(((iHi | iLo) & 1) == 0) {
    iHi >>= 1;
    iLo >>= 1;
  }

  while(((iLo%3)==0) && ((iHi%3)==0)) {
    iLo /= 3;
    iHi /= 3;
  }

  sint64 A, B, C;
  if(iLo > iHi) {
    A = iLo;
    B = iHi;
  } else {
    A = iHi;
    B = iLo;
  }

  do {
    C = A % B;
    A = B;
    B = C;
  } while(B > 0);

  iLo /= A;
  iHi /= A;

  return Fraction(iHi, iLo);
}

Fraction::Fraction(void)
: m_Hi(0)
, m_Lo(1)
{
}

Fraction::Fraction(sint64 iValue)
: m_Hi(iValue)
, m_Lo(1)
{
}

Fraction::Fraction(sint64 iHi, sint64 iLo)
: m_Hi(iHi)
, m_Lo(iLo)
{
}

Fraction::Fraction(double fValue) {
  int xp;
  double mant = frexp(fValue, &xp);

  // fValue = mant * 2 ^ xp;
  // 0.5 <= mant < 1.0

  if(mant == 0.5) {
    xp--;
    if(xp > 62)
      m_Hi = _LL(0x7FFFFFFFFFFFFFFF);
    else
      m_Hi = sint64(1) << xp;
    m_Lo = 1;
  } else if(xp > 63) {
    m_Hi = _LL(0x7FFFFFFFFFFFFFFF);
    m_Lo = 1;
  } else if (xp < -63) {
    m_Hi = 0;
    m_Lo = 1;
  } else if (xp >= 0) {
    *this = reduce(sint64(0.5 + ldexp(mant, 62)), _LL(1)<<(62-xp));
  } else {
    *this = reduce(sint64(0.5 + ldexp(mant, xp+62)), _LL(1)<<62);
  }
}

Fraction::Fraction(const Fraction& fraction) {
  m_Hi = fraction.m_Hi;
  m_Lo = fraction.m_Lo;
}

//////////////////////////////////////////////////////////////////////

bool Fraction::operator==(Fraction b) const {
  return (m_Hi * b.m_Lo == m_Lo * b.m_Hi);
}

bool Fraction::operator!=(Fraction b) const {
  return (m_Hi * b.m_Lo != m_Lo * b.m_Hi);
}

bool Fraction::operator<(Fraction b) const {
  return (m_Hi * b.m_Lo < m_Lo * b.m_Hi);
}

bool Fraction::operator<=(Fraction b) const {
  return (m_Hi * b.m_Lo <= m_Lo * b.m_Hi);
}

bool Fraction::operator>(Fraction b) const {
  return (m_Hi * b.m_Lo > m_Lo * b.m_Hi);
}

bool Fraction::operator>=(Fraction b) const {
  return (m_Hi * b.m_Lo >= m_Lo * b.m_Hi);
}

//////////////////////////////////////////////////////////////////////

Fraction Fraction::operator+(Fraction b) const {
  return reduce(m_Hi * b.m_Lo + m_Lo * b.m_Hi, m_Lo * b.m_Lo);
}

Fraction Fraction::operator-(Fraction b) const {
  return reduce(m_Hi * b.m_Lo - m_Lo * b.m_Hi, m_Lo * b.m_Lo);
}

Fraction Fraction::operator*(Fraction b) const {
  return reduce(m_Hi * b.m_Hi, m_Lo * b.m_Lo);
}

Fraction Fraction::operator/(Fraction b) const {
  return reduce(m_Hi * b.m_Lo, m_Lo * b.m_Hi);
}

Fraction Fraction::operator*(sint64 b) const {
  return reduce(m_Hi * b, m_Lo);
}

Fraction Fraction::operator/(sint64 b) const {
  return reduce(m_Hi, m_Lo * b);
}

//////////////////////////////////////////////////////////////////////

sint64 Fraction::scale64t(sint64 b) const {
  return (m_Hi * b) / m_Lo;
}

sint64 Fraction::scale64r(sint64 b) const {
  return (m_Hi * b + (m_Lo >> 1)) / m_Lo;
}

sint64 Fraction::scale64u(sint64 b) const {
  return (m_Hi * b + m_Lo - 1) / m_Lo;
}

sint64 Fraction::scale64it(sint64 b) const {
  return (m_Lo * b) / m_Hi;
}

sint64 Fraction::scale64ir(sint64 b) const {
  return (m_Lo * b + (m_Hi >> 1)) / m_Hi;
}

sint64 Fraction::scale64iu(sint64 b) const {
  return (m_Lo * b + m_Hi - 1) / m_Hi;
}

sint64 Fraction::round64t(void) const {
  return m_Hi / m_Lo;
}

sint64 Fraction::round64r(void) const {
  return (m_Hi + (m_Lo >> 1)) / m_Lo;
}

sint64 Fraction::round64u(void) const {
  return (m_Hi + m_Lo - 1) / m_Lo;
}

//////////////////////////////////////////////////////////////////////

Fraction::operator sint64() const {
  return (m_Hi + (m_Lo >> 1)) / m_Lo;
}

Fraction::operator uint64() const {
  return uint64((m_Hi + (m_Lo >> 1)) / m_Lo);
}

Fraction::operator double() const {
  return (double(m_Hi) / double(m_Lo));
}

//////////////////////////////////////////////////////////////////////
