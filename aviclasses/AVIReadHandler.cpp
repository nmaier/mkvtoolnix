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

#include "AVIReadHandler.h"
#include "AVIIndex.h"
#include "list.h"
#include "Fixes.h"

#include "common_mmreg.h"
#include <math.h>

#include "common.h"
#include "error.h"

//#define STREAMING_DEBUG

///////////////////////////////////////////////////////////////////////////

/*namespace {
  enum { kVDST_AVIReadHandler = 2 };

  enum {
    kVDM_AvisynthDetected,    // AVI: Avisynth detected. Extended error handling enabled.
    kVDM_OpenDMLIndexDetected,  // AVI: OpenDML hierarchical index detected on stream %d.
    kVDM_IndexMissing,      // AVI: Index not found or damaged -- reconstructing via file scan.
    kVDM_InvalidChunkDetected,  // AVI: Invalid chunk detected at %lld. Enabling aggressive recovery mode.
    kVDM_StreamFailure,      // AVI: Invalid block found at %lld -- disabling streaming.
    kVDM_FixingBadSampleRate,  // AVI: Stream %d has an invalid sample rate. Substituting %lu samples/sec as placeholder.  
  };
}*/

///////////////////////////////////////////////////////////////////////////

// The following comes from the OpenDML 1.0 spec for extended AVI files

// bIndexType codes
//
#define AVI_INDEX_OF_INDEXES 0x00  // when each entry in aIndex
                  // array points to an index chunk

#define AVI_INDEX_OF_CHUNKS 0x01  // when each entry in aIndex
                  // array points to a chunk in the file

#define AVI_INDEX_IS_DATA  0x80  // when each entry is aIndex is
                  // really the data

// bIndexSubtype codes for INDEX_OF_CHUNKS

#define AVI_INDEX_2FIELD  0x01  // when fields within frames
                  // are also indexed
  struct _avisuperindex_entry {
    sint64_le qwOffset;    // absolute file offset, offset 0 is
                // unused entry??
    uint32_le dwSize;      // size of index chunk at this offset
    uint32_le dwDuration;    // time span in stream ticks
  };
  struct _avistdindex_entry {
    uint32_le dwOffset;      // qwBaseOffset + this is absolute file offset
    uint32_le dwSize;      // bit 31 is set if this is NOT a keyframe
  };
  struct _avifieldindex_entry {
    uint32_le  dwOffset;
    uint32_le  dwSize;
    uint32_le  dwOffsetField2;
  };

typedef struct __attribute__((__packed__)) _avisuperindex_chunk {
  uint32_le fcc;          // ’ix##’
  uint32_le cb;          // size of this structure
  uint16_le wLongsPerEntry;    // must be 4 (size of each entry in aIndex array)
  uint8 bIndexSubType;      // must be 0 or AVI_INDEX_2FIELD
  uint8 bIndexType;      // must be AVI_INDEX_OF_INDEXES
  uint32_le nEntriesInUse;    // number of entries in aIndex array that
                // are used
  uint32_le dwChunkId;      // ’##dc’ or ’##db’ or ’##wb’, etc
  uint32_le dwReserved[3];    // must be 0
  struct _avisuperindex_entry aIndex[];
} AVISUPERINDEX, * PAVISUPERINDEX;

typedef struct __attribute__((__packed__)) _avistdindex_chunk {
  uint32_le fcc;          // ’ix##’
  uint32_le cb;
  uint16_le wLongsPerEntry;    // must be sizeof(aIndex[0])/sizeof(DWORD)
  uint8 bIndexSubType;      // must be 0
  uint8 bIndexType;      // must be AVI_INDEX_OF_CHUNKS
  uint32_le nEntriesInUse;    //
  uint32_le dwChunkId;      // ’##dc’ or ’##db’ or ’##wb’ etc..
  sint64_le qwBaseOffset;    // all dwOffsets in aIndex array are
                // relative to this
  uint32_le dwReserved3;      // must be 0
  struct _avistdindex_entry aIndex[];
} AVISTDINDEX, * PAVISTDINDEX;

/*typedef struct _avifieldindex_chunk {
  uint32_le    fcc;
  uint32_le    cb;
  uint16_le    wLongsPerEntry;
  uint8    bIndexSubType;
  uint8    bIndexType;
  uint32_le    nEntriesInUse;
  uint32_le    dwChunkId;
  sint64_le  qwBaseOffset;
  uint32_le    dwReserved3;
  struct  _avifieldindex_entry aIndex[];
} AVIFIELDINDEX, * PAVIFIELDINDEX;*/

///////////////////////////////////////////////////////////////////////////

IAVIReadStream::~IAVIReadStream() {
}

///////////////////////////////////////////////////////////////////////////

class AVIStreamNode;
class AVIReadHandler;

class AVIReadCache {
public:
  long cache_hit_bytes, cache_miss_bytes;
  int reads;

  AVIReadCache(int nlines, int nstream, AVIReadHandler *root, AVIStreamNode *psnData);
  ~AVIReadCache();

  void ResetStatistics();
  bool WriteBegin(sint64 pos, long len);
  void Write(void *buffer, long len);
  void WriteEnd();
  long Read(void *dest, sint64 chunk_pos, sint64 pos, long len);

  long getMaxRead() {
    return (lines_max - 1) << 4;
  }

private:
  AVIStreamNode *psnData;
  sint64 (*buffer)[2];
  int lines_max, lines;
  long read_tail, write_tail, write_hdr;
  int write_offset;
  int stream;
  AVIReadHandler *source;
};

class AVIStreamNode : public ListNode2<AVIStreamNode> {
public:
  AVIStreamHeader_fixed  hdr;
  char          *pFormat;
  long          lFormatLen;
  AVIIndex        index;
  sint64          bytes;
  bool          keyframe_only;
  bool          was_VBR;
  double          bitrate_mean;
  double          bitrate_stddev;
  double          max_deviation;    // seconds
  int            handler_count;
  class AVIReadCache    *cache;
  int            streaming_count;
  sint64          stream_push_pos;
  sint64          stream_bytes;
  int            stream_pushes;
  long          length;
  long          frames;
  List2<class AVIReadStream>  listHandlers;

  AVIStreamNode();
  ~AVIStreamNode();

  void FixVBRAudio();
};

AVIStreamNode::AVIStreamNode() {
  pFormat = NULL;
  bytes = 0;
  handler_count = 0;
  streaming_count = 0;
  
  stream_bytes = 0;
  stream_pushes = 0;
  cache = NULL;

  was_VBR = false;
}

AVIStreamNode::~AVIStreamNode() {
  delete pFormat;
  delete cache;
}

void AVIStreamNode::FixVBRAudio() {
  w32WAVEFORMATEX *pWaveFormat = (w32WAVEFORMATEX *)pFormat;

  // If this is an MP3 stream, undo the Nandub 1152 value.

  if (pWaveFormat->wFormatTag == 0x0055) {
    pWaveFormat->nBlockAlign = 1;
  }

  // Determine VBR statistics.

  was_VBR = true;

  const AVIIndexEntry2 *pent = index.index2Ptr();
  sint64 size_accum = 0;
  sint64 max_dev = 0;
  double size_sq_sum = 0.0;

  for(int i=0; i<frames; ++i) {
    unsigned size = pent[i].size & 0x7ffffffful;
    sint64 mean_center = (bytes * (2*i+1)) / (2*frames);
    sint64 dev = mean_center - (size_accum + (size>>1));

    if (dev<0)
      dev = -dev;

    if (dev > max_dev)
      max_dev = dev;

    size_accum += size;
    size_sq_sum += (double)size*size;
  }

  // I hate probability & sadistics.
  //
  // Var(X) = E(X2) - E(X)^2
  //      = S(X2)/n - (S(x)/n)^2
  //      = (n*S(X2) - S(X)^2)/n^2
  //
  // SD(x) = sqrt(n*S(X2) - S(X)^2) / n

  double frames_per_second = (double)hdr.dwRate / (double)hdr.dwScale;
  double sum1_bits = bytes * 8.0;
  double sum2_bits = size_sq_sum * 64.0;

  bitrate_mean    = (sum1_bits / frames) * frames_per_second;
  bitrate_stddev    = sqrt(frames * sum2_bits - sum1_bits * sum1_bits) / frames * frames_per_second;
  max_deviation    = (double)max_dev * 8.0 / bitrate_mean;

  // Assume that each audio block is of the same duration.

  hdr.dwRate      = (uint32)(bitrate_mean/8.0 + 0.5);
  hdr.dwScale      = pWaveFormat->nBlockAlign;
  hdr.dwSampleSize  = pWaveFormat->nBlockAlign;
}

///////////////////////////////////////////////////////////////////////////

/*class AVIFileDesc : public ListNode2<AVIFileDesc> {
public:
  HANDLE    hFile;
  HANDLE    hFileUnbuffered;
  sint64    i64Size;
};*/

class AVIStreamNode;

class AVIReadHandler : public IAVIReadHandler /*, private File64*/ {
public:
  bool    fDisableFastIO;

  AVIReadHandler(mm_io_c *input);
  virtual ~AVIReadHandler();

  void AddRef();
  void Release();
  IAVIReadStream *GetStream(uint32 fccType, sint32 lParam);
  void EnableFastIO(bool);
  bool isOptimizedForRealtime();
  bool isStreaming();
  bool isIndexFabricated();
  //bool AppendFile(const char *pszFile);
  //bool getSegmentHint(const char **ppszPath);

  void EnableStreaming(int stream);
  void DisableStreaming(int stream);
  void AdjustRealTime(bool fRealTime);
  bool Stream(AVIStreamNode *, sint64 pos);
  sint64 getStreamPtr();
  void FixCacheProblems(class AVIReadStream *);
  long ReadData(int stream, void *buffer, sint64 position, long len);

private:
//  enum { STREAM_SIZE = 65536 };
  enum { STREAM_SIZE = 1048576 };
  enum { STREAM_RT_SIZE = 65536 };
  enum { STREAM_BLOCK_SIZE = 4096 };

