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

#include "common/common_pch.h"
#include "common/memory.h"
#include "common/output.h"
#include "M2VParser.h"

#define BUFF_SIZE 2*1024*1024

MPEGFrame::MPEGFrame(binary *n_data, uint32_t n_size, bool n_bCopy):
  size(n_size), bCopy(n_bCopy) {

  if(bCopy) {
    data = (binary *)safemalloc(size);
    memcpy(data, n_data, size);
  } else {
    data = n_data;
  }
  invisible      = false;
  firstRef       = -1;
  secondRef      = -1;
  seqHdrData     = nullptr;
  seqHdrDataSize = 0;
}

MPEGFrame::~MPEGFrame(){
  safefree(data);
  safefree(seqHdrData);
}

void M2VParser::SetEOS(){
  MPEGChunk * c;
  while((c = mpgBuf->ReadChunk())){
    chunks.push_back(c);
  }
  mpgBuf->ForceFinal();  //Force the last frame out.
  c = mpgBuf->ReadChunk();
  if(c) chunks.push_back(c);
  FillQueues();
  m_eos = true;
}


int32_t M2VParser::WriteData(binary* data, uint32_t dataSize){
  //If we are at EOS
  if(m_eos)
    return -1;

  if(mpgBuf->Feed(data, dataSize)){
    return -1;
  }

  //Fill the chunks buffer
  while(mpgBuf->GetState() == MPEG2_BUFFER_STATE_CHUNK_READY){
    MPEGChunk * c = mpgBuf->ReadChunk();
    if(c) chunks.push_back(c);
  }

  if(needInit){
    if(InitParser() == 0)
      needInit = false;
  }

  FillQueues();
  if(buffers.empty()){
    parserState = MPV_PARSER_STATE_NEED_DATA;
  }else{
    parserState = MPV_PARSER_STATE_FRAME;
  }

  return 0;
}

void M2VParser::DumpQueues(){
  while(!chunks.empty()){
    delete chunks.front();
    chunks.erase(chunks.begin());
  }
  while(!buffers.empty()){
    delete buffers.front();
    buffers.pop();
  }
}

M2VParser::M2VParser(){

  mpgBuf = new MPEGVideoBuffer(BUFF_SIZE);

  notReachedFirstGOP = true;
  previousTimecode = 0;
  previousDuration = 0;
  queueTime = 0;
  waitExpectedTime = 0;
  probing = false;
  waitSecondField = false;
  m_eos = false;
  mpegVersion = 1;
  needInit = true;
  parserState = MPV_PARSER_STATE_NEED_DATA;
  firstRef = -1;
  secondRef = -1;
  gopNum = -1;
  frameNum = 0;
  gopPts = 0;
  highestPts = 0;
  usePictureFrames = false;
  seqHdrChunk = nullptr;
  gopChunk = nullptr;
  keepSeqHdrsInBitstream = true;
  bFrameMissingReferenceWarning = false;
}

int32_t M2VParser::InitParser(){
  //Gotta find a sequence header now
  MPEGChunk* chunk;
  //MPEGChunk* seqHdrChunk;
  for(size_t i = 0; i < chunks.size(); i++){
    chunk = chunks[i];
    if(chunk->GetType() == MPEG_VIDEO_SEQUENCE_START_CODE){
      //Copy the header for later, we must copy because the actual chunk will be deleted in a bit
      binary * hdrData = new binary[chunk->GetSize()];
      memcpy(hdrData, chunk->GetPointer(), chunk->GetSize());
      seqHdrChunk = new MPEGChunk(hdrData, chunk->GetSize()); //Save this for adding as private data...
      ParseSequenceHeader(chunk, m_seqHdr);

      //Look for sequence extension to identify mpeg2
      binary* pData = chunk->GetPointer();
      for(size_t j = 3; j < chunk->GetSize() - 4; j++){
        if(pData[j] == 0x00 && pData[j+1] == 0x00 && pData[j+2] == 0x01 && pData[j+3] == 0xb5 && ((pData[j+4] & 0xF0) == 0x10)){
          mpegVersion = 2;
          break;
        }
      }
      return 0;
    }
  }
  return -1;
}

