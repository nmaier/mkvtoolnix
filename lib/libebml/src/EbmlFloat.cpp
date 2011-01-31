/****************************************************************************
** libebml : parse EBML files, see http://embl.sourceforge.net/
**
** <file/class description>
**
** Copyright (C) 2002-2010 Steve Lhomme.  All rights reserved.
**
** This file is part of libebml.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public
** License along with this library; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
** See http://www.matroska.org/license/lgpl/ for LGPL licensing information.
**
** Contact license@matroska.org if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

/*!
	\file
	\version \$Id: EbmlFloat.cpp 347 2010-06-26 09:07:13Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/

#include <cassert>

#include "ebml/EbmlFloat.h"

START_LIBEBML_NAMESPACE

EbmlFloat::EbmlFloat(const EbmlFloat::Precision prec)
 :EbmlElement(0, false)
{
	SetPrecision(prec);
}

EbmlFloat::EbmlFloat(const double aDefaultValue, const EbmlFloat::Precision prec)
 :EbmlElement(0, true), Value(aDefaultValue), DefaultValue(aDefaultValue)
{
	SetDefaultIsSet();
	SetPrecision(prec);
}

EbmlFloat::EbmlFloat(const EbmlFloat & ElementToClone)
 :EbmlElement(ElementToClone)
 ,Value(ElementToClone.Value)
 ,DefaultValue(ElementToClone.DefaultValue)
{
}

void EbmlFloat::SetDefaultValue(double aValue)
{
    assert(!DefaultISset());
    DefaultValue = aValue;
    SetDefaultIsSet();
}

const double EbmlFloat::DefaultVal() const
{
    assert(DefaultISset());
    return DefaultValue;
}

EbmlFloat::operator const float() const {return float(Value);}
EbmlFloat::operator const double() const {return double(Value);}


/*!
	\todo handle exception on errors
	\todo handle 10 bits precision
*/
filepos_t EbmlFloat::RenderData(IOCallback & output, bool bForceRender, bool bWithDefault)
{
	assert(GetSize() == 4 || GetSize() == 8);

	if (GetSize() == 4) {
		float val = Value;
		int Tmp;
		memcpy(&Tmp, &val, 4);
		big_int32 TmpToWrite(Tmp);
		output.writeFully(&TmpToWrite.endian(), GetSize());
	} else if (GetSize() == 8) {
		double val = Value;
		int64 Tmp;
		memcpy(&Tmp, &val, 8);
		big_int64 TmpToWrite(Tmp);
		output.writeFully(&TmpToWrite.endian(), GetSize());
	}

	return GetSize();
}

uint64 EbmlFloat::UpdateSize(bool bWithDefault, bool bForceRender)
{
	if (!bWithDefault && IsDefaultValue())
		return 0;
	return GetSize();
}

/*!
	\todo remove the hack for possible endianess pb (test on little & big endian)
*/
filepos_t EbmlFloat::ReadData(IOCallback & input, ScopeMode ReadFully)
{
	if (ReadFully != SCOPE_NO_DATA)
	{
		binary Buffer[20];
		assert(GetSize() <= 20);
		input.readFully(Buffer, GetSize());

		if (GetSize() == 4) {
			big_int32 TmpRead;
			TmpRead.Eval(Buffer);
			int32 tmpp = int32(TmpRead);
			float val;
			memcpy(&val, &tmpp, 4);
			Value = val;
			SetValueIsSet();
		} else if (GetSize() == 8) {
			big_int64 TmpRead;
			TmpRead.Eval(Buffer);
			int64 tmpp = int64(TmpRead);
			double val;
			memcpy(&val, &tmpp, 8);
			Value = val;
			SetValueIsSet();
		}
	}

	return GetSize();
}

bool EbmlFloat::IsSmallerThan(const EbmlElement *Cmp) const
{
	if (EbmlId(*this) == EbmlId(*Cmp))
		return this->Value < static_cast<const EbmlFloat *>(Cmp)->Value;
	else
		return false;
}

END_LIBEBML_NAMESPACE
