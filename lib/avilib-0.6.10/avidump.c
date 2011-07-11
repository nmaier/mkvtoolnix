/*
 *  avidump.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  based on code:
 *  (c)94 UP-Vision Computergrafik for c't
 *  Extracts some infos from RIFF files, modified by Gerd Knorr.
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

#include "common/os.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "xio.h"

#if defined(SYS_UNIX) || defined(COMP_MSC) || defined(SYS_APPLE)
#define lseek64 lseek
#endif

//#define AVI_DEBUG
#ifdef HAVE_ENDIAN_H
# include <endian.h>
#endif
#if defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
# define SWAP2(x) (((x>>8) & 0x00ff) |\
                   ((x<<8) & 0xff00))

# define SWAP4(x) (((x>>24) & 0x000000ff) |\
                   ((x>>8)  & 0x0000ff00) |\
                   ((x<<8)  & 0x00ff0000) |\
                   ((x<<24) & 0xff000000))
# define SWAP8(x) (((SWAP4(x)<<32) & 0xffffffff00000000ULL) |\
                   (SWAP4(x)))
#else
# define SWAP2(a) (a)
# define SWAP4(a) (a)
# define SWAP8(a) (a)
#endif

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef DWORD FOURCC;             /* Type of FOUR Character Codes */
typedef unsigned char boolean;
#define TRUE  1
#define FALSE 0
#define BUFSIZE 4096

#if !defined(STDOUT_FILENO)
#define STDOUT_FILENO stdout
#endif

/* Macro to convert expressions of form 'F','O','U','R' to
   numbers of type FOURCC: */

#undef MAKEFOURCC
#if defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
# define MAKEFOURCC(a,b,c,d) ((((DWORD)a)<<24) | (((DWORD)b)<<16) | \
                              (((DWORD)c)<< 8) | ( (DWORD)d)      )
#else
# define MAKEFOURCC(a,b,c,d) ( ((DWORD)a)      | (((DWORD)b)<< 8) | \
                              (((DWORD)c)<<16) | (((DWORD)d)<<24)  )
#endif

/* The only FOURCCs interpreted by this program: */

#define RIFFtag MAKEFOURCC('R','I','F','F')
#define LISTtag MAKEFOURCC('L','I','S','T')
#define avihtag MAKEFOURCC('a','v','i','h')
#define strhtag MAKEFOURCC('s','t','r','h')
#define strftag MAKEFOURCC('s','t','r','f')
#define vidstag MAKEFOURCC('v','i','d','s')
#define audstag MAKEFOURCC('a','u','d','s')
#define dmlhtag MAKEFOURCC('d','m','l','h')
#define idx1tag MAKEFOURCC('i','d','x','1')
#define Tag00db MAKEFOURCC('0','0','d','b')
#define Tag00dc MAKEFOURCC('0','0','d','c')
#define Tag01db MAKEFOURCC('0','1','d','b')
#define Tag01dc MAKEFOURCC('0','1','d','c')
#define Tag00wb MAKEFOURCC('0','0','w','b')
#define Tag01wb MAKEFOURCC('0','1','w','b')
#define Tagwave MAKEFOURCC('f','m','t',' ')
#define Tagdata MAKEFOURCC('d','a','t','a')
#define TagDATA MAKEFOURCC('D','A','T','A')
#define indxtag MAKEFOURCC('i','n','d','x')
#define Tag00__ MAKEFOURCC('0','0','_','_')
#define Tagix00 MAKEFOURCC('i','x','0','0')
#define Tagix01 MAKEFOURCC('i','x','0','1')
#define Tagix02 MAKEFOURCC('i','x','0','2')
#define Tagix03 MAKEFOURCC('i','x','0','3')

// Build a string from a FOURCC number (s must have room for at least 5 chars)

static void FOURCC2Str(FOURCC fcc, char* s)
{
#if defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
    s[0]=(fcc >> 24) & 0xFF;
    s[1]=(fcc >> 16) & 0xFF;
    s[2]=(fcc >>  8) & 0xFF;
    s[3]=(fcc      ) & 0xFF;
#else
    s[0]=(fcc      ) & 0xFF;
    s[1]=(fcc >>  8) & 0xFF;
    s[2]=(fcc >> 16) & 0xFF;
    s[3]=(fcc >> 24) & 0xFF;
#endif
    s[4]=0;
}