  mm_io_c *m_pInput, *m_pUnbufferedInput;

  int ref_count;
  sint64    i64StreamPosition;
  int      streams;
  char    *streamBuffer;
  int      sbPosition;
  int      sbSize;
  long    fStreamsActive;
  int      nRealTime;
  int      nActiveStreamers;
  bool    fFakeIndex;
  sint64    i64Size;
  int      nFiles, nCurrentFile;
  char *    pSegmentHint;

  // Whenever a file is aggressively recovered, do not allow streaming.

  bool    mbFileIsDamaged;

  List2<AVIStreamNode>    listStreams;
  //List2<AVIFileDesc>      listFiles;

  void    _construct(/*const char *pszFile*/);
  void    _parseFile(List2<AVIStreamNode>& streams);
  bool    _parseStreamHeader(List2<AVIStreamNode>& streams, uint32 dwLengthLeft, bool& bIndexDamaged);
  bool    _parseIndexBlock(List2<AVIStreamNode>& streams, int count, sint64);
  void    _parseExtendedIndexBlock(List2<AVIStreamNode>& streams, AVIStreamNode *pasn, sint64 fpos, uint32 dwLength);
  void    _destruct();

  char *    _StreamRead(long& bytes);
  void    _SelectFile(int file);

  bool    _readChunkHeader(uint32_le& pfcc, uint32& pdwLen);
};

IAVIReadHandler *CreateAVIReadHandler(mm_io_c *input) {
  return new AVIReadHandler(input);
}

///////////////////////////////////////////////////////////////////////////

AVIReadCache::AVIReadCache(int nlines, int nstream, AVIReadHandler *root, AVIStreamNode *psnData) {
  buffer = new sint64[nlines][2];
  if (!buffer) throw error_c("out of memory");

  this->psnData  = psnData;
  lines    = 0;
  lines_max  = nlines;
  read_tail  = 0;
  write_tail  = 0;
  stream    = nstream;
  source    = root;
  ResetStatistics();
}

AVIReadCache::~AVIReadCache() {
  delete[] buffer;
}

void AVIReadCache::ResetStatistics() {
  reads    = 0;
  cache_hit_bytes  = cache_miss_bytes = 0;
}

bool AVIReadCache::WriteBegin(sint64 pos, long len) {
  int needed;

  // delete lines as necessary to make room

  needed = 1 + (len+15)/16;

  if (needed > lines_max)
    return false;

  while(lines+needed > lines_max) {
    int siz = (int)((buffer[read_tail][1]+15)/16 + 1);
    read_tail += siz;
    lines -= siz;
    if (read_tail >= lines_max)
      read_tail -= lines_max;
  }

  // write in header

//  _RPT1(0,"\tbeginning write at line %ld\n", write_tail);

  write_hdr = write_tail;
  write_offset = 0;

  buffer[write_tail][0] = pos;
  buffer[write_tail][1] = 0;

  if (++write_tail >= lines_max)
    write_tail = 0;

  return true;
}

void AVIReadCache::Write(void *src, long len) {
  long dest;

  // copy in data

  buffer[write_hdr][1] += len;

  dest = write_tail + (len + write_offset + 15)/16;

  if (dest > lines_max) {
    int tc = (lines_max - write_tail)*16 - write_offset;

    memcpy((char *)&buffer[write_tail][0] + write_offset, src, tc);
    memcpy(&buffer[0][0], (char *)src + tc, len - tc);

    write_tail = (len-tc)/16;
    write_offset = (len-tc)&15;

  } else {
    memcpy((char *)&buffer[write_tail][0] + write_offset, src, len);
    write_tail += (len+write_offset)/16;
    if (write_tail >= lines_max)
      write_tail = 0;

    write_offset = (len+write_offset) & 15;
  }
}

void AVIReadCache::WriteEnd() {
  long cnt = (long)(1 + (buffer[write_hdr][1]+15)/16);
  lines += cnt;
  write_tail = write_hdr + cnt;

  if (write_tail >= lines_max)
    write_tail -= lines_max;

//  _RPT3(0,"\twrite complete -- header at line %d, size %ld, next line %ld\n", write_hdr, (long)buffer[write_hdr][1], write_tail);
}

long AVIReadCache::Read(void *dest, sint64 chunk_pos, sint64 pos, long len) {
  long ptr;
  sint64 offset;

//  _RPT3(0,"Read request: chunk %16I64, pos %16I64d, %ld bytes\n", chunk_pos, pos, len);

  ++reads;

  do {
    // scan buffer looking for a range that contains data

    ptr = read_tail;
    while(ptr != write_tail) {
      offset = pos - buffer[ptr][0];

//    _RPT4(0,"line %8d: pos %16I64d, len %ld bytes (%ld lines)\n", ptr, buffer[ptr][0], (long)buffer[ptr][1], (long)(buffer[ptr][1]+15)/16);

      if (offset>=0 && offset < buffer[ptr][1]) {
        long end;

        // cache hit

//        _RPT1(0, "cache hit (offset %I64d)\n", chunk_pos);

        cache_hit_bytes += len;

        while (cache_hit_bytes > 16777216) {
          cache_miss_bytes >>= 1;
          cache_hit_bytes >>= 1;
        }

        if (len > (long)(buffer[ptr][1]*16 - offset))
          len = (long)(buffer[ptr][1]*16 - offset);

        ptr += 1+((long)offset>>4);
        if (ptr >= lines_max)
          ptr -= lines_max;

        end = ptr + ((len+((long)offset&15)+15)>>4);

        if (end > lines_max) {
          long tc = (lines_max - ptr)*16 - ((long)offset&15);
          memcpy(dest, (char *)&buffer[ptr][0] + ((long)offset&15), tc);
          memcpy((char *)dest + tc, (char *)&buffer[0][0], len-tc);
        } else {
          memcpy(dest, (char *)&buffer[ptr][0] + ((long)offset&15), len);
        }

        return len;
      }

//    _RPT4(0,"[x] line %8d: pos %16I64d, len %ld bytes (%ld lines)\n", ptr, buffer[ptr][0], (long)buffer[ptr][1], (long)(buffer[ptr][1]+15)/16);
      ptr += (long)(1+(buffer[ptr][1] + 15)/16);

      if (ptr >= lines_max)
        ptr -= lines_max;
    }

    if (source->getStreamPtr() > chunk_pos)
      break;

  } while(source->Stream(psnData, chunk_pos));

//  OutputDebugString("cache miss\n");
//  _RPT1(0, "cache miss (offset %I64d)\n", chunk_pos);

  cache_miss_bytes += len;

  while (cache_miss_bytes > 16777216) {
    cache_miss_bytes >>= 1;
    cache_hit_bytes >>= 1;
  }

  return source->ReadData(stream, dest, pos, len);
}

///////////////////////////////////////////////////////////////////////////

class AVIReadStream : public IAVIReadStream, public ListNode2<AVIReadStream> {
  friend class AVIReadHandler;

public:
  AVIReadStream(AVIReadHandler *, AVIStreamNode *, int);
  ~AVIReadStream();

  long BeginStreaming(long lStart, long lEnd, long lRate);
  long EndStreaming();
  long Info(w32AVISTREAMINFO *pasi, long lSize);
  bool IsKeyFrame(long lFrame);
  long Read(long lStart, long lSamples, void *lpBuffer, long cbBuffer, long *plBytes, long *plSamples);
  long Start();
  long End();
  long PrevKeyFrame(long lFrame);
  long NextKeyFrame(long lFrame);
  long NearestKeyFrame(long lFrame);
  long FormatSize(long lFrame, long *plSize);
  long ReadFormat(long lFrame, void *pFormat, long *plSize);
  bool isStreaming();
  bool isKeyframeOnly();
  void Reinit();
  bool getVBRInfo(double& bitrate_mean, double& bitrate_stddev, double& maxdev);

private:
  AVIReadHandler *parent;
  AVIStreamNode *psnData;
  AVIIndexEntry2 *pIndex;
  AVIReadCache *rCache;
  long& length;
  long& frames;
  long sampsize;
  int streamno;
  bool fStreamingEnabled;
  bool fStreamingActive;
  int iStreamTrackCount;
  long lStreamTrackValue;
  long lStreamTrackInterval;
  bool fRealTime;

  sint64    i64CachedPosition;
  AVIIndexEntry2  *pCachedEntry;

};

///////////////////////////////////////////////////////////////////////////

AVIReadStream::AVIReadStream(AVIReadHandler *parent, AVIStreamNode *psnData, int streamno)
:length(psnData->length)
,frames(psnData->frames)
{
  this->parent = parent;
  this->psnData = psnData;
  this->streamno = streamno;

  fStreamingEnabled = false;
  fStreamingActive = false;
  fRealTime = false;

  parent->AddRef();

  pIndex = psnData->index.index2Ptr();
  sampsize = psnData->hdr.dwSampleSize;

  // Hack to imitate Microsoft's parser.  It seems to ignore this value
  // for audio streams.

  if (psnData->hdr.fccType == streamtypeAUDIO)
    sampsize = ((w32WAVEFORMATEX *)psnData->pFormat)->nBlockAlign;

  if (sampsize) {
    i64CachedPosition = 0;
    pCachedEntry = pIndex;
  }

  psnData->listHandlers.AddTail(this);
}

AVIReadStream::~AVIReadStream() {
  EndStreaming();
  // AVIReadStream is a Node in a list owned by AVIReadHandler (its parent)
  // So call Remove() _before_ releasing the parent
  Remove();
  parent->Release();
}

void AVIReadStream::Reinit() {
  pIndex = psnData->index.index2Ptr();
  i64CachedPosition = 0;
  pCachedEntry = pIndex;
}

