/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_vorbis.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_vorbis.cpp,v 1.1 2003/03/03 18:00:30 mosu Exp $
    \brief Vorbis packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include "config.h"

#ifdef HAVE_OGGVORBIS

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common.h"
#include "pr_generic.h"
#include "p_vorbis.h"
// #include "vorbis_header_utils.h"

#include "KaxTracks.h"
#include "KaxTrackAudio.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

vorbis_packetizer_c::vorbis_packetizer_c(audio_sync_t *nasync, range_t *nrange,
                                         void *d_header, int l_header,
                                         void *d_comments, int l_comments,
                                         void *d_codecsetup, int l_codecsetup)
  throw (error_c) : q_c() {
  int i;

  packetno = 0;
  old_granulepos = 0;
  memcpy(&async, nasync, sizeof(audio_sync_t));
  memcpy(&range, nrange, sizeof(range_t));
  last_granulepos = 0;
  last_granulepos_seen = 0;
  memset(headers, 0, 3 * sizeof(ogg_packet));
  headers[0].packet = (unsigned char *)malloc(l_header);
  headers[1].packet = (unsigned char *)malloc(l_comments);
  headers[2].packet = (unsigned char *)malloc(l_codecsetup);
  if ((headers[0].packet == NULL) || (headers[1].packet == NULL) ||
      (headers[2].packet == NULL))
    die("malloc");
  memcpy(headers[0].packet, d_header, l_header);
  memcpy(headers[1].packet, d_comments, l_comments);
  memcpy(headers[2].packet, d_codecsetup, l_codecsetup);
  headers[0].bytes = l_header;
  headers[1].bytes = l_comments;
  headers[2].bytes = l_codecsetup;
  headers[0].b_o_s = 1;
  headers[1].packetno = 1;
  headers[2].packetno = 2;
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);
  for (i = 0; i < 3; i++)
    if (vorbis_synthesis_headerin(&vi, &vc, &headers[i]) < 0)
      throw error_c("Error: vorbis_packetizer: Could not extract the "
                    "stream's parameters from the first packets.\n");
  set_header();
}

vorbis_packetizer_c::~vorbis_packetizer_c() {
  int i;

  for (i = 0; i < 3; i++)
    if (headers[i].packet != NULL)
      free(headers[i].packet);
}

int vorbis_packetizer_c::encode_silence(int fd) {
//   vorbis_dsp_state   lvds;
//   vorbis_block       lvb;
//   vorbis_info        lvi;
//   vorbis_comment     lvc;
//   ogg_stream_state   loss;
//   ogg_page           log;
//   ogg_packet         lop;
//   ogg_packet         h_main, h_comments, h_codebook;
//   int                samples, i, j, k, eos;
//   float            **buffer;
  
//   samples = vi.rate * async.displacement / 1000;
//   vorbis_info_init(&lvi);
//   if (vorbis_encode_setup_vbr(&lvi, vi.channels, vi.rate, 1))
//     return EOTHER;
//   vorbis_encode_setup_init(&lvi);
//   vorbis_analysis_init(&lvds, &lvi);
//   vorbis_block_init(&lvds, &lvb);
//   vorbis_comment_init(&lvc);
//   lvc.vendor = strdup(VERSIONINFO);
//   lvc.user_comments = (char **)mmalloc(4);
//   lvc.comment_lengths = (int *)mmalloc(4);
//   lvc.comments = 0;
//   ogg_stream_init(&loss, serialno);
//   vorbis_analysis_headerout(&lvds, &lvc, &h_main, &h_comments, &h_codebook);
//   ogg_stream_packetin(&loss, &h_main);
//   ogg_stream_packetin(&loss, &h_comments);
//   ogg_stream_packetin(&loss, &h_codebook);
//   while (ogg_stream_flush(&loss, &log)) {
//     write(fd, log.header, log.header_len);
//     write(fd, log.body, log.body_len);
//   }
//   eos = 0;
//   for (i = 0; i <= 1; i++) {
//     if (i == 0) {
//       buffer = vorbis_analysis_buffer(&lvds, samples);
//       for (j = 0; j < samples; j++)
//         for (k = 0; k < vi.channels; k++)
//           buffer[k][j] = 0.0f;
//       vorbis_analysis_wrote(&lvds, samples);
//     } else
//       vorbis_analysis_wrote(&lvds, 0);
//     while (vorbis_analysis_blockout(&lvds, &lvb) == 1) {
//       vorbis_analysis(&lvb, NULL);
//       vorbis_bitrate_addblock(&lvb);
//       while (vorbis_bitrate_flushpacket(&lvds, &lop)) {
//         ogg_stream_packetin(&loss, &lop);
//         while (!eos) {
//           if (!ogg_stream_pageout(&loss, &log))
//             break;
//           write(fd, log.header, log.header_len);
//           write(fd, log.body, log.body_len);
//           if (ogg_page_eos(&log))
//             eos = 1;
//         }
//       }
//     }
//   }

//   ogg_stream_clear(&loss);
//   vorbis_block_clear(&lvb);
//   vorbis_dsp_clear(&lvds);
//   vorbis_info_clear(&lvi);
//   vorbis_comment_clear(&lvc);
  
  return 0;
}