static DWORD fcc_type;

#define EoLST  0
#define INT32  1
#define INT16  2
#define FLAGS  3
#define CCODE  4
#define HEX16  5
#define _TAG   6
#define INT8   7
#define INT64  8

struct FLAGLIST {
    int  bit;
    char *name;
};

struct VAL {
    int  type;
    char *name;
    struct FLAGLIST *flags;
};

struct FLAGLIST flags_avih[] = {
	{ 0x00000010, "hasindex" },
	{ 0x00000020, "useindex" },
	{ 0x00000100, "interleaved" },
	{ 0x00010000, "for_capture" },
	{ 0x00020000, "copyrighted" },
	{ 0, NULL }
};

struct VAL names_avih[] = {
    { INT32,  "us_frame" },
    { INT32,  "max_bps" },
    { INT32,  "pad_gran" },
    { FLAGS,  "flags", flags_avih },
    { INT32,  "tot_frames" },
    { INT32,  "init_frames" },
    { INT32,  "streams" },
    { INT32,  "sug_bsize" },
    { INT32,  "width" },
    { INT32,  "height" },
    { INT32,  "scale" },
    { INT32,  "rate" },
    { INT32,  "start" },
    { INT32,  "length" },
    { EoLST,  NULL }
};

struct VAL names_strh[] = {
    { CCODE,  "fcc_handler" },
    { FLAGS,  "flags" },
    { INT32,  "priority" },
    { INT32,  "init_frames" },
    { INT32,  "scale" },
    { INT32,  "rate" },
    { INT32,  "start" },
    { INT32,  "length" },
    { INT32,  "sug_bsize" },
    { INT32,  "quality" },
    { INT32,  "samp_size" },
    { EoLST,  NULL }
};

struct VAL names_strf_vids[] = {
    { INT32,  "size" },
    { INT32,  "width" },
    { INT32,  "height" },
    { INT16,  "planes" },
    { INT16,  "bit_cnt" },
    { CCODE,  "compression" },
    { INT32,  "image_size" },
    { INT32,  "xpels_meter" },
    { INT32,  "ypels_meter" },
    { INT32,  "num_colors" },
    { INT32,  "imp_colors" },
    { EoLST,  NULL }
};

struct VAL names_strf_auds[] = {
    { HEX16,  "format" },
    { INT16,  "channels" },
    { INT32,  "rate" },
    { INT32,  "av_bps" },
    { INT16,  "blockalign" },
    { INT16,  "bits" },
    { INT16,  "cbSize" },
    { INT16,  "wID" },
    { INT32,  "fdwFlags" },
    { INT16,  "nBlockSize" },
    { INT16,  "nFramesPerBlock" },
    { INT16,  "nCodecDelay" },
    { EoLST,  NULL }    
};

struct VAL names_dmlh[] = {
    { INT32,  "frames" },
    { EoLST,  NULL }    
};

struct VAL names_idx1[] = {
    { _TAG,  "tag" },
    { INT32,  "flags" },
    { INT32,  "pos" },
    { INT32,  "length" },
    { EoLST,  NULL }    
};
struct VAL names_indx[] = {
    { INT16, "longs/entry"},
    { INT8, "index_sub_t" },
    { INT8, "index_t" },
    { INT32, "entries_used"},
    { CCODE, "fcc_handler"},
    { INT32, "reserved1"},
    { INT32, "reserved2"},
    { INT32, "reserved3"},
    { EoLST,  NULL }    
};
struct VAL names_stdidx[] = {
    { INT16, "longs/entry"},
    { INT8, "index_sub_t" },
    { INT8, "index_t" },
    { INT32, "entries_used"},
    { CCODE, "fcc_handler"},
    { INT64, "base_offset"},
    { INT32, "reserved3"},
    { EoLST,  NULL }    
};


