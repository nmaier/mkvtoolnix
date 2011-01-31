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
	\version \$Id: EbmlUInteger.cpp 347 2010-06-26 09:07:13Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
	\author Moritz Bunkus <moritz @ bunkus.org>
*/
#include <cassert>

#include "ebml/EbmlUInteger.h"

START_LIBEBML_NAMESPACE

EbmlUInteger::EbmlUInteger()
 :EbmlElement(DEFAULT_UINT_SIZE, false)
{}

EbmlUInteger::EbmlUInteger(uint64 aDefaultValue)
 :EbmlElement(DEFAULT_UINT_SIZE, true), Value(aDefaultValue), DefaultValue(aDefaultValue)
{
	SetDefaultIsSet();
}

EbmlUInteger::EbmlUInteger(const EbmlUInteger & ElementToClone)
 :EbmlElement(ElementToClone)
 ,Value(ElementToClone.Value)
 ,DefaultValue(ElementToClone.DefaultValue)
{
}

void EbmlUInteger::SetDefaultValue(uint64 aValue)
{
    assert(!DefaultISset());
    DefaultValue = aValue;
    SetDefaultIsSet();
}

uint64 EbmlUInteger::DefaultVal() const
{
    assert(DefaultISset());
    return DefaultValue;
}

EbmlUInteger::operator uint8()  const {return uint8(Value); }
EbmlUInteger::operator uint16() const {return uint16(Value);}
EbmlUInteger::operator uint32() const {return uint32(Value);}
EbmlUInteger::operator uint64() const {return Value;}


/*!
	\todo handle exception on errors
*/
filepos_t EbmlUInteger::RenderData(IOCallback & output, bool bForceRender, bool bWithDefault)
{
	binary FinalData[8]; // we don't handle more than 64 bits integers
	
	if (GetSizeLength() > 8)
		return 0; // integer bigger coded on more than 64 bits are not supported
	
	uint64 TempValue = Value;
	for (unsigned int i=0; i<GetSize();i++) {
		FinalData[GetSize()-i-1] = TempValue & 0xFF;
		TempValue >>= 8;
	}
	
	output.writeFully(FinalData,GetSize());

	return GetSize();
}

uint64 EbmlUInteger::UpdateSize(bool bWithDefault, bool bForceRender)
{
	if (!bWithDefault && IsDefaultValue())
		return 0;

	if (Value <= 0xFF) {
		SetSize_(1);
	} else if (Value <= 0xFFFF) {
		SetSize_(2);
	} else if (Value <= 0xFFFFFF) {
		SetSize_(3);
	} else if (Value <= 0xFFFFFFFF) {
		SetSize_(4);
	} else if (Value <= EBML_PRETTYLONGINT(0xFFFFFFFFFF)) {
		SetSize_(5);
	} else if (Value <= EBML_PRETTYLONGINT(0xFFFFFFFFFFFF)) {
		SetSize_(6);
	} else if (Value <= EBML_PRETTYLONGINT(0xFFFFFFFFFFFFFF)) {
		SetSize_(7);
	} else {
		SetSize_(8);
	}

	if (GetDefaultSize() > GetSize()) {
		SetSize_(GetDefaultSize());
	}

	return GetSize();
}

filepos_t EbmlUInteger::ReadData(IOCallback & input, ScopeMode ReadFully)
{
	if (ReadFully != SCOPE_NO_DATA)
	{
		binary Buffer[8];
		input.readFully(Buffer, GetSize());
		Value = 0;
		
		for (unsigned int i=0; i<GetSize(); i++)
		{
			Value <<= 8;
			Value |= Buffer[i];
		}
		SetValueIsSet();
	}

	return GetSize();
}

bool EbmlUInteger::IsSmallerThan(const EbmlElement *Cmp) const
{
	if (EbmlId(*this) == EbmlId(*Cmp))
		return this->Value < static_cast<const EbmlUInteger *>(Cmp)->Value;
	else
		return false;
}

END_LIBEBML_NAMESPACE
