/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mkvmerge.cpp
  main program, command line parameter checking, looping, output handling

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on test6.cpp from the libmatroska, written by Steve Lhomme
  <robux4@users.sf.net> for the Matroska project
  http://www.matroska.org/

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: mkvmerge.cpp,v 1.2 2003/02/16 00:47:52 mosu Exp $
    \brief create matroska files from other media files, main file
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <iostream>

#include "StdIOCallback.h"

#include "EbmlHead.h"
#include "EbmlSubHead.h"
#include "FileKax.h"
#include "KaxSegment.h"
#include "KaxTracks.h"
#include "KaxTrackEntryData.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"
#include "KaxCluster.h"
#include "KaxClusterData.h"
#include "KaxBlock.h"
#include "KaxBlockAdditional.h"

using namespace LIBMATROSKA_NAMESPACE;

int main(int argc, char **argv) {
  char *file1;
  char *file2;
  uint64 fsize;
  
  if (argc < 2) {
    fprintf(stderr, "(%s) Input, output missing\n", __FILE__);
    exit(1);
  }
  file1 = argv[1];
  file2 = argv[2];

  try {
    // write the head of the file (with everything already configured)
    StdIOCallback out_file(file2, MODE_CREATE);

    ///// Writing EBML test
    EbmlHead FileHead;
    
    EDocType & MyDocType = GetChild<EDocType>(FileHead);
    *static_cast<EbmlString *>(&MyDocType) = "matroska";

    EDocTypeVersion & MyDocTypeVer = GetChild<EDocTypeVersion>(FileHead);
    *(static_cast<EbmlUInteger *>(&MyDocTypeVer)) = 1;

    EDocTypeReadVersion & MyDocTypeReadVer =
      GetChild<EDocTypeReadVersion>(FileHead);
    *(static_cast<EbmlUInteger *>(&MyDocTypeReadVer)) = 1;

    FileHead.Render(out_file);

    KaxSegment FileSegment;
    // size is unknown and will always be, we can render it right away
    FileSegment.Render(out_file);

    KaxTracks & MyTracks = GetChild<KaxTracks>(FileSegment);
    
    // fill track 1 params
    KaxTrackEntry & MyTrack1 = GetChild<KaxTrackEntry>(MyTracks);

    KaxTrackNumber & MyTrack1Number = GetChild<KaxTrackNumber>(MyTrack1);
    *(static_cast<EbmlUInteger *>(&MyTrack1Number)) = 1;

    *(static_cast<EbmlUInteger *>(&GetChild<KaxTrackType>(MyTrack1))) =
      track_audio;

    KaxCodecID & MyTrack1CodecID = GetChild<KaxCodecID>(MyTrack1);
    MyTrack1CodecID.CopyBuffer((binary *)"Dummy Audio Codec",
      countof("Dummy Audio Codec"));

    // audio specific params
    KaxTrackAudio & MyTrack1Audio = GetChild<KaxTrackAudio>(MyTrack1);
    
    KaxAudioSamplingFreq &MyTrack1Freq =
      GetChild<KaxAudioSamplingFreq>(MyTrack1Audio);
    *(static_cast<EbmlFloat *>(&MyTrack1Freq)) = 44100.0;
    MyTrack1Freq.ValidateSize();

    KaxAudioChannels & MyTrack1Channels =
      GetChild<KaxAudioChannels>(MyTrack1Audio);
    *(static_cast<EbmlUInteger *>(&MyTrack1Channels)) = 2;

    MyTracks.Render(out_file);

    // creation of the original binary/raw files
    StdIOCallback in_file(file1, MODE_READ);
    
    in_file.setFilePointer(0L, seek_end);
    
    fsize = (unsigned int)in_file.getFilePointer();
    
    binary *buf_in = new binary[fsize];

    // start muxing (2 binary frames for each text frame)
    in_file.setFilePointer(0L, seek_beginning);

    in_file.read(buf_in, fsize);

    KaxCluster cluster;
    KaxClusterTimecode & MyClusterTimecode =
      GetChild<KaxClusterTimecode>(cluster);
    *(static_cast<EbmlUInteger *>(&MyClusterTimecode)) = 0; // base timecode

    KaxBlockGroup & MyBlockG1 = GetChild<KaxBlockGroup>(cluster);
    KaxBlock & MyBlock1 = GetChild<KaxBlock>(MyBlockG1);

    DataBuffer data1 = DataBuffer((binary *)buf_in, fsize);
    MyBlock1.AddFrame(MyTrack1, 10, data1);

    cluster.Render(out_file);

    delete[] buf_in;

    out_file.close();
  } catch (std::exception & Ex) {
    std::cout << Ex.what() << std::endl;
  }

  return 0;
}