void vorbis_packetizer_c::setup_displacement() {
//   char tmpname[30];
//   int fd, old_verbose, status;
//   ogm_reader_c *ogm_reader;
//   audio_sync_t nosync;
//   range_t norange;
//   generic_packetizer_c *old_packetizer;
//   ogmmerge_page_t *mpage;
  
//   if (async.displacement <= 0)
//     return;
    
//   strcpy(tmpname, "/tmp/ogmmerge-XXXXXX");
//   if ((fd = mkstemp(tmpname)) == -1) {
//     fprintf(stderr, "FATAL: vorbis_packetizer: mkstemp() failed.\n");
//     exit(1);
//   }
//   if (encode_silence(fd) < 0) {
//     fprintf(stderr, "FATAL: Could not encode silence.\n");
//     exit(1);
//   }
//   close(fd);  

//   old_verbose = verbose;
//   memset(&norange, 0, sizeof(range_t));
//   verbose = 0;
//   nosync.displacement = 0;
//   nosync.linear = 1.0;
//   try {
//     ogm_reader = new ogm_reader_c(tmpname, NULL, NULL, NULL, &nosync,
//                                   &norange, NULL, NULL);
//   } catch (error_c error) {
//     fprintf(stderr, "FATAL: vorbis_packetizer: Could not create an " \
//             "ogm_reader for the temporary file.\n%s\n", error.get_error());
//     exit(1);
//   }
//   ogm_reader->overwrite_eos(1);
//   memcpy(&nosync, &async, sizeof(audio_sync_t));
//   async.displacement = 0;
//   async.linear = 1.0;
//   status = ogm_reader->read();
//   mpage = ogm_reader->get_header_page();
//   free(mpage->og->header);
//   free(mpage->og->body);
//   free(mpage->og);
//   free(mpage);
//   skip_packets = 2;
//   old_packetizer = ogm_reader->set_packetizer(this);
//   while (status == EMOREDATA)
//     status = ogm_reader->read();
//   ogm_reader->set_packetizer(old_packetizer);
//   delete ogm_reader;
//   verbose = old_verbose;
//   unlink(tmpname);
//   memcpy(&async, &nosync, sizeof(audio_sync_t));
}

#define AVORBIS "A_VORBIS"