long AVIReadStream::BeginStreaming(long lStart, long lEnd, long lRate) {
  if (fStreamingEnabled)
    return 0;

//  OutputDebugString(lRate>1500 ? "starting: fast" : "starting: slow");

  if (lRate <= 1500) {
    parent->AdjustRealTime(true);
    fRealTime = true;
  } else
    fRealTime = false;

  if (parent->fDisableFastIO)
    return 0;

  if (!psnData->streaming_count) {
//    if (!(psnData->cache = new AVIReadCache(psnData->hdr.fccType == streamtypeVIDEO ? 65536 : 16384, streamno, parent, psnData)))
//      return AVIERR_MEMORY;

    psnData->stream_bytes = 0;
    psnData->stream_pushes = 0;
    psnData->stream_push_pos = 0;
  }
  ++psnData->streaming_count;

  fStreamingEnabled = true;
  fStreamingActive = false;
  iStreamTrackCount = 0;
  lStreamTrackValue = -1;
  lStreamTrackInterval = -1;
  return 0;
}

long AVIReadStream::EndStreaming() {
  if (!fStreamingEnabled)
    return 0;

  if (fRealTime)
    parent->AdjustRealTime(false);

  if (fStreamingActive)
    parent->DisableStreaming(streamno);

  fStreamingEnabled = false;
  fStreamingActive = false;

  if (!--psnData->streaming_count) {
    delete psnData->cache;
    psnData->cache = NULL;
  }
  return 0;
}

long AVIReadStream::Info(w32AVISTREAMINFO *pasi, long lSize) {
  w32AVISTREAMINFO asi;

  memset(&asi, 0, sizeof asi);

  asi.fccType        = psnData->hdr.fccType;
  asi.fccHandler      = psnData->hdr.fccHandler;
  asi.dwFlags        = psnData->hdr.dwFlags;
  asi.wPriority      = psnData->hdr.wPriority;
  asi.wLanguage      = psnData->hdr.wLanguage;
  asi.dwScale        = psnData->hdr.dwScale;
  asi.dwRate        = psnData->hdr.dwRate;
  asi.dwStart        = psnData->hdr.dwStart;
  asi.dwLength      = psnData->hdr.dwLength;
  asi.dwInitialFrames    = psnData->hdr.dwInitialFrames;
  asi.dwSuggestedBufferSize = psnData->hdr.dwSuggestedBufferSize;
  asi.dwQuality      = psnData->hdr.dwQuality;
  asi.dwSampleSize    = psnData->hdr.dwSampleSize;
  asi.rcFrame.top      = psnData->hdr.rcFrame.top;
  asi.rcFrame.left    = psnData->hdr.rcFrame.left;
  asi.rcFrame.right    = psnData->hdr.rcFrame.right;
  asi.rcFrame.bottom    = psnData->hdr.rcFrame.bottom;

  if (lSize < sizeof asi)
    memcpy(pasi, &asi, lSize);
  else {
    memcpy(pasi, &asi, sizeof asi);
    memset((char *)pasi + sizeof asi, 0, lSize - sizeof asi);
  }

  return 0;
}

bool AVIReadStream::IsKeyFrame(long lFrame) {
  if (sampsize)
    return true;
  else {
    if (lFrame < 0 || lFrame >= length)
      return false;

    return !(pIndex[lFrame].size & 0x80000000);
  }
}

long AVIReadStream::Read(long lStart, long lSamples, void *lpBuffer, long cbBuffer, long *plBytes, long *plSamples) {
  long lActual;

  if (lStart < 0 || lStart >= length || (lSamples <= 0 && lSamples != AVISTREAMREAD_CONVENIENT)) {
    // umm... dummy!  can't read outside of stream!

    if (plBytes) *plBytes = 0;
    if (plSamples) *plSamples = 0;

    return 0;
  }

  // blocked or discrete?

  if (sampsize) {
    AVIIndexEntry2 *avie2, *avie2_limit = pIndex+frames;
    sint64 byte_off = (sint64)lStart * sampsize;
    sint64 bytecnt;
    sint64 actual_bytes=0;
    sint64 block_pos;

    // too small to hold a sample?

    if (lpBuffer && cbBuffer < sampsize) {
      if (plBytes) *plBytes = sampsize * lSamples;
      if (plSamples) *plSamples = lSamples;

      return AVIERR_BUFFERTOOSMALL;
    }

    // find the frame that has the starting sample -- try and work
    // from our last position to save time

    if (byte_off >= i64CachedPosition) {
      block_pos = i64CachedPosition;
      avie2 = pCachedEntry;
      byte_off -= block_pos;
    } else {
      block_pos = 0;
      avie2 = pIndex;
    }

    while(byte_off >= (avie2->size & 0x7FFFFFFF)) {
      byte_off -= (avie2->size & 0x7FFFFFFF);
      block_pos += (avie2->size & 0x7FFFFFFF);
      ++avie2;
    }

    pCachedEntry = avie2;
    i64CachedPosition = block_pos;

    // Client too lazy to specify a size?

    if (lSamples == AVISTREAMREAD_CONVENIENT) {
      lSamples = ((avie2->size & 0x7FFFFFFF) - (long)byte_off) / sampsize;

      if (!lSamples && avie2+1 < avie2_limit)
        lSamples = ((avie2[0].size & 0x7FFFFFFF) + (avie2[1].size & 0x7FFFFFFF) - (long)byte_off) / sampsize;

      if (lSamples < 0)
        lSamples = 1;
    }

    // trim down sample count

    if (lpBuffer && lSamples > cbBuffer / sampsize)
      lSamples = cbBuffer / sampsize;

    if (lStart+lSamples > length)
      lSamples = length - lStart;

    bytecnt = lSamples * sampsize;

    // begin reading frames from this point on

    if (lpBuffer) {
      // detect streaming

      if (fStreamingEnabled) {

        // We consider the client to be streaming if we detect at least
        // 3 consecutive accesses

        if (lStart == lStreamTrackValue) {
          ++iStreamTrackCount;

          if (iStreamTrackCount >= 15) {

            sint64 streamptr = parent->getStreamPtr();
            sint64 fptrdiff = streamptr - avie2->pos;

            if (!parent->isStreaming() || streamptr<0 || (fptrdiff<4194304 && fptrdiff>-4194304)) {
              if (!psnData->cache)
                psnData->cache = new AVIReadCache(psnData->hdr.fccType == streamtypeVIDEO ? 131072 : 16384, streamno, parent, psnData);
              else
                psnData->cache->ResetStatistics();

              if (!fStreamingActive) {
                fStreamingActive = true;
                parent->EnableStreaming(streamno);
              }

#ifdef STREAMING_DEBUG
              OutputDebugString("[a] streaming enabled\n");
#endif
            }
          } else {
#ifdef STREAMING_DEBUG
            OutputDebugString("[a] streaming detected\n");
#endif
          }
        } else {
#ifdef STREAMING_DEBUG
          OutputDebugString("[a] streaming disabled\n");
#endif
          iStreamTrackCount = 0;

          if (fStreamingActive) {
            fStreamingActive = false;
            parent->DisableStreaming(streamno);
          }
        }
      }

      while(bytecnt > 0) {
        long tc;

        tc = (avie2->size & 0x7FFFFFFF) - (long)byte_off;
        if (tc > bytecnt)
          tc = (long)bytecnt;

        if (psnData->cache && fStreamingActive && tc < psnData->cache->getMaxRead()) {
//OutputDebugString("[a] attempting cached read\n");
          lActual = psnData->cache->Read(lpBuffer, avie2->pos, avie2->pos + byte_off + 8, tc);
          psnData->stream_bytes += lActual;
        } else
          lActual = parent->ReadData(streamno, lpBuffer, avie2->pos + byte_off + 8, tc);

        if (lActual < 0)
          break;

        actual_bytes += lActual;
        ++avie2;
        byte_off = 0;

        if (lActual < tc)
          break;

        bytecnt -= tc;
        lpBuffer = (char *)lpBuffer + tc;
      }

      if (actual_bytes < sampsize) {
        if (plBytes) *plBytes = 0;
        if (plSamples) *plSamples = 0;
        return AVIERR_FILEREAD;
      }

      actual_bytes -= actual_bytes % sampsize;

      if (plBytes) *plBytes = (long)actual_bytes;
      if (plSamples) *plSamples = (long)actual_bytes / sampsize;

      lStreamTrackValue = lStart + (long)actual_bytes / sampsize;

    } else {
      if (plBytes) *plBytes = (long)bytecnt;
      if (plSamples) *plSamples = lSamples;
    }

  } else {
    AVIIndexEntry2 *avie2 = &pIndex[lStart];

    if (lpBuffer && (avie2->size & 0x7FFFFFFF) > cbBuffer) {
      if (plBytes) *plBytes = avie2->size & 0x7FFFFFFF;
      if (plSamples) *plSamples = 1;

      return AVIERR_BUFFERTOOSMALL;
    }

    if (lpBuffer) {

      // detect streaming

      if (fStreamingEnabled && lStart != lStreamTrackValue) {
        if (lStreamTrackValue>=0 && lStart-lStreamTrackValue == lStreamTrackInterval) {
          if (++iStreamTrackCount >= 15) {

            sint64 streamptr = parent->getStreamPtr();
            sint64 fptrdiff = streamptr - avie2->pos;

            if (!parent->isStreaming() || streamptr<0 || (fptrdiff<4194304 && fptrdiff>-4194304)) {
              if (!psnData->cache)
                psnData->cache = new AVIReadCache(psnData->hdr.fccType == streamtypeVIDEO ? 131072 : 16384, streamno, parent, psnData);
              else
                psnData->cache->ResetStatistics();

              if (!fStreamingActive) {
                fStreamingActive = true;
                parent->EnableStreaming(streamno);
              }

#ifdef STREAMING_DEBUG
              OutputDebugString("[v] streaming activated\n");
#endif
            }
          } else {
#ifdef STREAMING_DEBUG
            OutputDebugString("[v] streaming detected\n");
#endif
          }
        } else {
          iStreamTrackCount = 0;
#ifdef STREAMING_DEBUG
          OutputDebugString("[v] streaming disabled\n");
#endif
          if (lStreamTrackValue>=0 && lStart > lStreamTrackValue) {
            lStreamTrackInterval = lStart - lStreamTrackValue;
          } else
            lStreamTrackInterval = -1;

          if (fStreamingActive) {
            fStreamingActive = false;
            parent->DisableStreaming(streamno);
          }
        }
            
        lStreamTrackValue = lStart;
      }

      // read data
      
      unsigned size = avie2->size & 0x7FFFFFFF;

      if (psnData->cache && fStreamingActive && size < psnData->cache->getMaxRead()) {
//OutputDebugString("[v] attempting cached read\n");
        lActual = psnData->cache->Read(lpBuffer, avie2->pos, avie2->pos + 8, size);
        psnData->stream_bytes += lActual;
      } else
        lActual = parent->ReadData(streamno, lpBuffer, avie2->pos+8, size);

      if (lActual != size) {
        if (plBytes) *plBytes = 0;
        if (plSamples) *plSamples = 0;
        return AVIERR_FILEREAD;
      }
    }

    if (plBytes) *plBytes = avie2->size & 0x7FFFFFFF;
    if (plSamples) *plSamples = 1;
  }

  if (psnData->cache && fStreamingActive) {

    // Are we experiencing a high rate of cache misses?

    if (psnData->cache->cache_miss_bytes*2 > psnData->cache->cache_hit_bytes && psnData->cache->reads > 50) {

      // sh*t, notify the parent that we have cache misses so it can check which stream is
      // screwing up, and disable streaming on feeds that are too far off

      parent->FixCacheProblems(this);
      iStreamTrackCount = 0;
    }
  }/* else if (fStreamingEnabled) {

    // hmm... our cache got killed!

    iStreamTrackCount = 0;

  }*/

  return 0;
}

