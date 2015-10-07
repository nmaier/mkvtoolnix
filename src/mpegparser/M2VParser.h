/*****************************************************************************

    MPEG Video Packetizer

    Copyright(C) 2004 John Cannon <spyder@matroska.org>

    This program is free software ; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation ; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY ; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program ; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

 **/

#ifndef MTX_M2VPARSER_H
#define MTX_M2VPARSER_H

#include "common/common_pch.h"

#include "MPEGVideoBuffer.h"
#include <stdio.h>
#include <queue>

enum MPEG2ParserState_e {
  MPV_PARSER_STATE_FRAME,
  MPV_PARSER_STATE_NEED_DATA,
  MPV_PARSER_STATE_EOS,
  MPV_PARSER_STATE_ERROR
};

class MPEGFrame {
public:
  binary *data;
  uint32_t size;
  binary *seqHdrData;
  uint32_t seqHdrDataSize;
  MediaTime duration;
  char frameType;
  MediaTime timecode;
  MediaTime firstRef;
  MediaTime secondRef;
  bool stamped;
  bool invisible;
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
  std::vector<MPEGChunk*> chunks; //Hold the chunks until we can order them
  std::queue<MPEGFrame*> waitQueue; //Holds unstamped buffers until we can stamp them.
  std::queue<MPEGFrame*> buffers; //Holds stamped buffers until they are requested.
  MediaTime previousTimecode;
  MediaTime previousDuration;
  //Added to allow reading the header's raw data, contains first found seq hdr.
  MPEGChunk* seqHdrChunk, *gopChunk;
  MPEG2SequenceHeader m_seqHdr; //current sequence header
  MPEG2GOPHeader m_gopHdr; //current GOP header
  MediaTime queueTime;
  MediaTime waitExpectedTime;
  bool      waitSecondField;
  bool      probing;
  MediaTime firstRef;
  bool needInit;
  bool m_eos;
  bool notReachedFirstGOP;
  bool keepSeqHdrsInBitstream;
  MediaTime secondRef;
  bool invisible;
  int32_t   gopNum;
  MediaTime frameNum;
  MediaTime gopPts;
  MediaTime highestPts;
  bool usePictureFrames;
  uint8_t mpegVersion;
  MPEG2ParserState_e parserState;
  MPEGVideoBuffer * mpgBuf;
  std::list<int64_t> m_timecodes;

  int32_t InitParser();
  void DumpQueues();
  int32_t FillQueues();
  void ShoveRef(MediaTime ref);
  void ClearRef();
  MediaTime GetFrameDuration(MPEG2PictureHeader picHdr);
  void FlushWaitQueue();
  int32_t OrderFrame(MPEGFrame* frame);
  void StampFrame(MPEGFrame* frame);
  int32_t PrepareFrame(MPEGChunk* chunk, MediaTime timecode, MPEG2PictureHeader picHdr);
public:
  M2VParser();
  virtual ~M2VParser();

  MPEG2SequenceHeader GetSequenceHeader(){
    return m_seqHdr;
  }

  //BE VERY CAREFUL WITH THIS CALL
  MPEGChunk * GetRealSequenceHeader(){
    return seqHdrChunk;
  }

  uint8_t GetMPEGVersion() const{
    return mpegVersion;
  }

  //Returns a pointer to a frame that has been read
  virtual MPEGFrame * ReadFrame();

  //Returns the max amount of data that can be written to the buffer
  int32_t GetFreeBufferSpace(){
    return mpgBuf->GetFreeBufferSpace();
  }

  //Writes data to the internal buffer.
  int32_t WriteData(binary* data, uint32_t dataSize);

  //Returns the current state of the parser
  MPEG2ParserState_e GetState();

  //Sets "end of stream" status on the buffer, forces timestamping of frames waiting.
  //Do not call this without good reason.
  void SetEOS();

  void SeparateSequenceHeaders() {
    keepSeqHdrsInBitstream = false;
  }

  //Using this method if the M2VParser will process only the beginning of the file.
  void SetProbeMode() {
    probing = true;
  }

  void AddTimecode(int64_t timecode);
};


#endif //MTX_M2VPARSER_H