M2VParser::~M2VParser(){
  DumpQueues();
  if (!probing && !waitQueue.empty()) {
    mxwarn(Y("Video ended with a shortened group of pictures. Some frames have been dropped. You may want to fix the MPEG2 video stream before attempting to multiplex it.\n"));
  }
  FlushWaitQueue();
  delete seqHdrChunk;
  delete gopChunk;
  delete mpgBuf;
}

MediaTime M2VParser::GetFrameDuration(MPEG2PictureHeader picHdr){
  if(m_seqHdr.progressiveSequence){
    if(!picHdr.topFieldFirst && picHdr.repeatFirstField)
      return 4;
    else if(picHdr.topFieldFirst && picHdr.repeatFirstField)
      return 6;
    else
      return 2;
  }
  if(picHdr.pictureStructure != MPEG2_PICTURE_TYPE_FRAME){
    return 1;
  }
  if(picHdr.progressive && picHdr.repeatFirstField){ //TODO: fix this to support progressive sequences also.
    return 3;
  }else{
    return 2;
  }
}

MPEG2ParserState_e M2VParser::GetState(){
  FillQueues();
  if(!buffers.empty())
    parserState = MPV_PARSER_STATE_FRAME;
  else
    parserState = MPV_PARSER_STATE_NEED_DATA;

  if(m_eos && parserState == MPV_PARSER_STATE_NEED_DATA)
    parserState = MPV_PARSER_STATE_EOS;
  return parserState;
}

void M2VParser::FlushWaitQueue(){
  waitSecondField = false;

  while(!waitQueue.empty()){
    delete waitQueue.front();
    waitQueue.pop();
    if (!m_timecodes.empty())
      m_timecodes.pop_front();
  }
}

void M2VParser::StampFrame(MPEGFrame* frame){
  MediaTime timeunit;

  timeunit = (MediaTime)(1000000000/(m_seqHdr.frameOrFieldRate*2));
  if (m_timecodes.empty())
    frame->timecode = previousTimecode + previousDuration;
  else {
    frame->timecode = m_timecodes.front();
    m_timecodes.pop_front();
  }
  previousTimecode = frame->timecode;
  frame->duration = (MediaTime)(frame->duration * timeunit);
  previousDuration = frame->duration;

  if(frame->frameType == 'P'){
    frame->firstRef = firstRef;
    ShoveRef(previousTimecode);
  }else if(frame->frameType == 'B'){
    frame->firstRef = firstRef;
    frame->secondRef = secondRef;
  } else{
    ShoveRef(previousTimecode);
  }
  frame->stamped = true;
}

int32_t M2VParser::OrderFrame(MPEGFrame* frame){
  MPEGFrame *p = frame;
  bool flushQueue = false;

  if (waitSecondField && (p->pictureStructure == MPEG2_PICTURE_TYPE_FRAME)){
    mxerror(Y("Unexpected picture frame after single field frame. Fix the MPEG2 video stream before attempting to multiplex it.\n"));
  }

  if((p->timecode == queueTime) && waitQueue.empty()){
    if((p->pictureStructure == MPEG2_PICTURE_TYPE_FRAME) || waitSecondField)
      queueTime++;
    StampFrame(p);
    buffers.push(p);
  }else if((p->timecode > queueTime) && waitQueue.empty()){
    waitExpectedTime = p->timecode - 1;
    p->stamped = false;
    waitQueue.push(p);
  }else if(p->timecode > waitExpectedTime){
    p->stamped = false;
    waitQueue.push(p);
  }else if(p->timecode == waitExpectedTime){
    StampFrame(p);
    waitQueue.push(p);
    if((p->pictureStructure == MPEG2_PICTURE_TYPE_FRAME) || waitSecondField)
      flushQueue = true;
  }else{
    StampFrame(p);
    waitQueue.push(p);
  }

  if(flushQueue){
    int i,l;
    waitSecondField = false;

    l = waitQueue.size();
    for(i=0;i<l;i++){
      p = waitQueue.front();
      waitQueue.pop();
      if(!p->stamped) {
        StampFrame(p);
      }
      buffers.push(p);
      if((p->pictureStructure == MPEG2_PICTURE_TYPE_FRAME) || waitSecondField){
        queueTime++;
      }
      if(p->pictureStructure != MPEG2_PICTURE_TYPE_FRAME)
        waitSecondField = !waitSecondField;
    }
    waitSecondField = false;
  }else if (p->pictureStructure != MPEG2_PICTURE_TYPE_FRAME){
    waitSecondField = !waitSecondField;
  }

  return 0;
}

