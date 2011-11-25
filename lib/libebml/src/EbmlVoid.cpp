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
	\version \$Id: EbmlVoid.cpp 1232 2005-10-15 15:56:52Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/
#include "ebml/EbmlVoid.h"
#include "ebml/EbmlContexts.h"

START_LIBEBML_NAMESPACE

DEFINE_EBML_CLASS_GLOBAL(EbmlVoid, 0xEC, 1, "EBMLVoid");

EbmlVoid::EbmlVoid()
{
	SetValueIsSet();
}

filepos_t EbmlVoid::RenderData(IOCallback & output, bool /* bForceRender */, bool /* bWithDefault */)
{
	// write dummy data by 4KB chunks
	static binary DummyBuf[4*1024];

	uint64 SizeToWrite = GetSize();
	while (SizeToWrite > 4*1024)
	{
		output.writeFully(DummyBuf, 4*1024);
		SizeToWrite -= 4*1024;
	}
	output.writeFully(DummyBuf, SizeToWrite);
	return GetSize();
}

uint64 EbmlVoid::ReplaceWith(EbmlElement & EltToReplaceWith, IOCallback & output, bool ComeBackAfterward, bool bWithDefault)
{
	EltToReplaceWith.UpdateSize(bWithDefault);
	if (HeadSize() + GetSize() < EltToReplaceWith.GetSize() + EltToReplaceWith.HeadSize()) {
		// the element can't be written here !
		return INVALID_FILEPOS_T;
	}
	if (HeadSize() + GetSize() - EltToReplaceWith.GetSize() - EltToReplaceWith.HeadSize() == 1) {
		// there is not enough space to put a filling element
		return INVALID_FILEPOS_T;
	}

	uint64 CurrentPosition = output.getFilePointer();

	output.setFilePointer(GetElementPosition());
	EltToReplaceWith.Render(output, bWithDefault);

	if (HeadSize() + GetSize() - EltToReplaceWith.GetSize() - EltToReplaceWith.HeadSize() > 1) {
	  // fill the rest with another void element
	  EbmlVoid aTmp;
	  aTmp.SetSize_(HeadSize() + GetSize() - EltToReplaceWith.GetSize() - EltToReplaceWith.HeadSize() - 1); // 1 is the length of the Void ID
	  int HeadBefore = aTmp.HeadSize();
	  aTmp.SetSize_(aTmp.GetSize() - CodedSizeLength(aTmp.GetSize(), aTmp.GetSizeLength(), aTmp.IsFiniteSize()));
	  int HeadAfter = aTmp.HeadSize();
	  if (HeadBefore != HeadAfter) {
		  aTmp.SetSizeLength(CodedSizeLength(aTmp.GetSize(), aTmp.GetSizeLength(), aTmp.IsFiniteSize()) - (HeadAfter - HeadBefore));
	  }
	  aTmp.RenderHead(output, false, bWithDefault); // the rest of the data is not rewritten
	}

	if (ComeBackAfterward) {
		output.setFilePointer(CurrentPosition);
	}

	return GetSize() + HeadSize();
}

uint64 EbmlVoid::Overwrite(const EbmlElement & EltToVoid, IOCallback & output, bool ComeBackAfterward, bool bWithDefault)
{
//	EltToVoid.UpdateSize(bWithDefault);
	if (EltToVoid.GetElementPosition() == 0) {
		// this element has never been written
		return 0;
	}
	if (EltToVoid.GetSize() + EltToVoid.HeadSize() <2) {
		// the element can't be written here !
		return 0;
	}

	uint64 CurrentPosition = output.getFilePointer();

	output.setFilePointer(EltToVoid.GetElementPosition());

	// compute the size of the voided data based on the original one
	SetSize(EltToVoid.GetSize() + EltToVoid.HeadSize() - 1); // 1 for the ID
	SetSize(GetSize() - CodedSizeLength(GetSize(), GetSizeLength(), IsFiniteSize()));
	// make sure we handle even the strange cases
	//uint32 A1 = GetSize() + HeadSize();
	//uint32 A2 = EltToVoid.GetSize() + EltToVoid.HeadSize();
	if (GetSize() + HeadSize() != EltToVoid.GetSize() + EltToVoid.HeadSize()) {
		SetSize(GetSize()-1);
		SetSizeLength(CodedSizeLength(GetSize(), GetSizeLength(), IsFiniteSize()) + 1);
	}

	if (GetSize() != 0) {
		RenderHead(output, false, bWithDefault); // the rest of the data is not rewritten
	}

	if (ComeBackAfterward) {
		output.setFilePointer(CurrentPosition);
	}

	return EltToVoid.GetSize() + EltToVoid.HeadSize();
}

END_LIBEBML_NAMESPACE
