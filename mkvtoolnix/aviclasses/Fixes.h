//  VirtualDub - Video processing and capture application
//  Copyright (C) 1998-2001 Avery Lee
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

//
// Modified by Julien 'Cyrius' Coloos
// 20-09-2003
//

#ifndef f_FIXES_H
#define f_FIXES_H

#include "common_gdivfw.h"

// Those stupid idiots at Microsoft forgot to change this structure
// when making the VfW headers for Win32.  The result is that the
// AVIStreamHeader is correct in 16-bit Windows, but the 32-bit
// headers define RECT as LONGs instead of SHORTs.  This creates
// invalid AVI files!!!!

typedef struct {
  sint16_le  left;
  sint16_le  top;
  sint16_le  right;
  sint16_le  bottom;
} w32RECT16;

typedef struct {
    uint32_le  fccType;
    uint32_le  fccHandler;
    uint32_le  dwFlags;
    uint16_le  wPriority;
    uint16_le  wLanguage;
    uint32_le  dwInitialFrames;
    uint32_le  dwScale;  
    uint32_le  dwRate;
    uint32_le  dwStart;
    uint32_le  dwLength;
    uint32_le  dwSuggestedBufferSize;
    uint32_le  dwQuality;
    uint32_le  dwSampleSize;
    w32RECT16  rcFrame;
} AVIStreamHeader_fixed;

#endif
