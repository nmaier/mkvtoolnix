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
	\version \$Id: EbmlVoid.h 1079 2005-03-03 13:18:14Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/
#ifndef LIBEBML_VOID_H
#define LIBEBML_VOID_H

#include "EbmlTypes.h"
#include "EbmlBinary.h"

START_LIBEBML_NAMESPACE

DECLARE_EBML_BINARY(EbmlVoid)
	public:
		EbmlVoid(const EbmlVoid & ElementToClone) :EbmlBinary(ElementToClone){}

		/*!
			\brief Set the size of the data (not the complete size of the element)
		*/
		void SetSize(uint64 aSize) {SetSize_(aSize);}

		/*!
			\note overwrite to write fake data 
		*/
		filepos_t RenderData(IOCallback & output, bool bForceRender, bool bWithDefault = false);

		/*!
			\brief Replace the void element content (written) with this one
		*/
		uint64 ReplaceWith(EbmlElement & EltToReplaceWith, IOCallback & output, bool ComeBackAfterward = true, bool bWithDefault = false);

		/*!
			\brief Void the content of an element
		*/
		uint64 Overwrite(const EbmlElement & EltToVoid, IOCallback & output, bool ComeBackAfterward = true, bool bWithDefault = false);

        EBML_CONCRETE_CLASS(EbmlVoid)
};

END_LIBEBML_NAMESPACE

#endif // LIBEBML_VOID_H
