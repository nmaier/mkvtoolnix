/*
 *  avimisc.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream processing tool
 *      
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include <stdio.h>
#include <stdlib.h>

//SLM
#ifdef WIN32
#else
#include <unistd.h>
#endif

#include <sys/stat.h>
#include <string.h>

#include "avilib.h"

void AVI_info(avi_t *avifile)
{

  long frames, rate, mp3rate, chunks, tot_bytes;

  int width, height, format, chan, bits;

  int j, tracks, tmp;
  
  double fps;
  
  char *codec;

  frames =  AVI_video_frames(avifile);
   width =  AVI_video_width(avifile);
  height =  AVI_video_height(avifile);

  fps    =  AVI_frame_rate(avifile);
  codec  =  AVI_video_compressor(avifile);

  printf("[avilib] V: %6.3f fps, codec=%s, frames=%ld, width=%d, height=%d\n",  fps, ((strlen(codec)==0)? "RGB": codec), frames, width, height);

  tracks=AVI_audio_tracks(avifile);

  tmp=AVI_get_audio_track(avifile);

  for(j=0; j<tracks; ++j) {
      
      AVI_set_audio_track(avifile, j);
      
      rate   =  AVI_audio_rate(avifile);
      format =  AVI_audio_format(avifile);
      chan   =  AVI_audio_channels(avifile);
      bits   =  AVI_audio_bits(avifile);
      mp3rate=  AVI_audio_mp3rate(avifile);

      chunks = AVI_audio_chunks(avifile);
      tot_bytes = AVI_audio_bytes(avifile);

      
      if(chan>0) {
	  printf("[avilib] A: %ld Hz, format=0x%02x, bits=%d, channels=%d, bitrate=%ld kbps,\n", rate, format, bits, chan, mp3rate); 
	  printf("[avilib]    %ld chunks, %ld bytes\n", chunks, tot_bytes);
      } else
	  printf("[avilib] A: no audio track found\n");
  }

  AVI_set_audio_track(avifile, tmp); //reset
  
}


int AVI_file_check(char *import_file)
{
    // check for sane video file
    struct stat fbuf;

    if(stat(import_file, &fbuf) || import_file==NULL){
        fprintf(stderr, "(%s) invalid input file \"%s\"\n", __FILE__, 
                import_file);
        return(1);
    }
    
    return(0);
}


char *AVI_codec2str(short cc)
{
    
    switch (cc) {
	
    case 0x1://PCM
	return("PCM");
	break;
    case 0x2://MS ADPCM
	return("MS ADPCM");
	break;
    case 0x11://IMA ADPCM
	printf("Audio in ADPCM format\n");
	break;
    case 0x31://MS GSM 6.10
    case 0x32://MSN Audio
	printf("Audio in MS GSM 6.10 format\n");
	break;
    case 0x50://MPEG Layer-1,2    
	return("MPEG Layer-1/2");
	break;
    case 0x55://MPEG Layer-3
	return("MPEG Layer-3");
	break;
    case 0x160:
    case 0x161://DivX audio
	return("DivX WMA");
	break;
    case 0x401://Intel Music Coder
	printf("Audio in IMC format\n");
	break;
    case 0x2000://AC3
	return("AC3");
	break;
    default:
	return("unknown");
    }	
    return("unknown");
}
