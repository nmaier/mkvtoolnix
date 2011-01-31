/****************************************************************************
** libebml : parse EBML files, see http://embl.sourceforge.net/
**
** <file/class description>
**
** Copyright (C) 2002-2010 Steve Lhomme.  All rights reserved.
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
	\version \$Id: EbmlSInteger.cpp 347 2010-06-26 09:07:13Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
	\author Moritz Bunkus <moritz @ bunkus.org>
*/
#include <cassert>

#include "ebml/EbmlSInteger.h"

START_LIBEBML_NAMESPACE

EbmlSInteger::EbmlSInteger()
 :EbmlElement(DEFAULT_INT_SIZE, false)
{}

EbmlSInteger::EbmlSInteger(int64 aDefaultValue)
 :EbmlElement(DEFAULT_INT_SIZE, true), Value(aDefaultValue)
{
	SetDefaultIsSet();
}

EbmlSInteger::EbmlSInteger(const EbmlSInteger & ElementToClone)
 :EbmlElement(ElementToClone)
 ,Value(ElementToClone.Value)
 ,DefaultValue(ElementToClone.DefaultValue)
{
}

EbmlSInteger::operator int8() {return  int8(Value);}
EbmlSInteger::operator int16() {return int16(Value);}
EbmlSInteger::operator int32() {return int32(Value);}
EbmlSInteger::operator int64() {return Value;}

/*!
	\todo handle exception on errors
*/
filepos_t EbmlSInteger::RenderData(IOCallback & output, bool bForceRender, bool bWithDefault)
{
	binary FinalData[8]; // we don't handle more than 64 bits integers
	unsigned int i;
	
	if (GetSizeLength() > 8)
		return 0; // integer bigger coded on more than 64 bits are not supported
	
	int64 TempValue = Value;
	for (i=0; i<GetSize();i++) {
		FinalData[GetSize()-i-1] = binary(TempValue & 0xFF);
		TempValue >>= 8;
	}
	
	output.writeFully(FinalData,GetSize());

	return GetSize();
}

uint64 EbmlSInteger::UpdateSize(bool bWithDefault, bool bForceRender)
{
	if (!bWithDefault && IsDefaultValue())
		return 0;

	if (Value <= 0x7F && Value >= (-0x80)) {
		SetSize_(1);
	} else if (Value <= 0x7FFF && Value >= (-0x8000)) {
		SetSize_(2);
	} else if (Value <= 0x7FFFFF && Value >= (-0x800000)) {
		SetSize_(3);
	} else if (Value <= 0x7FFFFFFF && Value >= (-0x80000000)) {
		SetSize_(4);
	} else if (Value <= EBML_PRETTYLONGINT(0x7FFFFFFFFF) &&
		   Value >= EBML_PRETTYLONGINT(-0x8000000000)) {
		SetSize_(5);
	} else if (Value <= EBML_PRETTYLONGINT(0x7FFFFFFFFFFF) &&
		   Value >= EBML_PRETTYLONGINT(-0x800000000000)) {
		SetSize_(6);
	} else if (Value <= EBML_PRETTYLONGINT(0x7FFFFFFFFFFFFF) &&
		   Value >= EBML_PRETTYLONGINT(-0x80000000000000)) {
		SetSize_(7);
	} else {
		SetSize_(8);
	}

	if (GetDefaultSize() > GetSize()) {
		SetSize_(GetDefaultSize());
	}

	return GetSize();
}

filepos_t EbmlSInteger::ReadData(IOCallback & input, ScopeMode ReadFully)
{
	if (ReadFully != SCOPE_NO_DATA)
	{
		binary Buffer[8];
		input.readFully(Buffer, GetSize());
		
		if (Buffer[0] & 0x80)
			Value = -1; // this is a negative value
		else
			Value = 0; // this is a positive value
		
		for (unsigned int i=0; i<GetSize(); i++)
		{
			Value <<= 8;
			Value |= Buffer[i];
		}
		SetValueIsSet();
	}

	return GetSize();
}

bool EbmlSInteger::IsSmallerThan(const EbmlElement *Cmp) const
{
	if (EbmlId(*this) == EbmlId(*Cmp))
		return this->Value < static_cast<const EbmlSInteger *>(Cmp)->Value;
	else
		return false;
}

END_LIBEBML_NAMESPACE
