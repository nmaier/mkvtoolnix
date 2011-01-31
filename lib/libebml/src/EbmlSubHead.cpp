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
	\version \$Id: EbmlSubHead.cpp 639 2004-07-09 20:59:14Z mosu $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/
#include "ebml/EbmlSubHead.h"
#include "ebml/EbmlContexts.h"

START_LIBEBML_NAMESPACE

DEFINE_EBML_UINTEGER_DEF(EVersion,            0x4286, 2, EbmlHead, "EBMLVersion", 1);
DEFINE_EBML_UINTEGER_DEF(EReadVersion,        0x42F7, 2, EbmlHead, "EBMLReadVersion", 1);
DEFINE_EBML_UINTEGER_DEF(EMaxIdLength,        0x42F2, 2, EbmlHead, "EBMLMaxIdLength", 4);
DEFINE_EBML_UINTEGER_DEF(EMaxSizeLength,      0x42F3, 2, EbmlHead, "EBMLMaxSizeLength", 8);
DEFINE_EBML_STRING_DEF  (EDocType,            0x4282, 2, EbmlHead, "EBMLDocType", "matroska");
DEFINE_EBML_UINTEGER_DEF(EDocTypeVersion,     0x4287, 2, EbmlHead, "EBMLDocTypeVersion", 1);
DEFINE_EBML_UINTEGER_DEF(EDocTypeReadVersion, 0x4285, 2, EbmlHead, "EBMLDocTypeReadVersion", 1);

END_LIBEBML_NAMESPACE
