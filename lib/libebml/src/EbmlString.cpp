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
	\version \$Id: EbmlString.cpp 347 2010-06-26 09:07:13Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/
#include <cassert>

#include "ebml/EbmlString.h"

START_LIBEBML_NAMESPACE

EbmlString::EbmlString()
 :EbmlElement(0, false)
{
	SetDefaultSize(0);
/* done automatically	
	SetSize_(Value.length());
	if (GetDefaultSize() > GetSize())
		SetSize_(GetDefaultSize());*/
}

EbmlString::EbmlString(const std::string & aDefaultValue)
 :EbmlElement(0, true), Value(aDefaultValue), DefaultValue(aDefaultValue)
{
	SetDefaultSize(0);
	SetDefaultIsSet();
/* done automatically	
	SetSize_(Value.length());
	if (GetDefaultSize() > GetSize())
		SetSize_(GetDefaultSize());*/
}

/*!
	\todo Cloning should be on the same exact type !
*/
EbmlString::EbmlString(const EbmlString & ElementToClone)
 :EbmlElement(ElementToClone)
 ,Value(ElementToClone.Value)
 ,DefaultValue(ElementToClone.DefaultValue)
{
}

void EbmlString::SetDefaultValue(std::string & aValue)
{
    assert(!DefaultISset());
    DefaultValue = aValue;
    SetDefaultIsSet();
}

const std::string & EbmlString::DefaultVal() const
{
    assert(DefaultISset());
    return DefaultValue;
}


/*!
	\todo handle exception on errors
*/
filepos_t EbmlString::RenderData(IOCallback & output, bool bForceRender, bool bWithDefault)
{
	filepos_t Result;
	output.writeFully(Value.c_str(), Value.length());
	Result = Value.length();
	
	if (Result < GetDefaultSize()) {
		// pad the rest with 0
		binary *Pad = new binary[GetDefaultSize() - Result];
		if (Pad == NULL)
		{
			return Result;
		}
		memset(Pad, 0x00, GetDefaultSize() - Result);
		output.writeFully(Pad, GetDefaultSize() - Result);
		Result = GetDefaultSize();
		delete [] Pad;
	}
	
	return Result;
}

EbmlString::operator const std::string &() const {return Value;}

EbmlString & EbmlString::operator=(const std::string & NewString)
{
	Value = NewString;
	SetValueIsSet();
/* done automatically	
	SetSize_(Value.length());
	if (GetDefaultSize() > GetSize())
		SetSize_(GetDefaultSize());*/
	return *this;
}

uint64 EbmlString::UpdateSize(bool bWithDefault, bool bForceRender)
{
	if (!bWithDefault && IsDefaultValue())
		return 0;

	if (Value.length() < GetDefaultSize()) {
		SetSize_(GetDefaultSize());
	} else {
		SetSize_(Value.length());
	}
	return GetSize();
}

filepos_t EbmlString::ReadData(IOCallback & input, ScopeMode ReadFully)
{
	if (ReadFully != SCOPE_NO_DATA)
	{
		if (GetSize() == 0) {
			Value = "";
			SetValueIsSet();
		} else {
			char *Buffer = new char[GetSize() + 1];
			if (Buffer == NULL) {
				// unable to store the data, skip it
				input.setFilePointer(GetSize(), seek_current);
			} else {
				input.readFully(Buffer, GetSize());
				if (Buffer[GetSize()-1] != '\0') {
					Buffer[GetSize()] = '\0';
				}
				Value = Buffer;
				delete [] Buffer;
				SetValueIsSet();
			}
		}
	}

	return GetSize();
}

END_LIBEBML_NAMESPACE
