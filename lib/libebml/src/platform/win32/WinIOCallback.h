/****************************************************************************
** libebml : parse EBML files, see http://embl.sourceforge.net/
**
** <file/class description>
**
** Copyright (C) 2002-2003 Steve Lhomme.  All rights reserved.
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
	\version \$Id: WinIOCallback.h 1090 2005-03-16 12:47:59Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
	\author Jory Stone       <jcsston @ toughguy.net>
	\author Cyrius           <suiryc @ users.sf.net>
*/

#ifndef LIBEBML_WINIOCALLBACK_H
#define LIBEBML_WINIOCALLBACK_H

#include <windows.h>
#include <stdexcept>
#include <string>
#include "ebml/IOCallback.h"

START_LIBEBML_NAMESPACE

class WinIOCallback: public IOCallback
{	
public:
	WinIOCallback(const wchar_t* Path, const open_mode aMode, DWORD dwFlags=0);
	WinIOCallback(const char* Path, const open_mode aMode, DWORD dwFlags=0);
	virtual ~WinIOCallback();

	bool open(const wchar_t* Path, const open_mode Mode, DWORD dwFlags=0);
	bool open(const char* Path, const open_mode Mode, DWORD dwFlags=0);

	virtual uint32 read(void*Buffer,size_t Size);
	virtual size_t write(const void*Buffer,size_t Size);
	virtual void setFilePointer(int64 Offset,seek_mode Mode=seek_beginning);
	virtual uint64 getFilePointer();
	virtual void close();
	
	bool IsOk() { return mOk; };	
	const std::string &GetLastErrorStr() { return mLastErrorStr; };
	bool SetEOF();
protected:
	bool mOk;
	std::string mLastErrorStr;
	uint64 mCurrentPosition;

	HANDLE mFile;
};

END_LIBEBML_NAMESPACE

#endif // LIBEBML_WINIOCALLBACK_H
