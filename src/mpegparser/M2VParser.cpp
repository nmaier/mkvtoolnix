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

#include "M2VParser.h"

#include "common.h"

#define BUFF_SIZE 2*1024*1024

MPEGFrame::MPEGFrame(binary* data, uint32_t size, bool bCopy){
  if(bCopy){
    this->data  = (binary *)safemalloc(size);
    memcpy(this->data, data, size);
  }else{
    this->data = data;
  }
  this->bCopy = bCopy;
  this->size = size;
  firstRef = -1;
  secondRef = -1;
}

MPEGFrame::~MPEGFrame(){
  safefree(data);
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

/*Fraction ConvertFramerateToFraction(float frameRate){
  if(frameRate == 24000/1001.0f)
  return Fraction(24000,1001);
  else if(frameRate == 30000/1001.0f)
  return Fraction(30000,1001);
  else
  return Fraction(frameRate);
  }*/

M2VParser::M2VParser(){
    
  mpgBuf = new MPEGVideoBuffer(BUFF_SIZE);
    
  notReachedFirstGOP = true;
  currentStampingTime = 0;    
  position = 0;
  m_eos = false;
  mpegVersion = 1;
  needInit = true;
  parserState = MPV_PARSER_STATE_NEED_DATA;
  firstRef = -1;
  secondRef = -1;
  nextSkip = -1;
  nextSkipDuration = -1;
  seqHdrChunk = NULL;
}

int32_t M2VParser::InitParser(){
  //Gotta find a sequence header now
  MPEGChunk* chunk;
  //MPEGChunk* seqHdrChunk;    
  for(int i = 0; i < chunks.size(); i++){
    chunk = chunks[i];        
    if(chunk->GetType() == MPEG_VIDEO_SEQUENCE_START_CODE){
      //Copy the header for later, we must copy because the actual chunk will be deleted in a bit
      binary * hdrData = new binary[chunk->GetSize()];
      memcpy(hdrData, chunk->GetPointer(), chunk->GetSize());      
      seqHdrChunk = new MPEGChunk(hdrData, chunk->GetSize()); //Save this for adding as private data...
      MPEG2SequenceHeader seqHdr = ParseSequenceHeader(chunk);            
      m_seqHdr = seqHdr;

      //Look for sequence extension to identify mpeg2
      binary* pData = chunk->GetPointer();
      for(int i = 3; i < chunk->GetSize() - 4; i++){
        if(pData[i] == 0x00 && pData[i+1] == 0x00 && pData[i+2] == 0x01 && pData[i+3] == 0xb5 && ((pData[i+4] & 0xF0) == 0x10)){
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
  delete seqHdrChunk;
  delete mpgBuf;
}

const MediaTime M2VParser::GetPosition(){
  return position;    
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

int32_t M2VParser::GetState(){
  FillQueues();
  if(!buffers.empty())
    parserState = MPV_PARSER_STATE_FRAME;
  else
    parserState = MPV_PARSER_STATE_NEED_DATA;

  if(m_eos && parserState == MPV_PARSER_STATE_NEED_DATA)
    parserState = MPV_PARSER_STATE_EOS;
  return parserState;
}

int32_t M2VParser::CountBFrames(){
  //We count after the first chunk.
  int32_t count = 0;
  if(m_eos) return 0;
  if(notReachedFirstGOP) return 0;
  for(int i = 1; i < chunks.size(); i++){
    MPEGChunk* c = chunks[i];
    if(c->GetType() == MPEG_VIDEO_PICTURE_START_CODE){
      MPEG2PictureHeader h = ParsePictureHeader(c);
      if(h.frameType == MPEG2_B_FRAME){
        count += GetFrameDuration(h);
      }else{
        return count;
      }
    }
  }
  return -1;
}    

int32_t M2VParser::QueueFrame(MPEGChunk* seqHdr, MPEGChunk* chunk, MediaTime timecode, MPEG2PictureHeader picHdr){
  MPEGFrame* outBuf;
  bool bCopy = true;
  binary* pData = chunk->GetPointer();
  uint32_t dataLen = chunk->GetSize();  
  
  if(seqHdr){
    bCopy = false;
    dataLen += seqHdr->GetSize();
    pData = (binary *)safemalloc(dataLen);
    memcpy(pData, seqHdr->GetPointer(), seqHdr->GetSize());
    memcpy(pData+seqHdr->GetSize(),chunk->GetPointer(), chunk->GetSize());
    delete seqHdr;
    seqHdr = NULL;
  }
    
  MediaTime duration = GetFrameDuration(picHdr);
    
  outBuf = new MPEGFrame(pData, dataLen, bCopy);
    
  if(picHdr.frameType == MPEG2_I_FRAME){
    outBuf->frameType = 'I';
  }else if(picHdr.frameType == MPEG2_P_FRAME){
    outBuf->frameType = 'P';
  }else{
    outBuf->frameType = 'B';
  }
    
  outBuf->timecode = (MediaTime)(timecode * (1000000000/(m_seqHdr.frameRate*2)));
  outBuf->duration = (MediaTime)(duration * (1000000000/(m_seqHdr.frameRate*2)));
    
  if(outBuf->frameType == 'P'){
    outBuf->firstRef = (MediaTime)(firstRef * (1000000000/(m_seqHdr.frameRate*2)));
  }else if(outBuf->frameType == 'B'){
    outBuf->firstRef = (MediaTime)(firstRef * (1000000000/(m_seqHdr.frameRate*2)));
    outBuf->secondRef = (MediaTime)(secondRef * (1000000000/(m_seqHdr.frameRate*2)));
  }
  outBuf->rff = (bool) picHdr.repeatFirstField;
  outBuf->tff = (bool) picHdr.topFieldFirst;
  outBuf->progressive = (bool) picHdr.progressive;
  outBuf->pictureStructure = (uint8_t) picHdr.pictureStructure;
  buffers.push(outBuf);
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

//Reads frames until it can timestamp them, usually reads until it has
//an I, P, B+, P...this way it can stamp them all and set references.
//NOTE: references aren't yet used by our system but they are kept up
//with in this plugin.
int32_t M2VParser::FillQueues(){
  if(chunks.empty()){
    return -1;
  }
  bool done = false;    
  while(!done){
    MediaTime myTime = currentStampingTime;
    MPEGChunk* chunk = chunks.front();
    MPEGChunk* seqHdr = NULL;
    while(chunk->GetType() != MPEG_VIDEO_PICTURE_START_CODE){
      if(chunk->GetType() == MPEG_VIDEO_SEQUENCE_START_CODE){
        if (chunks.size() == 1) return -1;
        if(seqHdr) delete seqHdr;
        m_seqHdr = ParseSequenceHeader(chunk);
        seqHdr = chunk;
      }else{
        delete chunk; //Skip all non picture, non seq headers
      }
      chunks.erase(chunks.begin());
      if(chunks.empty()){
        if(seqHdr != NULL)
          chunks.push_back(seqHdr);
        return -1;
      }
      chunk = chunks.front();
    }
    MPEG2PictureHeader picHdr = ParsePictureHeader(chunk);        
    int bcount;
    if(myTime == nextSkip){
      myTime+=nextSkipDuration;
      currentStampingTime=myTime;
    }
    switch(picHdr.frameType){            
      case MPEG2_I_FRAME:                
        bcount = CountBFrames();
        if(bcount > 0){ //..BBIBB..
          myTime += bcount;          
          nextSkip = myTime;
          nextSkipDuration = GetFrameDuration(picHdr);
        }else{ //IPBB..
          if(bcount == -1 && !m_eos)
            return -1;
          currentStampingTime += GetFrameDuration(picHdr);;
        }
        ShoveRef(myTime);
        QueueFrame(seqHdr,chunk,myTime,picHdr);
        if(notReachedFirstGOP) notReachedFirstGOP = false;
        break;
      case MPEG2_P_FRAME:                
        bcount = CountBFrames();
        if(firstRef == -1) break;
        if(bcount > 0){ //..PBB..
          myTime += bcount;
          nextSkip = myTime;
          nextSkipDuration = GetFrameDuration(picHdr);
        }else{ //..PPBB..
          if(bcount == -1 && !m_eos) return -1;
          currentStampingTime+=GetFrameDuration(picHdr);
        }
        ShoveRef(myTime);
        QueueFrame(seqHdr, chunk, myTime, picHdr);                
        break;
      default: //B-frames                
        if(firstRef == -1 || secondRef == -1) break;
        QueueFrame(seqHdr,chunk,myTime,picHdr);
        currentStampingTime+=GetFrameDuration(picHdr);
    }        
    chunks.erase(chunks.begin());
    delete chunk;
    if(chunks.empty()) return -1;
  }
  return 0;
}

MPEGFrame* M2VParser::ReadFrame(){
  //if(m_eos) return NULL;
  if(GetState() != MPV_PARSER_STATE_FRAME){
    return NULL;
  }
  if(buffers.empty()){
    return NULL; // OOPS!
  }
  MPEGFrame* frame = buffers.front();
  buffers.pop();
  return frame;
}