int32_t M2VParser::PrepareFrame(MPEGChunk* chunk, MediaTime timecode, MPEG2PictureHeader picHdr){
  MPEGFrame* outBuf;
  bool bCopy = true;
  binary* pData = chunk->GetPointer();
  uint32_t dataLen = chunk->GetSize();

  if ((seqHdrChunk && keepSeqHdrsInBitstream &&
       (MPEG2_I_FRAME == picHdr.frameType)) || gopChunk) {
    uint32_t pos = 0;
    bCopy = false;
    dataLen +=
      (seqHdrChunk && keepSeqHdrsInBitstream ? seqHdrChunk->GetSize() : 0) +
      (gopChunk ? gopChunk->GetSize() : 0);
    pData = (binary *)safemalloc(dataLen);
    if (seqHdrChunk && keepSeqHdrsInBitstream &&
        (MPEG2_I_FRAME == picHdr.frameType)) {
      memcpy(pData, seqHdrChunk->GetPointer(), seqHdrChunk->GetSize());
      pos += seqHdrChunk->GetSize();
      delete seqHdrChunk;
      seqHdrChunk = nullptr;
    }
    if (gopChunk) {
      memcpy(pData + pos, gopChunk->GetPointer(), gopChunk->GetSize());
      pos += gopChunk->GetSize();
      delete gopChunk;
      gopChunk = nullptr;
    }
    memcpy(pData + pos, chunk->GetPointer(), chunk->GetSize());
  }

  outBuf = new MPEGFrame(pData, dataLen, bCopy);

  if (seqHdrChunk && !keepSeqHdrsInBitstream &&
      (MPEG2_I_FRAME == picHdr.frameType)) {
    outBuf->seqHdrData = (binary *)safemalloc(seqHdrChunk->GetSize());
    outBuf->seqHdrDataSize = seqHdrChunk->GetSize();
    memcpy(outBuf->seqHdrData, seqHdrChunk->GetPointer(),
           outBuf->seqHdrDataSize);
    delete seqHdrChunk;
    seqHdrChunk = nullptr;
  }

  if(picHdr.frameType == MPEG2_I_FRAME){
    outBuf->frameType = 'I';
  }else if(picHdr.frameType == MPEG2_P_FRAME){
    outBuf->frameType = 'P';
  }else{
    outBuf->frameType = 'B';
  }

  outBuf->timecode = timecode; // Still the sequence number

  outBuf->invisible = invisible;
  outBuf->duration = GetFrameDuration(picHdr);
  outBuf->rff = (picHdr.repeatFirstField != 0);
  outBuf->tff = (picHdr.topFieldFirst != 0);
  outBuf->progressive = (picHdr.progressive != 0);
  outBuf->pictureStructure = (uint8_t) picHdr.pictureStructure;

  OrderFrame(outBuf);
  return 0;
}

void M2VParser::ShoveRef(MediaTime ref){
  if(firstRef == -1){
    firstRef = ref;
  }else if(secondRef == -1){
    secondRef = ref;
  }else{
    firstRef = secondRef;
    secondRef = ref;
  }
}

void M2VParser::ClearRef(){
  firstRef  = -1;
  secondRef = -1;
}

