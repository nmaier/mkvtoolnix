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
	\version \$Id: EbmlSubHead.h 639 2004-07-09 20:59:14Z mosu $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/
#ifndef LIBEBML_SUBHEAD_H
#define LIBEBML_SUBHEAD_H

#include <string>

#include "EbmlUInteger.h"
#include "EbmlString.h"

START_LIBEBML_NAMESPACE

DECLARE_EBML_UINTEGER(EVersion)
	public:
		EVersion(const EVersion & ElementToClone) : EbmlUInteger(ElementToClone) {}

        EBML_CONCRETE_CLASS(EVersion)
};

DECLARE_EBML_UINTEGER(EReadVersion)
	public:
		EReadVersion(const EReadVersion & ElementToClone) : EbmlUInteger(ElementToClone) {}

        EBML_CONCRETE_CLASS(EReadVersion)
};

DECLARE_EBML_UINTEGER(EMaxIdLength)
	public:
		EMaxIdLength(const EMaxIdLength & ElementToClone) : EbmlUInteger(ElementToClone) {}

        EBML_CONCRETE_CLASS(EMaxIdLength)
};

DECLARE_EBML_UINTEGER(EMaxSizeLength)
	public:
		EMaxSizeLength(const EMaxSizeLength & ElementToClone) : EbmlUInteger(ElementToClone) {}

        EBML_CONCRETE_CLASS(EMaxSizeLength)
};

DECLARE_EBML_STRING(EDocType)
	public:
		EDocType(const EDocType & ElementToClone) : EbmlString(ElementToClone) {}

        EBML_CONCRETE_CLASS(EDocType)
};

DECLARE_EBML_UINTEGER(EDocTypeVersion)
	public:
		EDocTypeVersion(const EDocTypeVersion & ElementToClone) : EbmlUInteger(ElementToClone) {}

        EBML_CONCRETE_CLASS(EDocTypeVersion)
};

DECLARE_EBML_UINTEGER(EDocTypeReadVersion)
	public:
		EDocTypeReadVersion(const EDocTypeReadVersion & ElementToClone) : EbmlUInteger(ElementToClone) {}

        EBML_CONCRETE_CLASS(EDocTypeReadVersion)
};

END_LIBEBML_NAMESPACE

#endif // LIBEBML_SUBHEAD_H