long AVIReadStream::Start() {
  return 0;
}

long AVIReadStream::End() {
  return length;
}

long AVIReadStream::PrevKeyFrame(long lFrame) {
  if (sampsize)
    return lFrame>0 ? lFrame-1 : -1;

  if (lFrame < 0)
    return -1;

  if (lFrame >= length)
    lFrame = length;

  while(--lFrame > 0)
    if (!(pIndex[lFrame].size & 0x80000000))
      return lFrame;

  return -1;
}

long AVIReadStream::NextKeyFrame(long lFrame) {
  if (sampsize)
    return lFrame<length ? lFrame+1 : -1;

  if (lFrame < 0)
    return 0;

  if (lFrame >= length)
    return -1;

  while(++lFrame < length)
    if (!(pIndex[lFrame].size & 0x80000000))
      return lFrame;

  return -1;
}

long AVIReadStream::NearestKeyFrame(long lFrame) {
  long lprev;

  if (sampsize)
    return lFrame;

  if (IsKeyFrame(lFrame))
    return lFrame;

  lprev = PrevKeyFrame(lFrame);

  if (lprev < 0)
    return 0;
  else
    return lprev;
}

long AVIReadStream::FormatSize(long lFrame, long *plSize) {
  *plSize = psnData->lFormatLen;
  return 0;
}

long AVIReadStream::ReadFormat(long lFrame, void *pFormat, long *plSize) {
  if (!pFormat) {
    *plSize = psnData->lFormatLen;
    return 0;
  }

  if (*plSize < psnData->lFormatLen) {
    memcpy(pFormat, psnData->pFormat, *plSize);
  } else {
    memcpy(pFormat, psnData->pFormat, psnData->lFormatLen);
    *plSize = psnData->lFormatLen;
  }

  return 0;
}

bool AVIReadStream::isStreaming() {
  return psnData->cache && fStreamingActive && parent->isStreaming();
}

bool AVIReadStream::isKeyframeOnly() {
   return psnData->keyframe_only;
}

