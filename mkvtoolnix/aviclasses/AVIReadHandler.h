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

#ifndef f_AVIREADHANDLER_H
#define f_AVIREADHANDLER_H

#include "mm_io.h"
#include "common_gdivfw.h"

// These are meant as AVIFile replacements.  They're not quite to AVIFile
// specs, but they'll do for now.

class IAVIReadStream {
public:
  virtual ~IAVIReadStream();

  virtual long BeginStreaming(long lStart, long lEnd, long lRate)=0;
  virtual long EndStreaming()=0;
  virtual long Info(w32AVISTREAMINFO *pasi, long lSize)=0;
  virtual bool IsKeyFrame(long lFrame)=0;
  virtual long Read(long lStart, long lSamples, void *lpBuffer, long cbBuffer, long *plBytes, long *plSamples)=0;
  virtual long Start()=0;
  virtual long End()=0;
  virtual long PrevKeyFrame(long lFrame)=0;
  virtual long NextKeyFrame(long lFrame)=0;
  virtual long NearestKeyFrame(long lFrame)=0;
  virtual long FormatSize(long lFrame, long *plSize)=0;
  virtual long ReadFormat(long lFrame, void *pFormat, long *plSize)=0;
  virtual bool isStreaming()=0;
  virtual bool isKeyframeOnly()=0;

  virtual bool getVBRInfo(double& bitrate_mean, double& bitrate_stddev, double& maxdev)=0;
};

class IAVIReadHandler {
public:
  virtual void AddRef()=0;
  virtual void Release()=0;
  virtual IAVIReadStream *GetStream(uint32 fccType, sint32 lParam)=0;
  virtual void EnableFastIO(bool)=0;
  virtual bool isOptimizedForRealtime()=0;
  virtual bool isStreaming()=0;
  virtual bool isIndexFabricated()=0;
  //virtual bool AppendFile(const char *pszFile)=0;
  //virtual bool getSegmentHint(const char **ppszPath)=0;
};

IAVIReadHandler *CreateAVIReadHandler(mm_io_c *input);

#endif
