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
	\version \$Id: EbmlString.h 347 2010-06-26 09:07:13Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/
#ifndef LIBEBML_STRING_H
#define LIBEBML_STRING_H

#include <string>

#include "EbmlTypes.h"
#include "EbmlElement.h"

START_LIBEBML_NAMESPACE

/*!
    \class EbmlString
    \brief Handle all operations on a printable string EBML element
*/
class EBML_DLL_API EbmlString : public EbmlElement {
	public:
		EbmlString();
		EbmlString(const std::string & aDefaultValue);
		EbmlString(const EbmlString & ElementToClone);
	
		virtual ~EbmlString() {}
	
		virtual bool ValidateSize() const {return IsFiniteSize();} // any size is possible
		filepos_t RenderData(IOCallback & output, bool bForceRender, bool bWithDefault = false);
		filepos_t ReadData(IOCallback & input, ScopeMode ReadFully = SCOPE_ALL_DATA);
		filepos_t UpdateSize(bool bWithDefault = false, bool bForceRender = false);
	
		EbmlString & operator=(const std::string &);
		operator const std::string &() const;
	
		void SetDefaultValue(std::string &);
    
		const std::string & DefaultVal() const;

		bool IsDefaultValue() const {
			return (DefaultISset() && Value == DefaultValue);
		}

#if defined(EBML_STRICT_API)
    private:
#else
    protected:
#endif
		std::string Value;  /// The actual value of the element
		std::string DefaultValue;
};

END_LIBEBML_NAMESPACE

#endif // LIBEBML_STRING_H