#define MAX_BUF (4096)
static char buffer[MAX_BUF];

static size_t avi_read(int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while (r < len) {
      n = xio_read (fd, buf + r, len - r);

      if (n <= 0)
	  return r;
      r += n;
   }

   return r;
}

static size_t avi_write (int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while (r < len) {
      n = xio_write (fd, buf + r, len - r);
      if (n < 0)
         return n;
      
      r += n;
   }
   return r;
}

static size_t avi_read_write (int fd_in, int fd_out, size_t len)
{
  
  size_t w=len, z=0;
  
  while(w>0) {
  
    if(w>MAX_BUF) {
      z=avi_read(fd_in, buffer, MAX_BUF);
      if(z<0) return(z);
      z=avi_write(fd_out, buffer, MAX_BUF);
      if(z<0) return(z);
      w -=MAX_BUF;
    } else {
      z=avi_read(fd_in, buffer, w);
      if(z<0) return(z);
      z=avi_write(fd_out, buffer, w);
      if(z<0) return(z);
      w = 0;
    }
  } //data > 0

  return(len);
}


static void dump_vals(int fd, int count, struct VAL *names)
{
    DWORD i,j,val32;
    WORD  val16;
    off_t val64;
    char  val8;
    
    for (i = 0; names[i].type != EoLST; i++) {
	switch (names[i].type) {
	case INT8:
	    xio_read(fd, &val8, 1);
	    printf("\t%-12s = %d\n",names[i].name,val8);
	    break;
	case INT64:
	    xio_read(fd, &val64, 8);
	    val64 = SWAP8(val64);
	    printf("\t%-12s = 0x%016" PRIx64 "\n",names[i].name,(uint64_t)val64);
	    break;
	case INT32:
	    xio_read(fd, &val32, 4);
	    val32 = SWAP4(val32);
	    printf("\t%-12s = %ld\n",names[i].name,val32);
	    break;
	case CCODE:
	    xio_read(fd, &val32,4);
	    val32 = SWAP4(val32);
	    if (val32) {
		printf("\t%-12s = %c%c%c%c (0x%lx)\n",names[i].name,
		       (int)( val32        & 0xff),
		       (int)((val32 >>  8) & 0xff),
		       (int)((val32 >> 16) & 0xff),
		       (int)((val32 >> 24) & 0xff),
		       val32);
	    } else {
		printf("\t%-12s = unset (0)\n",names[i].name);
	    }
	    break;
	case _TAG:
	    xio_read(fd, &val32,4);
	    val32 = SWAP4(val32);
	    if (val32) {
		printf("\t%-12s = %c%c%c%c\n",names[i].name,
		       (int)( val32        & 0xff),
		       (int)((val32 >>  8) & 0xff),
		       (int)((val32 >> 16) & 0xff),
		       (int)((val32 >> 24) & 0xff));
	    } else {
		printf("\t%-12s = unset (0)\n",names[i].name);
	    }
	    break;
	case FLAGS:
	    xio_read(fd, &val32,4);
	    val32 = SWAP4(val32);
	    printf("\t%-12s = 0x%lx\n",names[i].name,val32);
	    if (names[i].flags) {
		for (j = 0; names[i].flags[j].bit != 0; j++)
		    if (names[i].flags[j].bit & val32)
			printf("\t\t0x%x: %s\n",
			       names[i].flags[j].bit,names[i].flags[j].name);
	    }
	    break;
	case INT16:
	    xio_read(fd, &val16,2);
	    val16 = SWAP2(val16);
	    printf("\t%-12s = %ld\n",names[i].name,(long)val16);
	    break;

	case HEX16:
	    xio_read(fd, &val16,2);
	    val16 = SWAP2(val16);
	    printf("\t%-12s = 0x%lx\n",names[i].name,(long)val16);
	    break;
	}
    }
}