//Maintains the time of the last start of GOP and uses the temporal_reference
//field as an offset.
int32_t M2VParser::FillQueues(){
  if(chunks.empty()){
    return -1;
  }
  bool done = false;
  while(!done){
    MediaTime myTime;
    MPEGChunk* chunk = chunks.front();
    while (chunk->GetType() != MPEG_VIDEO_PICTURE_START_CODE) {
      if (chunk->GetType() == MPEG_VIDEO_GOP_START_CODE) {
        ParseGOPHeader(chunk, m_gopHdr);
        if (frameNum != 0) {
          gopPts = highestPts + 1;
        }
        if (gopChunk)
          delete gopChunk;
        gopChunk = chunk;
        gopNum++;
        /* Perform some sanity checks */
        if(waitSecondField){
          mxerror(Y("Single field frame before GOP header detected. Fix the MPEG2 video stream before attempting to multiplex it.\n"));
        }
        if(!waitQueue.empty()){
          mxwarn(Y("Shortened GOP detected. Some frames have been dropped. You may want to fix the MPEG2 video stream before attempting to multiplex it.\n"));
          FlushWaitQueue();
        }
        if(m_gopHdr.brokenLink){
          mxinfo(Y("Found group of picture with broken link. You may want use smart reencode before attempting to multiplex it.\n"));
        }
        // There are too many broken videos to do the following so ReferenceBlock will be wrong for broken videos.
        /*
        if(m_gopHdr.closedGOP){
          ClearRef();
        }
        */
      } else if (chunk->GetType() == MPEG_VIDEO_SEQUENCE_START_CODE) {
        if (seqHdrChunk)
          delete seqHdrChunk;
        ParseSequenceHeader(chunk, m_seqHdr);
        seqHdrChunk = chunk;

      }

      chunks.erase(chunks.begin());
      if (chunks.empty())
        return -1;
      chunk = chunks.front();
    }
    MPEG2PictureHeader picHdr;
    ParsePictureHeader(chunk, picHdr);

    if (picHdr.pictureStructure == 0x03) {
      usePictureFrames = true;
    }
    myTime = gopPts + picHdr.temporalReference;
    invisible = false;

    if (myTime > highestPts)
      highestPts = myTime;

    switch(picHdr.frameType){
      case MPEG2_I_FRAME:
        PrepareFrame(chunk, myTime, picHdr);
        notReachedFirstGOP = false;
        break;
      case MPEG2_P_FRAME:
        if(firstRef == -1) break;
        PrepareFrame(chunk, myTime, picHdr);
        break;
      default: //B-frames
        if(firstRef == -1 || secondRef == -1){
          if(!m_gopHdr.closedGOP && !m_gopHdr.brokenLink){
            if(gopNum > 0){
              mxerror(Y("Found B frame without second reference in a non closed GOP. Fix the MPEG2 video stream before attempting to multiplex it.\n"));
            } else if (!probing && !bFrameMissingReferenceWarning){
              mxwarn(Y("Found one or more B frames without second reference in the first GOP. You may want to fix the MPEG2 video stream or use smart reencode before attempting to multiplex it.\n"));
              bFrameMissingReferenceWarning = true;
            }
          }
          invisible = true;
        }
        PrepareFrame(chunk, myTime, picHdr);
    }
    frameNum++;
    chunks.erase(chunks.begin());
    delete chunk;
    if (chunks.empty())
      return -1;
  }
  return 0;
}

MPEGFrame* M2VParser::ReadFrame(){
  //if(m_eos) return nullptr;
  if(GetState() != MPV_PARSER_STATE_FRAME){
    return nullptr;
  }
  if(buffers.empty()){
    return nullptr; // OOPS!
  }
  MPEGFrame* frame = buffers.front();
  buffers.pop();
  return frame;
}

void
M2VParser::AddTimecode(int64_t timecode) {
  std::list<int64_t>::iterator idx = m_timecodes.begin();
  while ((idx != m_timecodes.end()) && (timecode > *idx))
    idx++;
  m_timecodes.insert(idx, timecode);
}