void vorbis_packetizer_c::set_header() {
  using namespace LIBMATROSKA_NAMESPACE;
  
  if (kax_last_entry == NULL)
    track_entry =
      &GetChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks));
  else
    track_entry =
      &GetNextChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks),
        static_cast<KaxTrackEntry &>(*kax_last_entry));
  kax_last_entry = track_entry;

  if (serialno == -1)
    serialno = track_number++;
  KaxTrackNumber &tnumber =
    GetChild<KaxTrackNumber>(static_cast<KaxTrackEntry &>(*track_entry));
  *(static_cast<EbmlUInteger *>(&tnumber)) = serialno;
  
  *(static_cast<EbmlUInteger *>
    (&GetChild<KaxTrackType>(static_cast<KaxTrackEntry &>(*track_entry)))) =
      track_audio;

  KaxCodecID &codec_id =
    GetChild<KaxCodecID>(static_cast<KaxTrackEntry &>(*track_entry));
  codec_id.CopyBuffer((binary *)AVORBIS, countof(AVORBIS));

  KaxTrackAudio &track_audio =
    GetChild<KaxTrackAudio>(static_cast<KaxTrackEntry &>(*track_entry));

  KaxAudioSamplingFreq &kax_freq = GetChild<KaxAudioSamplingFreq>(track_audio);
  *(static_cast<EbmlFloat *>(&kax_freq)) = (float)vi.rate;
  
  KaxAudioChannels &kax_chans = GetChild<KaxAudioChannels>(track_audio);
  *(static_cast<EbmlUInteger *>(&kax_chans)) = vi.channels;
}

/* 
 * Some notes - processing is straight-forward if no AV synchronization
 * is needed - the packet is simply handed over to ogg_stream_packetin.
 * Unfortunately things are not that easy if AV sync is done. For a
 * negative displacement packets are simply discarded if their granulepos
 * is set before the displacement. For positive displacements the packetizer
 * has to generate silence and insert this silence just after the first
 * three stream packets - the Vorbis header, the comment and the
 * setup packets.
 * The creation of the silence is done by encoding 0 samples with 
 * libvorbis. This produces an OGG file that is read by a separate
 * ogm_reader_c. We set this very packetizer as the reader's packetizer
 * so that we get the packets we want from the 'silenced' file. This means
 * skipping the very first three packets, hence the 'skip_packets' variable.
 */
int vorbis_packetizer_c::process(char *data, int size, int64_t timecode) {
  ogg_packet op;
  
  /*
   * We might want to skip a certain amount of packets if this packetizer
   * is used by a second ogm_reader_c, one that reads from a 'silence'
   * file. skip_packets is then intially set to 3 in order to skip all
   * header packets.
   */
//   if (skip_packets > 0) {
//     skip_packets--;
//     return EMOREDATA;
//   }

  op.packet = (unsigned char *)data;
  op.bytes = size;
  fprintf(stdout, "packet_bs: %ld\n", vorbis_packet_blocksize(&vi, &op));

  // Recalculate the timecode if needed.
  if (timecode == -1)
    die("timecode = -1");

  // Handle the displacement.
  timecode += async.displacement;

  // Handle the linear sync - simply multiply with the given factor.
  timecode = (int64_t)((double)timecode * async.linear);

//   // range checking
//   if ((timecode >= range.start) &&
//       (last_granulepos_seen >= range.start) &&
//            ((range.end == 0) || (op->granulepos <= range.end))) {
//     // Adjust the granulepos
//     op->granulepos = (u_int64_t)(op->granulepos - range.start);
//     // If no or a positive displacement is set the packet has to be output.
//     if (async.displacement >= 0) {
//       ogg_stream_packetin(&os, op);
//       packetno++;
//     }
//     /*
//      * Only output the current packet if its granulepos ( = position in 
//      * samples) is bigger than the displacement.
//      */
//     else if (op->granulepos > 0) {
//       ogg_stream_packetin(&os, op);
//       packetno++;
//     }
//     queue_pages();
//     last_granulepos = op->granulepos;
//   } else if (op->e_o_s) {

  add_packet(data, size, (u_int64_t)timecode);

  return EMOREDATA;
}
 
#endif // HAVE_OGGVORBIS