#if 0
static void hexlog(unsigned char *buf, int count)
{
    int l,i;
    
    for (l = 0; l < count; l+= 16) {
	printf("\t  ");
	for (i = l; i < l+16; i++) {
	    if (i < count)
		printf("%02x ",buf[i]);
	    else
		printf("   ");
	    if ((i%4) == 3)
		printf(" ");
	}
	for (i = l; i < l+16; i++) {
	    if (i < count)
		printf("%c",isprint(buf[i]) ? buf[i] : '.');
	}
	printf("\n");
    }
}
#endif
static unsigned char*
off_t_to_char(off_t val, int base, int len)
{
    static const char digit[] = "0123456789abcdef";
    static char outbuf[32];
    char *p = outbuf + sizeof(outbuf);
    int i;

    *(--p) = 0;
    for (i = 0; i < len || val > 0; i++) {
	*(--p) = digit[ val % base ];
	val = val / base;
    }
    return (unsigned char *)p;
}    

/* Reads a chunk ID and the chunk's size from file f at actual
   file position : */

static boolean ReadChunkHead(int fd, FOURCC* ID, DWORD* size)
{
    if (!xio_read(fd, ID, sizeof(FOURCC))) return(FALSE);
    if (!xio_read(fd, size, sizeof(DWORD))) return(FALSE);

    *size = SWAP4(*size);

#ifdef AVI_DEBUG
    printf("ReadChunkHead size = %lu\n", *size);
#endif

    return(TRUE);
}

/* Processing of a chunk. (Will be called recursively!).
   Processes file f starting at position filepos.
   f contains filesize bytes.
   If DesiredTag!=0, ProcessChunk tests, whether the chunk begins
   with the DesiredTag. If the read chunk is not identical to
   DesiredTag, an error message is printed.
   RekDepth determines the recursion depth of the chunk.
   chunksize is set to the length of the chunk's data (excluding
   header and padding byte).
   ProcessChunk prints out information of the chunk to stdout
   and returns FALSE, if an error occured. */

static off_t datapos_tmp[8];

