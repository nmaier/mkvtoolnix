/*****************************************************************************
 *
 *  MPEG Video Packetizer 
 *
 *  Copyright(C) 2004 John Cannon <spyder@matroska.org>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 **/

#ifndef __M2VPARSER_H__
#define __M2VPARSER_H__

#include "MPEGVideoBuffer.h"
#include <stdio.h>
#include <queue>

#define MPV_PARSER_STATE_FRAME       0
#define MPV_PARSER_STATE_NEED_DATA   1
#define MPV_PARSER_STATE_EOS        -1
#define MPV_PARSER_STATE_ERROR      -2

class MPEGFrame {
public:
  binary *data;
  uint32_t size;
  MediaTime duration;
  char frameType;
  MediaTime timecode;
  MediaTime firstRef;
  MediaTime secondRef;
  bool rff;
  bool tff;
  bool progressive;
  uint8_t pictureStructure;
  bool bCopy;

  MPEGFrame(binary* data, uint32_t size, bool bCopy);
  ~MPEGFrame();
};

class M2VParser {
private:        
  std::vector<MPEGChunk*> chunks; //Hold the chunks until we can stamp them
  std::queue<MPEGFrame*> buffers; //Holds stamped buffers until they are requested.
  binary *temp;
  MediaTime position;
  MPEG2SequenceHeader m_seqHdr; //current sequence header
  MediaTime currentStampingTime;
  MediaTime firstRef;
  bool needInit;
  bool m_eos;
  bool notReachedFirstGOP;
  MediaTime nextSkip;
  MediaTime nextSkipDuration;
  MediaTime secondRef;
  uint8_t mpegVersion;
  int32_t parserState;
  MPEGVideoBuffer * mpgBuf;
    
  int32_t InitParser();
  void DumpQueues();
  int32_t FillQueues();
  int32_t CountBFrames();
  void ShoveRef(MediaTime ref);
  MediaTime GetFrameDuration(MPEG2PictureHeader picHdr);
  int32_t QueueFrame(MPEGChunk* seqHdr, MPEGChunk* chunk, MediaTime timecode, MPEG2PictureHeader picHdr);
public:
  M2VParser();
  virtual ~M2VParser();
    
  //Returns the current media position
  virtual const MediaTime GetPosition();  
    
  MPEG2SequenceHeader GetSequenceHeader(){
    return m_seqHdr;
  }
        
  uint8_t GetMPEGVersion(){
    return mpegVersion;
  }
        
  //Returns a pointer to a frame that has been read    
  virtual MPEGFrame * ReadFrame();        
    
  //Returns the max amount of data that can be written to the buffer
  int32_t GetBufferFreeSpace(){
    return mpgBuf->GetFreeSpace();  
  }
        
  //Writes data to the internal buffer.
  int32_t WriteData(binary* data, uint32_t dataSize);
    
  //Returns the current state of the parser
  int32_t GetState();
    
  //Sets "end of stream" status on the buffer, forces timestamping of frames waiting.
  //Do not call this without good reason.
  void SetEOS();
};


#endif //__M2VPARSER_H__
