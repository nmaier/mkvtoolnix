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
	\version \$Id: EbmlHead.cpp 1096 2005-03-17 09:14:52Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/
#include "ebml/EbmlHead.h"
#include "ebml/EbmlSubHead.h"
#include "ebml/EbmlContexts.h"

START_LIBEBML_NAMESPACE

DEFINE_START_SEMANTIC(EbmlHead)
DEFINE_SEMANTIC_ITEM(true, true, EVersion)        ///< EBMLVersion
DEFINE_SEMANTIC_ITEM(true, true, EReadVersion)    ///< EBMLReadVersion
DEFINE_SEMANTIC_ITEM(true, true, EMaxIdLength)    ///< EBMLMaxIdLength
DEFINE_SEMANTIC_ITEM(true, true, EMaxSizeLength)  ///< EBMLMaxSizeLength
DEFINE_SEMANTIC_ITEM(true, true, EDocType)        ///< DocType
DEFINE_SEMANTIC_ITEM(true, true, EDocTypeVersion) ///< DocTypeVersion
DEFINE_SEMANTIC_ITEM(true, true, EDocTypeReadVersion) ///< DocTypeReadVersion
DEFINE_END_SEMANTIC(EbmlHead)

DEFINE_EBML_MASTER_ORPHAN(EbmlHead, 0x1A45DFA3, 4, "EBMLHead\0ratamapaga");

EbmlHead::EbmlHead()
 :EbmlMaster(EbmlHead_Context)
{}

END_LIBEBML_NAMESPACE