static boolean ProcessChunk(int fd, off_t filepos, off_t filesize,
			    FOURCC DesiredTag, int RekDepth,
			    DWORD* chunksize)
{
    char   tagstr[5];   /* FOURCC of chunk converted to string */
    FOURCC chunkid;     /* read FOURCC of chunk                */
    off_t  datapos;     /* position of data in file to process */
    
    if (filepos>filesize-1) {  /* Oops. Must be something wrong!      */
	printf("  *****  Error: Data would be behind end of file!\n");
    }
    
    xio_lseek(fd, filepos, SEEK_SET); /* Go to desired file position!     */

    if (!ReadChunkHead(fd, &chunkid, chunksize)) {  /* read chunk header */
	printf("  *****  Error reading chunk at filepos 0x%s\n",
	       off_t_to_char(filepos,16,1));
	return(FALSE);
    }
    FOURCC2Str(chunkid,tagstr);       /* now we can PRINT the chunkid */
    if (DesiredTag) {                 /* do we have to test identity? */
	if (DesiredTag!=chunkid) {
	    char ds[5];
	    FOURCC2Str(DesiredTag,ds);
	    printf("\n\n *** Error: Expected chunk '%s', found '%s'\n",
		   ds,tagstr);
	    return(FALSE);
	}
    }
    
    datapos=filepos+sizeof(FOURCC)+sizeof(DWORD); /* here is the data */

    /* print out header: */
#ifdef AVI_DEBUG
    printf("(%Lu) %*c  ID:<%s>   Size: %lu\n",
	    filepos ,(RekDepth+1)*4,' ',tagstr, *chunksize);
#else
    printf("(0x%s) %*c  ID:<%s>   Size: 0x%08lx %8lu\n",
	   off_t_to_char(filepos,16,8),(RekDepth+1)*4,' ',tagstr, *chunksize, *chunksize);
#endif


    if (datapos + ((*chunksize+1)&~1) > filesize) {      /* too long? */
	printf("  *****  Error: Chunk exceeds file\n");
    }
    
    switch (chunkid) {
	
  /* Depending on the ID of the chunk and the internal state, the
     different IDs can be interpreted. At the moment the only
     interpreted chunks are RIFF- and LIST-chunks. For all other
     chunks only their header is printed out. */

    case RIFFtag:
    case LISTtag: {
	
	DWORD  datashowed=0;
	FOURCC formtype;         /* format of chunk                     */
	char   formstr[5];       /* format of chunk converted to string */
	DWORD  subchunksize=0;   /* size of a read subchunk             */
	
	if (!read(fd, &formtype, sizeof(FOURCC))) printf("ERROR\n");     /* read the form type */
	FOURCC2Str(formtype,formstr);            /* make it printable  */
	
	
	/* print out the indented form of the chunk: */
	if (chunkid==RIFFtag)
	    printf("%12c %*c  Form Type = <%s>\n",
		   ' ',(RekDepth+1)*4,' ',formstr);
	else
	    printf("%12c %*c  List Type = <%s>\n",
		   ' ',(RekDepth+1)*4,' ',formstr);
	
	datashowed=sizeof(FOURCC);    /* we showed the form type      */
	datapos+=(off_t) datashowed;  /* for the rest of the routine  */
	
	while (datashowed<*chunksize) {      /* while not showed all: */
	    
	  long subchunklen;           /* complete size of a subchunk  */
	  
	  datapos_tmp[RekDepth]=datapos;

	    /* recurse for subchunks of RIFF and LIST chunks: */
#ifdef AVI_DEBUG
	    printf("enter [%d] size=%lu pos=%Lu left=%lu\n", RekDepth, subchunksize, datapos, *chunksize-datashowed); 
#endif
	    if (!ProcessChunk(fd, datapos, filesize,  0,
			      RekDepth+1, &subchunksize)) return(FALSE);
	    
	    subchunklen = sizeof(FOURCC) +  /* this is the complete..   */
		sizeof(DWORD)  +  /* .. size of the subchunk  */
		((subchunksize+1) & ~1);
	    
	    datashowed += subchunklen;      /* we showed the subchunk   */
	    datapos    = datapos_tmp[RekDepth] + (off_t) subchunklen;      /* for the rest of the loop */
	    
#ifdef AVI_DEBUG
	    printf(" exit [%d] size=%lu/%lu pos=%Lu left=%lu\n", RekDepth, subchunksize, subchunklen, datapos, *chunksize-datashowed); 
#endif
	}
    } break;
    
    /* Feel free to put your extensions here! */
    
    case avihtag:
	dump_vals(fd,sizeof(names_avih)/sizeof(struct VAL),names_avih);
	break;
    case strhtag:
    {
	char   typestr[5];
	xio_read(fd, &fcc_type,sizeof(FOURCC));
	FOURCC2Str(fcc_type,typestr);
	printf("\tfcc_type     = %s\n",typestr);
	dump_vals(fd,sizeof(names_strh)/sizeof(struct VAL),names_strh);
	break;
    }
    case strftag:

	switch (fcc_type) {
	case vidstag:
	    dump_vals(fd,sizeof(names_strf_vids)/sizeof(struct VAL),names_strf_vids);
	    break;
	case audstag:
	    dump_vals(fd,sizeof(names_strf_auds)/sizeof(char*),names_strf_auds);
	    break;
	default:
	    printf("unknown\n");
	    break;
	}
	break;

    case Tagwave:
	dump_vals(fd,sizeof(names_strf_auds)/sizeof(char*),names_strf_auds);
	break;

    case indxtag: {
	long chunks=*chunksize-sizeof(names_indx)/sizeof(char*);
	off_t offset;
	DWORD size, duration;
	long u=0;
	off_t indxend = datapos + chunks;
	dump_vals(fd,sizeof(names_indx)/sizeof(char*),names_indx);

	while (datapos < indxend) {
	    xio_read(fd, &offset,8);
	    xio_read(fd, &size, 4);
	    xio_read(fd, &duration,4);
	    offset = SWAP8(offset);
	    size = SWAP4(size);
	    duration = SWAP4(duration);
	    if (size!=0)
	    printf("\t\t [%6ld] 0x%016" PRIx64 " 0x%08lx %8d\n", u++, (uint64_t)offset, size,
		    (int)duration);
	    datapos += 16;
	}
	break;
	}
    case Tagix00:
    case Tagix01:
    case Tagix02:
    case Tagix03: {
	long chunks=*chunksize-sizeof(names_stdidx)/sizeof(char*);
	unsigned int offset, size, key;
	long u=0;
	off_t indxend = datapos + chunks;
	dump_vals(fd,sizeof(names_stdidx)/sizeof(char*),names_stdidx);

	while (datapos < indxend) {
	    xio_read(fd, &offset,4);
	    xio_read(fd, &size, 4);
	    offset = SWAP4(offset);
	    size = SWAP4(size);
	    key  = size;
	    key  = key&0x80000000?0:1;
	    size &= 0x7fffffff;
	    if (size!=0)
	    printf("\t\t [%6ld] 0x%08x 0x%08x key=%s\n", u++, offset, size,
		    key?"yes":"no");
	    datapos += 8;
	}
		  }
	break;

    case idx1tag: {
	off_t idxend = datapos+*chunksize;

	while (datapos<idxend) {

	    DWORD val32;
	    static long u=0;

	    //tag:
	    xio_read(fd, &val32,4);
	    val32 = SWAP4(val32);
	    if(val32==Tag00db) {
		printf("\t\t [%6ld] %s=%c%c%c%c ", u++, "tag",
		       (int)( val32        & 0xff),
		       (int)((val32 >>  8) & 0xff),
		       (int)((val32 >> 16) & 0xff),
		       (int)((val32 >> 24) & 0xff));
	    } else printf("\t\t          %s=%c%c%c%c ", "tag",
		       (int)( val32        & 0xff),
		       (int)((val32 >>  8) & 0xff),
		       (int)((val32 >> 16) & 0xff),
		       (int)((val32 >> 24) & 0xff));
	    
	    
	    //flag
	    xio_read(fd, &val32, 4);
	    val32 = SWAP4(val32);
	    printf("flags=%02ld ",val32);

	    //pos
	    xio_read(fd, &val32, 4);
	    val32 = SWAP4(val32);
	    printf("0x%08lx",val32);

	    //size
	    xio_read(fd, &val32, 4);
	    val32 = SWAP4(val32);
	    printf("%8ld\n",val32);

	    datapos+=16;
	}
	}
	break;

    case dmlhtag:
	dump_vals(fd,sizeof(names_dmlh)/sizeof(struct VAL),names_dmlh);
	break;
    }
    
    return(TRUE);
}

