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
	\version \$Id: EbmlBinary.cpp 757 2011-06-12 09:51:25Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
	\author Julien Coloos	<suiryc @ users.sf.net>
*/
#include <cassert>
#include <string>

#include "ebml/EbmlBinary.h"
#include "ebml/StdIOCallback.h"

START_LIBEBML_NAMESPACE

EbmlBinary::EbmlBinary()
 :EbmlElement(0, false), Data(NULL)
{}

EbmlBinary::EbmlBinary(const EbmlBinary & ElementToClone)
 :EbmlElement(ElementToClone)
{
	if (ElementToClone.Data == NULL)
		Data = NULL;
	else {
		Data = (binary *)malloc(GetSize() * sizeof(binary));
		assert(Data != NULL);
		memcpy(Data, ElementToClone.Data, GetSize());
	}
}

EbmlBinary::~EbmlBinary(void) {
	if(Data)
		free(Data);
}

EbmlBinary::operator const binary &() const {return *Data;}


filepos_t EbmlBinary::RenderData(IOCallback & output, bool bForceRender, bool bWithDefault)
{
	output.writeFully(Data,GetSize());

	return GetSize();
}
	
/*!
	\note no Default binary value handled
*/
uint64 EbmlBinary::UpdateSize(bool bWithDefault, bool bForceRender)
{
	return GetSize();
}

filepos_t EbmlBinary::ReadData(IOCallback & input, ScopeMode ReadFully)
{
	if (Data != NULL)
		free(Data);
	
    if (ReadFully == SCOPE_NO_DATA || !GetSize())
	{
		Data = NULL;
		return GetSize();
	}

	Data = (binary *)malloc(GetSize());
    if (Data == NULL)
		throw CRTError(std::string("Error allocating data"));
	SetValueIsSet();
	return input.read(Data, GetSize());
}

bool EbmlBinary::operator==(const EbmlBinary & ElementToCompare) const
{
	return ((GetSize() == ElementToCompare.GetSize()) && !memcmp(Data, ElementToCompare.Data, GetSize()));
}

END_LIBEBML_NAMESPACE