bool AVIReadStream::getVBRInfo(double& bitrate_mean, double& bitrate_stddev, double& maxdev) {
  if (psnData->was_VBR) {
    bitrate_mean = psnData->bitrate_mean;
    bitrate_stddev = psnData->bitrate_stddev;
    maxdev = psnData->max_deviation;
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

AVIReadHandler::AVIReadHandler(mm_io_c *input)
:/* pAvisynthClipInfo(0)
, */
  m_pInput(input),
  mbFileIsDamaged(false)
{
  //this->hFile = INVALID_HANDLE_VALUE;
  //this->hFileUnbuffered = INVALID_HANDLE_VALUE;
  //this->paf = NULL;
  ref_count = 1;
  streams=0;
  fStreamsActive = 0;
  fDisableFastIO = false;
  streamBuffer = NULL;
  nRealTime = 0;
  nActiveStreamers = 0;
  fFakeIndex = false;
  nFiles = 1;
  nCurrentFile = 0;
  pSegmentHint = NULL;

  _construct(/*s*/);
}

AVIReadHandler::~AVIReadHandler() {
  _destruct();
}

void AVIReadHandler::_construct(/*const char *pszFile*/) {

  try {
    //AVIFileDesc *pDesc;

    // open file

    /*hFile = CreateFile(pszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (INVALID_HANDLE_VALUE == hFile)
      throw MyWin32Error("Couldn't open %s: %%s", GetLastError(), pszFile);

    hFileUnbuffered = CreateFile(
        pszFile,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING,
        NULL
      );*/

    //i64FilePosition = 0;

//     m_pUnbufferedInput = m_pInput->Clone();
//     if(m_pUnbufferedInput) {
//       m_pUnbufferedInput->setAccess(false, true);
//       if(!m_pUnbufferedInput->IsOpened()) {
//         delete m_pUnbufferedInput;
//         m_pUnbufferedInput = &m_pInput;
//       }
//     } else
      m_pUnbufferedInput = m_pInput;

    m_pInput->setFilePointer2(0, seek_beginning);

    // recursively parse file

    _parseFile(listStreams);

    // Create first link

    i64Size = m_pInput->get_size();

    /*if (!(pDesc = new AVIFileDesc))
      throw error_c("out of memory");

    pDesc->hFile      = hFile;
    pDesc->hFileUnbuffered  = hFileUnbuffered;
    pDesc->i64Size      = i64Size = _sizeFile();

    listFiles.AddHead(pDesc);*/

  } catch(...) {
    _destruct();
    throw;
  }
}

#if 0
bool AVIReadHandler::AppendFile(const char *pszFile) {
  List2<AVIStreamNode> newstreams;
  AVIStreamNode *pasn_old, *pasn_new, *pasn_old_next=NULL, *pasn_new_next=NULL;
  //AVIFileDesc *pDesc;

  nCurrentFile = -1;

  // open file

  /*hFile = CreateFile(pszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

  if (INVALID_HANDLE_VALUE == hFile)
    throw MyWin32Error("Couldn't open %s: %%s", GetLastError(), pszFile);

  hFileUnbuffered = CreateFile(
      pszFile,
      GENERIC_READ,
      FILE_SHARE_READ,
      NULL,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING,
      NULL
    );*/

  try {
    _parseFile(newstreams);

    pasn_old = listStreams.AtHead();
    pasn_new = newstreams.AtHead();

    while(!!(pasn_old_next = pasn_old->NextFromHead()) & !!(pasn_new_next = pasn_new->NextFromHead())) {
      const char *szPrefix;

      switch(pasn_old->hdr.fccType) {
      case streamtypeAUDIO:  szPrefix = "Cannot append segment: The audio streams "; break;
      case streamtypeVIDEO:  szPrefix = "Cannot append segment: The video streams "; break;
      case streamtypeINTERLEAVED:  szPrefix = "Cannot append segment: The DV streams "; break;
      default:        szPrefix = ""; break;
      }

      // If it's not an audio or video stream, why do we care?

      if (szPrefix) {
        // allow ivas as a synonym for vids

        uint32_le fccOld = pasn_old->hdr.fccType;
        uint32_le fccNew = pasn_new->hdr.fccType;

        if (fccOld != fccNew)
          throw error_c("Cannot append segment: The stream types do not match.");

//        if (pasn_old->hdr.fccHandler != pasn_new->hdr.fccHandler)
//          throw error_c("%suse incompatible compression types.", szPrefix);

        // A/B ?= C/D ==> AD ?= BC

        if ((sint64)pasn_old->hdr.dwScale * pasn_new->hdr.dwRate != (sint64)pasn_new->hdr.dwScale * pasn_old->hdr.dwRate)
          throw error_c("%shave different sampling rates (%.5f vs. %.5f)"
              ,szPrefix
              ,(double)pasn_old->hdr.dwRate / pasn_old->hdr.dwScale
              ,(double)pasn_new->hdr.dwRate / pasn_new->hdr.dwScale
              );

        if (pasn_old->hdr.dwSampleSize != pasn_new->hdr.dwSampleSize)
          throw error_c("%shave different block sizes (%d vs %d).", szPrefix, pasn_old->hdr.dwSampleSize, pasn_new->hdr.dwSampleSize);

        // I hate PCMWAVEFORMAT.

        bool fOk = pasn_old->lFormatLen == pasn_new->lFormatLen && !memcmp(pasn_old->pFormat, pasn_new->pFormat, pasn_old->lFormatLen);

        if (pasn_old->hdr.fccType == streamtypeAUDIO)
          if (((w32WAVEFORMATEX *)pasn_old->pFormat)->wFormatTag == WAVE_FORMAT_PCM
            && ((w32WAVEFORMATEX *)pasn_new->pFormat)->wFormatTag == WAVE_FORMAT_PCM)
              fOk = !memcmp(pasn_old->pFormat, pasn_new->pFormat, sizeof(w32PCMWAVEFORMAT));

        if (!fOk)
          throw error_c("%shave different data formats.", szPrefix);
      }

      pasn_old = pasn_old_next;
      pasn_new = pasn_new_next;
    }

    if (pasn_old_next || pasn_new_next)
      throw error_c("Cannot append segment: The segment has a different number of streams.");

    /*if (!(pDesc = new AVIFileDesc))
      throw error_c("out of memory");

    pDesc->hFile      = hFile;
    pDesc->hFileUnbuffered  = hFileUnbuffered;
    pDesc->i64Size      = _sizeFile();*/
  } catch(const error_c&) {
    while(pasn_new = newstreams.RemoveHead())
      delete pasn_new;

    /*CloseHandle(hFile);
    if (hFileUnbuffered != INVALID_HANDLE_VALUE)
      CloseHandle(hFileUnbuffered);*/

    throw;
  }

  // Accept segment; begin merging process.

  pasn_old = listStreams.AtHead();

  while(pasn_old_next = pasn_old->NextFromHead()) {
    pasn_new = newstreams.RemoveHead();

    // Fix up header.

    pasn_old->hdr.dwLength  += pasn_new->hdr.dwLength;

    if (pasn_new->hdr.dwSuggestedBufferSize > pasn_old->hdr.dwSuggestedBufferSize)
      pasn_old->hdr.dwSuggestedBufferSize = pasn_new->hdr.dwSuggestedBufferSize;

    pasn_old->bytes    += pasn_new->bytes;
    pasn_old->frames  += pasn_new->frames;
    pasn_old->length  += pasn_new->length;

    // Merge indices.

    int oldlen = pasn_old->index.indexLen();
    AVIIndexEntry2 *idx_old = pasn_old->index.takeIndex2();
    AVIIndexEntry2 *idx_new = pasn_new->index.index2Ptr();
    int i;

    pasn_old->index.clear();

    for(i=0; i<oldlen; i++) {
      idx_old[i].size ^= 0x80000000;
      pasn_old->index.add(&idx_old[i]);
    }

    delete[] idx_old;

    for(i=pasn_new->index.indexLen(); i; i--) {
      idx_new->size ^= 0x80000000;
      idx_new->pos += (sint64)nFiles << 48;
      pasn_old->index.add(idx_new++);
    }

    pasn_old->index.makeIndex2();

    // Notify all handlers.

    AVIReadStream *pStream, *pStreamNext;

    pStream = pasn_old->listHandlers.AtHead();
    while(pStreamNext = pStream->NextFromHead()) {
      pStream->Reinit();

      pStream = pStreamNext;
    }

    // Next!

    pasn_old = pasn_old_next;
    delete pasn_new;
  }  

  ++nFiles;
  //listFiles.AddTail(pDesc);

  return true;
}
#endif

void AVIReadHandler::_parseFile(List2<AVIStreamNode>& streamlist) {
  uint32_le fccType;
  uint32 dwLength;
  bool index_found = false;
  bool fAcceptIndexOnly = true;
  bool hyperindexed = false;
  bool bScanRequired = false;
  AVIStreamNode *pasn, *pasn_next;
  w32MainAVIHeader avihdr;
  bool bMainAVIHeaderFound = false;

  sint64  i64ChunkMoviPos = 0;
  uint32  dwChunkMoviLength = 0;

  if (!_readChunkHeader(fccType, dwLength))
    throw error_c("Invalid AVI file: File is less than 8 bytes");

  if (fccType != FOURCC_RIFF)
    throw error_c("Invalid AVI file: Not a RIFF file");

  // If the RIFF header is <4 bytes, assume it was an improperly closed
  // file.

  if(m_pInput->read(&fccType, 4) != 4)
    return;

  if (fccType != formtypeAVI)
    throw error_c("Invalid AVI file: RIFF type is not 'AVI'");

  // Aggressive mode recovery does extensive validation and searching to attempt to
  // recover an AVI.  It is significantly slower, however.  We place the flag here
  // so we can set it if something dreadfully wrong is with the AVI.

  bool bAggressive = false;

  // begin parsing chunks

  while(_readChunkHeader(fccType, dwLength)) {

//    _RPT4(0,"%08I64x %08I64x Chunk '%-4s', length %08lx\n", _posFile()+dwLengthLeft, _posFile(), &fccType, dwLength);

    // Invalid FCCs are bad.  If we find one, set the aggressive flag so if a scan
    // starts, we aggressively search for and validate data.

    if (!isValidFOURCC(fccType)) {
      bAggressive = true;
      break;
    }

//     bool bInvalidLength = false;

    switch(fccType) {
    case FOURCC_LIST:
      if(m_pInput->read(&fccType, 4) != 4)
        return;

      // If we find a LIST/movi chunk with zero size, jump straight to reindexing
      // (unclosed AVIFile output).

      if (!dwLength && fccType == listtypeAVIMOVIE) {
        i64ChunkMoviPos = m_pInput->getFilePointer();
        dwChunkMoviLength = 0xFFFFFFF0;
        goto terminate_scan;
      }

      // Some idiot Premiere plugin is writing AVI files with an invalid
      // size field in the LIST/hdrl chunk.

      if (dwLength<4 && fccType != listtypeAVIHEADER)
        throw error_c("Invalid AVI file: LIST chunk <4 bytes");

      if (dwLength < 4)
        dwLength = 0;
      else
        dwLength -= 4;

//      _RPT1(0,"\tList type '%-4s'\n", &fccType);

      switch(fccType) {
      case listtypeAVIMOVIE:

        if (dwLength < 8) {
          i64ChunkMoviPos = m_pInput->getFilePointer();
          dwChunkMoviLength = 0xFFFFFFF0;
          dwLength = 0;
        } else {
          i64ChunkMoviPos = m_pInput->getFilePointer();
          dwChunkMoviLength = dwLength;
        }

        if (fAcceptIndexOnly)
          goto terminate_scan;

        break;
      case listtypeAVIRECORD:    // silently enter grouping blocks
      case listtypeAVIHEADER:    // silently enter header blocks
        dwLength = 0;
        break;
      case listtypeSTREAMHEADER:
        if (!_parseStreamHeader(streamlist, dwLength, bScanRequired))
          fAcceptIndexOnly = false;
        else {
          mxverb(3, "AVI: OpenDML hierarchical index detected on stream %d.\n", streams);
          hyperindexed = true;
        }

        ++streams;
        dwLength = 0;
        break;
      }

      break;

    case ckidAVINEWINDEX:  // idx1
      if (!hyperindexed) {
        index_found = _parseIndexBlock(streamlist, dwLength/16, i64ChunkMoviPos);
        dwLength &= 15;
      }
      break;

    case ckidAVIPADDING:  // JUNK
      break;

    case mmioFOURCC('s','e','g','m'):      // VirtualDub segment hint block
      delete pSegmentHint;
      if (!(pSegmentHint = new char[dwLength]))
        throw error_c("out of memory");

      if(m_pInput->read(pSegmentHint, dwLength) != dwLength)
        return;

      if (dwLength&1)
        m_pInput->setFilePointer2(1, seek_current);

      dwLength = 0;
      break;

    case listtypeAVIHEADER:
      if (!bMainAVIHeaderFound) {
        uint32 tc = std::min<uint32>(dwLength, sizeof avihdr);
        memset(&avihdr, 0, sizeof avihdr);
        if(m_pInput->read(&avihdr, tc) != tc)
          return;
        dwLength -= tc;
        bMainAVIHeaderFound = true;
      }
      break;
    }

    if (dwLength) {
      if (!m_pInput->setFilePointer2(dwLength + (dwLength&1), seek_current))
        break;
    }

    // Quit as soon as we see the index block.

    if (fccType == ckidAVINEWINDEX)
      break;
  }

  if (i64ChunkMoviPos == 0)
    throw error_c("This AVI file doesn't have a movie data block (movi)!");

terminate_scan:

  if (!hyperindexed && !index_found)
    bScanRequired = true;

  if (bScanRequired) {
    mxverb(3, "AVI: Index not found or damaged -- reconstructing via file scan.\n");

    // It's possible that we were in the middle of reading an index when an error
    // occurred, so we need to clear all of the indices for all streams.

    pasn = streamlist.AtHead();

    while((pasn_next = pasn->NextFromHead()) != 0) {
      pasn->index.clear();
      pasn = pasn_next;
    }

    // obtain length of file and limit scanning if so

    sint64 i64FileSize = m_pInput->get_size();

    uint32 dwLengthLeft = dwChunkMoviLength;

//     long short_length = (long)((dwChunkMoviLength + _LL(1023)) >> 10);
//     long long_length = (long)((i64FileSize - i64ChunkMoviPos + _LL(1023)) >> 10);

//     sint64 length = (hyperindexed || bAggressive) ? long_length : short_length;
//     ProgressDialog pd("AVI Import Filter", bAggressive ? "Reconstructing missing index block (aggressive mode)" : "Reconstructing missing index block", length, true);

//     pd.setValueFormat("%ldK of %ldK");

    fFakeIndex = true;

    m_pInput->setFilePointer2(i64ChunkMoviPos, seek_beginning);

    // For standard AVI files, stop as soon as the movi chunk is exhausted or
    // the end of the AVI file is hit.  For OpenDML files, continue as long as
    // valid chunks are found.

    bool bStopWhenLengthExhausted = !hyperindexed && !bAggressive;

    for(;;) {
//       pd.advance((long)((m_pInput->getFilePointer() - i64ChunkMoviPos)/1024));
//       pd.check();

      // Exit if we are out of movi chunk -- except for OpenDML files.

      if (!bStopWhenLengthExhausted && dwLengthLeft < 8)
        break;

      // Validate the FOURCC itself but avoid validating the size, since it
      // may be too large for the last LIST/movi.
      
      if (!_readChunkHeader(fccType, dwLength))
        break;

      bool bValid = isValidFOURCC(fccType) && ((m_pInput->getFilePointer() + dwLength) <= i64FileSize);

      // In aggressive mode, verify that a valid FOURCC follows this chunk.

      if (bAggressive) {
        sint64 current_pos = m_pInput->getFilePointer();
        sint64 rounded_length = (dwLength+_LL(1)) & ~_LL(1);

        if (current_pos + dwLength > i64FileSize)
          bValid = false;
        else if (current_pos + rounded_length <= i64FileSize-8) {
          uint32_le fccNext;
          uint32 dwLengthNext;

          m_pInput->setFilePointer2(current_pos + rounded_length, seek_beginning);
          if (!_readChunkHeader(fccNext, dwLengthNext))
            break;

          bValid &= isValidFOURCC(fccNext) && ((m_pInput->getFilePointer() + dwLengthNext) <= i64FileSize);

          m_pInput->setFilePointer2(current_pos, seek_beginning);
        }
      }
      
      if (!bValid) {
        // Notify the user that recovering this file requires stronger measures.

        if (!bAggressive) {
          sint64 bad_pos = m_pInput->getFilePointer();
          mxverb(3, "AVI: Invalid chunk detected at %lld. Enabling aggressive recovery mode.\n", bad_pos);

          bAggressive = true;
          bStopWhenLengthExhausted = false;
//           pd.setCaption("Reconstructing missing index block (aggressive mode)");
//           pd.setLimit(long_length);
        }

        // Backup by seven bytes and continue.

        m_pInput->setFilePointer2(m_pInput->getFilePointer()-7, seek_beginning);
        continue;
      }

      int stream;
      
//      _RPT2(0,"(stream header) Chunk '%-4s', length %08lx\n", &fccType, dwLength);

      dwLengthLeft -= 8+(dwLength + (dwLength&1));

      // Skip over the chunk.  Don't skip over RIFF chunks of type AVIX, or
      // LIST chunks of type 'movi'.

      if (dwLength) {
        if (fccType == FOURCC_RIFF || fccType == FOURCC_LIST) {
          uint32_le fccType2;

          if (m_pInput->read(&fccType2, 4) != 4)
            break;

          if (fccType2 != formtypeAVIEXTENDED && fccType2 != listtypeAVIMOVIE) {
            if (!m_pInput->setFilePointer2(dwLength + (dwLength&1) - 4, seek_current))
              break;
          }
        } else {
          if (!m_pInput->setFilePointer2(dwLength + (dwLength&1), seek_current))
            break;
        }
      }

      if (m_pInput->getFilePointer() > i64FileSize)
        break;

      if (isxdigit(fccType&0xff) && isxdigit((fccType>>8)&0xff)) {
        stream = StreamFromFOURCC(fccType);

        if (stream >=0 && stream < streams) {

          pasn = streamlist.AtHead();

          while((pasn_next = pasn->NextFromHead()) && stream--)
            pasn = pasn_next;

          if (pasn && pasn_next) {

            // Set the keyframe flag for the first sample in the stream, or
            // if this is known to be a keyframe-only stream.  Do not set the
            // keyframe flag if the frame has zero bytes (drop frame).

            pasn->index.add(fccType, m_pInput->getFilePointer()-(dwLength + (dwLength&1))-8, dwLength, (!pasn->bytes || pasn->keyframe_only) && dwLength>0);
            pasn->bytes += dwLength;
          }
        }

      }
    }
  }

  mbFileIsDamaged |= bAggressive;

  // glue together indices

  pasn = streamlist.AtHead();

  int nStream = 0;
  while((pasn_next = pasn->NextFromHead()) != 0) {
    if (!pasn->index.makeIndex2())
      throw error_c("out of memory");

    pasn->frames = pasn->index.indexLen();

    // Clear sample size for video streams!

    if (pasn->hdr.fccType == streamtypeVIDEO)
      pasn->hdr.dwSampleSize=0;

    // Attempt to fix invalid dwRate/dwScale fractions (can result from unclosed
    // AVI files being written by DirectShow).

    if (pasn->hdr.dwRate==0 || pasn->hdr.dwScale == 0) {
      // If we're dealing with a video stream, try the frame rate in the AVI header.
      // If we're dealing with an audio stream, try the frame rate in the audio
      // format.
      // Otherwise, just use... uh, 15.

      if (pasn->hdr.fccType == streamtypeVIDEO) {
        if (bMainAVIHeaderFound) {
          pasn->hdr.dwRate  = avihdr.dwMicroSecPerFrame;    // This can be zero, in which case the default '15' will kick in below.
          pasn->hdr.dwScale  = 1000000;
        }
      } else if (pasn->hdr.fccType == streamtypeAUDIO) {
        const w32WAVEFORMATEX *pwfex = (const w32WAVEFORMATEX *)pasn->pFormat;

        pasn->hdr.dwRate  = pwfex->nAvgBytesPerSec;
        pasn->hdr.dwScale  = pwfex->nBlockAlign;
      }

      if (pasn->hdr.dwRate==0 || pasn->hdr.dwScale == 0) {
        pasn->hdr.dwRate = 15;
        pasn->hdr.dwScale = 1;
      }

      const int badstream = nStream;
      const double newrate = pasn->hdr.dwRate / (double)pasn->hdr.dwScale;
      mxverb(3, "AVI: Stream %d has an invalid sample rate. Substituting %lu samples/sec as placeholder.\n", badstream, newrate);
    }

    // Verify sample size == nBlockAlign for audio.  If we find runt samples,
    // assume someone did a VBR hack.

    if (pasn->hdr.fccType == streamtypeAUDIO) {
      const AVIIndexEntry2 *pIdx = pasn->index.index2Ptr();
      long nBlockAlign = ((w32WAVEFORMATEX *)pasn->pFormat)->nBlockAlign;

      pasn->hdr.dwSampleSize = nBlockAlign;

      for(int i=0; i<pasn->frames-1; ++i) {
        long s = pIdx[i].size & 0x7FFFFFFF;

        if (s && s < nBlockAlign) {
          pasn->FixVBRAudio();
          break;
        }
      }
    }

    if (pasn->hdr.dwSampleSize)
      pasn->length = (long)pasn->bytes / pasn->hdr.dwSampleSize;
    else
      pasn->length = pasn->frames;

    pasn = pasn_next;
    ++nStream;
  }

//  throw error_c("Parse complete.  Aborting.");
}

bool AVIReadHandler::_parseStreamHeader(List2<AVIStreamNode>& streamlist, uint32 dwLengthLeft, bool& bIndexDamaged) {
  AVIStreamNode *pasn;
  uint32_le fccType;
  uint32 dwLength;
  bool hyperindexed = false;
  
  if (!(pasn = new AVIStreamNode()))
    throw error_c("out of memory");

  try {
    while(dwLengthLeft >= 8 && _readChunkHeader(fccType, dwLength)) {

//      _RPT2(0,"(stream header) Chunk '%-4s', length %08lx\n", &fccType, dwLength);

      dwLengthLeft -= 8;

      if (dwLength > dwLengthLeft)
        throw error_c("Invalid AVI file: chunk size extends outside of parent");

      dwLengthLeft -= (dwLength + (dwLength&1));

      switch(fccType) {

      case ckidSTREAMHEADER:
        memset(&pasn->hdr, 0, sizeof pasn->hdr);

        if (dwLength < sizeof pasn->hdr) {
          if(m_pInput->read(&pasn->hdr, dwLength) != dwLength)
            throw error_c();
          if (dwLength & 1)
            m_pInput->setFilePointer2(1, seek_current);
        } else {
          if(m_pInput->read(&pasn->hdr, sizeof pasn->hdr) != sizeof pasn->hdr)
            throw error_c();
          m_pInput->setFilePointer2(dwLength+(dwLength&1) - sizeof pasn->hdr, seek_current);
        }
        dwLength = 0;

        pasn->keyframe_only = false;

        break;

      case ckidSTREAMFORMAT:
        if (!(pasn->pFormat = new char[pasn->lFormatLen = dwLength]))
          throw error_c("out of memory");

        if(m_pInput->read(pasn->pFormat, dwLength) != dwLength)
          throw error_c();

        if (pasn->hdr.fccType == streamtypeVIDEO) {
          switch(((w32BITMAPINFOHEADER *)pasn->pFormat)->biCompression) {
            case NULL:
            case bitmapFOURCC('R', 'A', 'W', ' '):
            case bitmapFOURCC('D', 'I', 'B', ' '):
            case bitmapFOURCC('d', 'm', 'b', '1'):
            case bitmapFOURCC('m', 'j', 'p', 'g'):
            case bitmapFOURCC('M', 'J', 'P', 'G'):
            case bitmapFOURCC('V', 'Y', 'U', 'Y'):
            case bitmapFOURCC('Y', 'U', 'Y', '2'):
            case bitmapFOURCC('U', 'Y', 'V', 'Y'):
            case bitmapFOURCC('Y', 'V', 'Y', 'U'):
            case bitmapFOURCC('Y', 'V', '1', '2'):
            case bitmapFOURCC('I', '4', '2', '0'):
            case bitmapFOURCC('Y', '4', '1', 'P'):
            case bitmapFOURCC('c', 'y', 'u', 'v'):
            case bitmapFOURCC('H', 'F', 'Y', 'U'):
            case bitmapFOURCC('b', 't', '2', '0'):
              pasn->keyframe_only = true;
          }
        }

        if (dwLength & 1)
          m_pInput->setFilePointer2(1, seek_current);
        dwLength = 0;
        break;

      case ckidOPENDMLINDEX:      // OpenDML extended index
        {
          sint64 posFileSave = m_pInput->getFilePointer();

          try {
            _parseExtendedIndexBlock(streamlist, pasn, -1, dwLength);
          } catch(const error_c&) {
            bIndexDamaged = true;
          }

          m_pInput->setFilePointer2(posFileSave, seek_beginning);
        }
        hyperindexed = true;
        break;

      case ckidAVIPADDING:  // JUNK
        break;
      }

      if (dwLength) {
        if (!m_pInput->setFilePointer2(dwLength + (dwLength&1), seek_current))
          break;
      }
    }

    if (dwLengthLeft)
      m_pInput->setFilePointer2(dwLengthLeft, seek_current);
  } catch(...) {
    delete pasn;
    throw;
  }

//  _RPT1(0,"Found stream: type %s\n", pasn->hdr.fccType==streamtypeVIDEO ? "video" : pasn->hdr.fccType==streamtypeAUDIO ? "audio" : "unknown");

  streamlist.AddTail(pasn);

  return hyperindexed;
}

bool AVIReadHandler::_parseIndexBlock(List2<AVIStreamNode>& streamlist, int count, sint64 movi_offset) {
  w32AVIINDEXENTRY avie[32];
  AVIStreamNode *pasn, *pasn_next;
  bool absolute_addr = true;

  // Some AVI files have relative addresses in their AVI index chunks, and some
  // relative.  They're supposed to be relative to the 'movi' chunk; all versions
  // of VirtualDub using fast write routines prior to build 4936 generate absolute
  // addresses (oops). AVIFile and ActiveMovie are both ambivalent.  I guess we'd
  // better be as well.

  while(count > 0) {
    int tc = count;
    int i;

    if (tc>32) tc=32;
    count -= tc;

    if (tc*sizeof(w32AVIINDEXENTRY) != m_pInput->read(avie, tc*sizeof(w32AVIINDEXENTRY))) {
      pasn = streamlist.AtHead();

      while((pasn_next = pasn->NextFromHead()) != 0) {
        pasn->index.clear();
        pasn->bytes = 0;

        pasn = pasn_next;
      }
      return false;
    }

    for(i=0; i<tc; i++) {
      int stream = StreamFromFOURCC(avie[i].ckid);

      if (absolute_addr && avie[i].dwChunkOffset<movi_offset)
        absolute_addr = false;

      pasn = streamlist.AtHead();

      while((pasn_next = (AVIStreamNode *)pasn->NextFromHead()) && stream--)
        pasn = pasn_next;

      if (pasn && pasn_next) {
        if (absolute_addr)
          pasn->index.add(&avie[i]);
        else
          pasn->index.add(avie[i].ckid, (movi_offset-4) + (sint64)avie[i].dwChunkOffset, avie[i].dwChunkLength, !!(avie[i].dwFlags & AVIIF_KEYFRAME));

        pasn->bytes += avie[i].dwChunkLength;
      }
    }
  }

  return true;

}

void AVIReadHandler::_parseExtendedIndexBlock(List2<AVIStreamNode>& streamlist, AVIStreamNode *pasn, sint64 fpos, uint32 dwLength) {
  union {
    AVISUPERINDEX idxsuper;
    AVISTDINDEX idxstd;
  };

  union {
    struct  _avisuperindex_entry    superent[64];
    uint32  dwHeap[256];
  };

  int entries, tp;
  int i;
  sint64 i64FPSave = m_pInput->getFilePointer();

  if (fpos>=0)
    m_pInput->setFilePointer2(fpos, seek_beginning);
  if(m_pInput->read((char *)&idxsuper + 8, sizeof(AVISUPERINDEX) - 8) != sizeof(AVISUPERINDEX) - 8)
    throw error_c();

  switch(idxsuper.bIndexType) {
  case AVI_INDEX_OF_INDEXES:
    // sanity check

    if (idxsuper.wLongsPerEntry != 4)
      throw error_c("Invalid superindex block in stream");

//    if (idxsuper.bIndexSubType != 0)
//      throw error_c("Field indexes not supported");

    entries = idxsuper.nEntriesInUse;

    while(entries > 0) {
      tp = sizeof superent / sizeof superent[0];
      if (tp>entries) tp=entries;

      if(m_pInput->read(superent, tp*sizeof superent[0]) != tp*sizeof superent[0])
        throw error_c();

      for(i=0; i<tp; i++)
        _parseExtendedIndexBlock(streamlist, pasn, superent[i].qwOffset+8, superent[i].dwSize-8);

      entries -= tp;
    }

    break;

  case AVI_INDEX_OF_CHUNKS:

//    if (idxstd.bIndexSubType != 0)
//      throw error_c("Frame indexes not supported");

    entries = idxstd.nEntriesInUse;

    // In theory, if bIndexSubType==AVI_INDEX_2FIELD it's supposed to have
    // wLongsPerEntry=3, and bIndexSubType==0 gives wLongsPerEntry=2.
    // Matrox's MPEG-2 stuff generates bIndexSubType=16 and wLongsPerEntry=6.
    // *sigh*
    //
    // For wLongsPerEntry==2 and ==3, dwOffset is at 0 and dwLength at 1;
    // for wLongsPerEntry==6, dwOffset is at 2 and all are keyframes.

    {
      if (idxstd.wLongsPerEntry!=2 && idxstd.wLongsPerEntry!=3 && idxstd.wLongsPerEntry!=6)
        throw error_c(string("Invalid OpenDML index block in stream, wLongsPerEntry=") + to_string(idxstd.wLongsPerEntry));

      while(entries > 0) {
        tp = (sizeof dwHeap / sizeof dwHeap[0]) / idxstd.wLongsPerEntry;
        if (tp>entries) tp=entries;

        if(m_pInput->read(dwHeap, tp*idxstd.wLongsPerEntry*sizeof(uint32)) != tp*idxstd.wLongsPerEntry*sizeof(uint32))
          throw error_c();

        if (idxstd.wLongsPerEntry == 6)
          for(i=0; i<tp; i++) {
            uint32 dwOffset = dwHeap[i*idxstd.wLongsPerEntry + 0];
            uint32 dwSize = dwHeap[i*idxstd.wLongsPerEntry + 2];

            pasn->index.add(idxstd.dwChunkId, (idxstd.qwBaseOffset+dwOffset)-8, dwSize, true);

            pasn->bytes += dwSize;
          }
        else
          for(i=0; i<tp; i++) {
            uint32 dwOffset = dwHeap[i*idxstd.wLongsPerEntry + 0];
            uint32 dwSize = dwHeap[i*idxstd.wLongsPerEntry + 1];

            pasn->index.add(idxstd.dwChunkId, (idxstd.qwBaseOffset+dwOffset)-8, dwSize&0x7FFFFFFF, !(dwSize&0x80000000));

            pasn->bytes += dwSize & 0x7FFFFFFF;
          }

        entries -= tp;
      }
    }

    break;

  default:
    throw error_c("Unknown hyperindex type");
  }

  m_pInput->setFilePointer2(i64FPSave, seek_beginning);
}

void AVIReadHandler::_destruct() {
  AVIStreamNode *pasn;
  //AVIFileDesc *pDesc;

  /*if (pAvisynthClipInfo)
    pAvisynthClipInfo->Release();

  if (paf)
    AVIFileRelease(paf);*/

  while((pasn = listStreams.RemoveTail()) != 0)
    delete pasn;

  delete streamBuffer;

  if(m_pUnbufferedInput && (m_pUnbufferedInput!=m_pInput))
    delete m_pUnbufferedInput;

  /*if (listFiles.IsEmpty()) {
    if (hFile != INVALID_HANDLE_VALUE)
      CloseHandle(hFile);
    if (hFileUnbuffered != INVALID_HANDLE_VALUE)
      CloseHandle(hFileUnbuffered);
  } else
    while(pDesc = listFiles.RemoveTail()) {
      CloseHandle(pDesc->hFile);
      CloseHandle(pDesc->hFileUnbuffered);
      delete pDesc;
    }*/

  delete pSegmentHint;
}

///////////////////////////////////////////////////////////////////////////

void AVIReadHandler::Release() {
  if (!--ref_count)
    delete this;
}

void AVIReadHandler::AddRef() {
  ++ref_count;
}

IAVIReadStream *AVIReadHandler::GetStream(uint32 fccType, sint32 lParam) {
  {
    AVIStreamNode *pasn, *pasn_next;
    int streamno = 0;

    pasn = listStreams.AtHead();

    while((pasn_next = pasn->NextFromHead()) != NULL) {
      if (pasn->hdr.fccType == fccType && !lParam--)
        break;

      pasn = pasn_next;
      ++streamno;
    }

    if (pasn_next) {
      return new AVIReadStream(this, pasn, streamno);
    }

    return NULL;
  }
}

void AVIReadHandler::EnableFastIO(bool f) {
  fDisableFastIO = !f;
}

bool AVIReadHandler::isOptimizedForRealtime() {
  return nRealTime!=0;
}

bool AVIReadHandler::isStreaming() {
  return nActiveStreamers!=0 && !mbFileIsDamaged;
}

bool AVIReadHandler::isIndexFabricated() {
  return fFakeIndex;
}

#if 0
bool AVIReadHandler::getSegmentHint(const char **ppszPath) {
  if (!pSegmentHint) {
    if (ppszPath)
      *ppszPath = NULL;

    return false;
  }

  if (ppszPath)
    *ppszPath = pSegmentHint+1;

  return !!pSegmentHint[0];
}
#endif

///////////////////////////////////////////////////////////////////////////

void AVIReadHandler::EnableStreaming(int stream) {
  if (!fStreamsActive) {
    if (!(streamBuffer = new char[STREAM_SIZE]))
      throw error_c("out of memory");

    i64StreamPosition = -1;
    sbPosition = sbSize = 0;
  }

  fStreamsActive |= (1<<stream);
  ++nActiveStreamers;
}

void AVIReadHandler::DisableStreaming(int stream) {
  fStreamsActive &= ~(1<<stream);

  if (!fStreamsActive) {
    delete streamBuffer;
    streamBuffer = NULL;
  }
  --nActiveStreamers;
}

void AVIReadHandler::AdjustRealTime(bool fInc) {
  if (fInc)
    ++nRealTime;
  else
    --nRealTime;
}

char *AVIReadHandler::_StreamRead(long& bytes) {
  if (nCurrentFile<0 || nCurrentFile != (int)(i64StreamPosition>>48))
    _SelectFile((int)(i64StreamPosition>>48));

  if (sbPosition >= sbSize) {
    if (nRealTime || (((i64StreamPosition&_LL(0x0000FFFFFFFFFFFF))+sbSize) & -STREAM_BLOCK_SIZE)+STREAM_SIZE > i64Size) {
      i64StreamPosition += sbSize;
      sbPosition = 0;
      m_pInput->setFilePointer2(i64StreamPosition & _LL(0x0000FFFFFFFFFFFF), seek_beginning);

      sbSize = m_pInput->read(streamBuffer, STREAM_RT_SIZE);

      if (sbSize < 0) {
        sbSize = 0;
// #pragma message(__TODO__ "Throw ?")
        //throw MyWin32Error("Failure streaming AVI file: %%s.",GetLastError());
      }
    } else {
      i64StreamPosition += sbSize;
      sbPosition = i64StreamPosition & (STREAM_BLOCK_SIZE-1);
      i64StreamPosition &= -STREAM_BLOCK_SIZE;
// #pragma message(__TODO__ "What to do with the unbuffered part ?")
      m_pUnbufferedInput->setFilePointer(i64StreamPosition & _LL(0x0000FFFFFFFFFFFF), seek_beginning);

      sbSize = m_pUnbufferedInput->read(streamBuffer, STREAM_SIZE);

      if (sbSize < 0) {
        sbSize = 0;
// #pragma message(__TODO__ "Throw ?")
        //throw MyWin32Error("Failure streaming AVI file: %%s.",GetLastError());
      }
    }
  }

  if (sbPosition >= sbSize)
    return NULL;

  if (bytes > sbSize - sbPosition)
    bytes = sbSize - sbPosition;

  sbPosition += bytes;

  return streamBuffer + sbPosition - bytes;
}

bool AVIReadHandler::Stream(AVIStreamNode *pusher, sint64 pos) {

  // Do not stream aggressively recovered files.

  if (mbFileIsDamaged)
    return false;

  bool read_something = false;

  if (!streamBuffer)
    return false;

  if (i64StreamPosition == -1) {
    i64StreamPosition = pos;
    sbPosition = 0;
  }

  if (pos < i64StreamPosition+sbPosition)
    return false;

  // >4Mb past current position!?

  if (pos > i64StreamPosition+sbPosition+4194304) {
//    OutputDebugString("Resetting streaming position!\n");
    i64StreamPosition = pos;
    sbSize = sbPosition = 0;
  }

/*  if (pusher->hdr.fccType == streamtypeVIDEO)
    OutputDebugString("pushed by video\n");
  else
    OutputDebugString("pushed by audio\n");*/

  ++pusher->stream_pushes;
  pusher->stream_push_pos = pos;

  while(pos >= i64StreamPosition+sbPosition) {
    long actual, left;
    char *src;
    uint32_le hdr[2];
    int stream;

    // read next header

    left = 8;
    while(left > 0) {
      actual = left;
      src = _StreamRead(actual);

      if (!src)
        return read_something;

      memcpy((char *)hdr + (8-left), src, actual);
      left -= actual;
    }

    stream = StreamFromFOURCC(hdr[0]);

    if (isxdigit(hdr[0]&0xff) && isxdigit((hdr[0]>>8)&0xff) && stream<32 &&
      ((1L<<stream) & fStreamsActive)) {

//      _RPT3(0,"\tstream: reading chunk at %I64x, length %6ld, stream %ld\n", i64StreamPosition+sbPosition-8, hdr[1], stream);

      AVIStreamNode *pasn, *pasn_next;
      int streamno = 0;

      pasn = listStreams.AtHead();

      while((pasn_next = pasn->NextFromHead()) != NULL) {
        if (streamno == stream) {
          unsigned chunk_size = hdr[1] + (hdr[1]&1);

          if (chunk_size >= 0x7ffffff0) {
            // Uh oh... assume the file has been damaged.  Disable streaming.
            sint64 bad_pos = i64StreamPosition+sbPosition-8;

            mxverb(3, "AVI: Invalid block found at %lld -- disabling streaming.\n", bad_pos);
            mbFileIsDamaged = true;
            i64StreamPosition = -1;
            sbPosition = sbSize = 0;
            return false;
          }

          long left = chunk_size;
          bool fWrite = pasn->cache->WriteBegin(i64StreamPosition + sbPosition, left);
          char *dst;

          while(left > 0) {
            actual = left;

            dst = _StreamRead(actual);
            
            if (!dst) {
              if (fWrite)
                pasn->cache->WriteEnd();
              return read_something;
            }

            if (fWrite)
              pasn->cache->Write(dst, actual);

            left -= actual;
          }

          if (fWrite)
            pasn->cache->WriteEnd();

          read_something = true;

          break;
        }

        pasn = pasn_next;
        ++streamno;
      }
    } else {

      if (hdr[0] != FOURCC_LIST && hdr[0] != FOURCC_RIFF) {
        long actual;

        // skip chunk

        unsigned chunk_size = hdr[1] + (hdr[1] & 1);

        if (chunk_size >= 0x7ffffff0) {
          mbFileIsDamaged = true;
          i64StreamPosition = -1;
          sbPosition = sbSize = 0;
          return false;
        }

        // Determine if the chunk is overly large.  If the chunk is too large, don't
        // stream through it.

        if (chunk_size > 262144) {
          // Force resynchronization on next read.
          i64StreamPosition += chunk_size;
          sbPosition = sbSize = 0;
          return read_something;
        }

        left = chunk_size;

        while(left > 0) {
          actual = left;

          if (!_StreamRead(actual))
            return read_something;

          left -= actual;
        }
      } else {
        left = 4;

        while(left > 0) {
          actual = left;

          if (!_StreamRead(actual))
            return read_something;

          left -= actual;
        }
      }

    }
  }

  return true;
}

sint64 AVIReadHandler::getStreamPtr() {
  return i64StreamPosition + sbPosition;
}

void AVIReadHandler::FixCacheProblems(AVIReadStream *arse) {
  AVIStreamNode *pasn, *pasn_next;

  // The simplest fix is simply to disable caching on the stream that's
  // cache-missing.  However, this is a bad idea if the problem is a low-bandwidth
  // audio stream that's outrunning a high-bandwidth video stream behind it.
  // Check the streaming leader, and if the streaming leader is comparatively
  // low bandwidth and running at least 512K ahead of the cache-missing stream,
  // disable its cache.

  AVIStreamNode *stream_leader = NULL;
  int stream_leader_no;
  int streamno=0;

  pasn = listStreams.AtHead();

  while((pasn_next = pasn->NextFromHead()) != NULL) {
    if (pasn->cache)
      if (!stream_leader || pasn->stream_pushes > stream_leader->stream_pushes) {
        stream_leader = pasn;
        stream_leader_no = streamno;
      }

    pasn = pasn_next;
    ++streamno;
  }

  if (stream_leader && stream_leader->stream_bytes*2 < arse->psnData->stream_bytes
    && stream_leader->stream_push_pos >= arse->psnData->stream_push_pos+524288) {
#ifdef STREAMING_DEBUG
    OutputDebugString("caching disabled on fast puny leader\n");
#endif
    delete stream_leader->cache;
    stream_leader->cache = NULL;

    DisableStreaming(stream_leader_no);

    i64StreamPosition = -1;
    sbPosition = sbSize = 0;
  } else {
#ifdef STREAMING_DEBUG
    OutputDebugString("disabling caching at request of client\n");
#endif

    arse->EndStreaming();

    if (arse->psnData == stream_leader) {
      i64StreamPosition = -1;
      sbPosition = sbSize = 0;
    }
  }

  // Reset cache statistics on all caches.

  pasn = listStreams.AtHead();

  while((pasn_next = pasn->NextFromHead()) != 0) {
    if (pasn->cache)
      pasn->cache->ResetStatistics();

    pasn = pasn_next;
  }
}

long AVIReadHandler::ReadData(int stream, void *buffer, sint64 position, long len) {
  if (nCurrentFile<0 || nCurrentFile != (int)(position>>48))
    _SelectFile((int)(position>>48));

//  _RPT3(0,"Reading from file %d, position %I64x, size %d\n", nCurrentFile, position, len);

  if (!m_pInput->setFilePointer2(position & _LL(0x0000FFFFFFFFFFFF), seek_beginning))
    return -1;
  return m_pInput->read(buffer, len);
}

bool AVIReadHandler::_readChunkHeader(uint32_le& pfcc, uint32& pdwLen) {
  uint32_le dw[2];
  sint32 actual;

  actual = m_pInput->read(dw, 8);

  if(actual != 8)
    return false;

  pfcc = dw[0];
  pdwLen = dw[1];

  return true;
}

void AVIReadHandler::_SelectFile(int file) {
  //AVIFileDesc *pDesc, *pDesc_next;

  nCurrentFile = file;

  /*pDesc = listFiles.AtHead();
  while((pDesc_next = pDesc->NextFromHead()) && file--)
    pDesc = pDesc_next;

  hFile      = pDesc->hFile;
  hFileUnbuffered  = pDesc->hFileUnbuffered;
  i64Size      = pDesc->i64Size;*/
}