static DWORD size;
static int eos=0;

static boolean DumpChunk(int fd, off_t filepos, off_t filesize,
			 FOURCC DesiredTag, int RekDepth,
			 DWORD* chunksize, int mode)
{
    char   tagstr[5];          /* FOURCC of chunk converted to string */
    FOURCC chunkid;            /* read FOURCC of chunk                */
    off_t  datapos;            /* position of data in file to process */
    
    if (filepos>filesize-1)  /* Oops. Must be something wrong!      */
	return(FALSE);
    
    xio_lseek(fd, filepos, SEEK_SET);

    if (!ReadChunkHead(fd, &chunkid,chunksize)) {  /* read chunk header */
      return(FALSE);
    }
    
    FOURCC2Str(chunkid,tagstr);       /* now we can PRINT the chunkid */

    if (DesiredTag) {                 /* do we have to test identity? */
      if (DesiredTag!=chunkid) {
	char ds[5];
	FOURCC2Str(DesiredTag,ds);
	return(FALSE);
      }
    }
    
    datapos=filepos+sizeof(FOURCC)+sizeof(DWORD); /* here is the data */

    //support for broken files
    if (datapos + ((*chunksize+1)&~1) > filesize) {
      size = filesize-datapos;
      eos=1;
    } else { 
      size = *chunksize;
    }
    
    switch (chunkid) {
	
  /* Depending on the ID of the chunk and the internal state, the
     different IDs can be interpreted. At the moment the only
     interpreted chunks are RIFF- and LIST-chunks. For all other
     chunks only their header is printed out. */

    case RIFFtag:
    case LISTtag: {
	
      DWORD datashowed;
      FOURCC formtype;       /* format of chunk                     */
      char   formstr[5];     /* format of chunk converted to string */
      DWORD  subchunksize;   /* size of a read subchunk             */
      
      xio_read(fd, &formtype,sizeof(FOURCC));    /* read the form type */
      FOURCC2Str(formtype,formstr);           /* make it printable  */
      
      datashowed=sizeof(FOURCC);    /* we showed the form type      */
      datapos+=(off_t)datashowed;   /* for the rest of the routine  */
      
      while (datashowed<*chunksize) {      /* while not showed all: */
	
	long subchunklen;           /* complete size of a subchunk  */
	
	/* recurse for subchunks of RIFF and LIST chunks: */
	if (!DumpChunk(fd, datapos,filesize,0,
		       RekDepth+1,&subchunksize,mode)) return(FALSE);
	
	subchunklen = sizeof(FOURCC) +  /* this is the complete..   */
	  sizeof(DWORD)  +  /* .. size of the subchunk  */
	  ((subchunksize+1) & ~1);
	
	datashowed += subchunklen;      /* we showed the subchunk   */
	datapos    += (off_t) subchunklen;      /* for the rest of the loop */
      }
    } 

    break;
    
    case Tag01wb:
    case Tag00wb:

      if(mode==1) {
	xio_lseek(fd, datapos, SEEK_SET);
	if(avi_read_write(fd, STDOUT_FILENO, size)!=size) return(FALSE);
      }
      if(eos) return(FALSE);
      
      break;
      
    case Tag00db:
    case Tag00dc:
    case Tag01db:
    case Tag01dc:
    case Tag00__:

      if(mode==0) {
	xio_lseek(fd, datapos, SEEK_SET);
	if(avi_read_write(fd, STDOUT_FILENO, size)!=size) return(FALSE);
      }
    
      if(eos) return(FALSE);

      break;
      
    case Tagdata:
    case TagDATA:
      
      if(mode==2) {
	xio_lseek(fd, datapos, SEEK_SET);
	if(avi_read_write(fd, STDOUT_FILENO, size)!=size) return(FALSE);
      }
      
      if(eos) exit(1);
      
      break;
    }
    
    return(TRUE);
}


int AVI_scan(char *file_name)
{
    off_t  filesize;     /* its size                    */
    off_t  filepos;

    int fd;

    DWORD  chunksize;    /* size of the RIFF chunk data */

#if defined(SYS_WINDOWS)
    if (!(fd=open(file_name, O_RDONLY|O_BINARY))) {
#else
    if (!(fd=xio_open(file_name, O_RDONLY))) {
#endif
	printf("\n\n *** Error opening file %s. Program aborted!\n",
	       file_name);
	return(1);
    }

    filesize = xio_lseek(fd, 0, SEEK_END);
    xio_lseek(fd, 0, SEEK_SET);

    printf("Contents of file %s (%s/", file_name,
	   off_t_to_char(filesize,10,1));
    printf("0x%s bytes):\n\n",off_t_to_char(filesize,16,1));

    for (filepos = 0; filepos < filesize;) {
	chunksize = 0;
	if (!ProcessChunk(fd, filepos,filesize,RIFFtag,0,&chunksize))
	    break;
	filepos += chunksize + 8;
	printf("\n");
    }

    xio_close(fd);

    return(0);
}


int AVI_dump(char *file_name, int mode)
{
    off_t  filesize;     /* its size                    */
    off_t  filepos;
    int fd;

    DWORD  chunksize;    /* size of the RIFF chunk data */

#if defined(SYS_WINDOWS)
    if (!(fd=open(file_name,O_RDONLY|O_BINARY))) return(1);
#else
    if (!(fd=xio_open(file_name,O_RDONLY))) return(1);
#endif

    filesize = xio_lseek(fd, 0, SEEK_END);
    xio_lseek(fd, 0, SEEK_SET);

    for (filepos = 0; filepos < filesize;) {
	chunksize = 0;
	if (!DumpChunk(fd,filepos,filesize,RIFFtag,0,&chunksize,mode))
	    break;
	filepos += chunksize + 8;
    }

    xio_close(fd);

    return(0);
}
