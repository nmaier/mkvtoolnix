/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Decoder header -
 *
 *  This program is an implementation of a part of one or more MPEG-4
 *  Video tools as specified in ISO/IEC 14496-2 standard.  Those intending
 *  to use this software module in hardware or software products are
 *  advised that its use may infringe existing patents or copyrights, and
 *  any such use would be at such party's own risk.  The original
 *  developer of this software module and his/her company, and subsequent
 *  editors and their companies, will have no liability for use of this
 *  software or modifications or derivatives thereof.
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
 *  $Id$
 *
 ****************************************************************************/
#ifndef __MPEG4PARSER_H
#define __MPEG4PARSER_H

#include "os.h"

/* Error codes */
#define XVID_ERR_FAIL    -1
#define XVID_ERR_OK       0
#define	XVID_ERR_MEMORY   1

/*****************************************************************************
 * Decoder structures
 ****************************************************************************/

typedef struct {
	void *handle;
} XVID_DEC_PARAM;


#define XVID_DEC_VOP     0
#define XVID_DEC_VOL     1
#define XVID_DEC_NOTHING 2 /* nothing was decoded */


typedef struct {
	void *bitstream;
	int length;
} XVID_PARSER_INPUT;

typedef struct {
  int x;
  int y;
} VECTOR;

typedef struct {
  VECTOR duv[3];
} WARPPOINTS;

typedef struct {
  uint32_t bufa;
  uint32_t bufb;
  uint32_t buf;
  uint32_t pos;
  uint32_t *tail;
  uint32_t *start;
  uint32_t length;
  uint32_t initpos;
} Bitstream;

#define MIN(X, Y) ((X)<(Y)?(X):(Y))

/* complexity estimation toggles */
typedef struct {
  int method;

  int opaque;
  int transparent;
  int intra_cae;
  int inter_cae;
  int no_update;
  int upsampling;

  int intra_blocks;
  int inter_blocks;
  int inter4v_blocks;
  int gmc_blocks;
  int not_coded_blocks;

  int dct_coefs;
  int dct_lines;
  int vlc_symbols;
  int vlc_bits;

  int apm;
  int npm;
  int interpolate_mc_q;
  int forw_back_mc_q;
  int halfpel2;
  int halfpel4;

  int sadct;
  int quarterpel;
} ESTIMATION;

typedef struct {
  // vol bitstream
  int time_inc_resolution;
  int fixed_time_inc;
  uint32_t time_inc_bits;

  uint32_t shape;
  uint32_t quant_bits;
  uint32_t quant_type;
  int complexity_estimation_disable;
  ESTIMATION estimation;

  int interlacing;
  uint32_t top_field_first;
  uint32_t alternate_vertical_scan;

  int aspect_ratio;
  int par_width;
  int par_height;

  int sprite_enable;
  int sprite_warping_points;
  int sprite_warping_accuracy;
  int sprite_brightness_change;

  int newpred_enable;
  int reduced_resolution_enable;

  // image

  int fixed_dimensions;
  uint32_t width;
  uint32_t height;
  uint32_t edged_width;
  uint32_t edged_height;

  int last_reduced_resolution;  // last reduced_resolution value
  int32_t frames;               // total frame number
  int32_t packed_mode;          // bframes packed bitstream? (1 = yes)
  int8_t scalability;
  int64_t time;                 // for record time
  int64_t time_base;
  int64_t last_time_base;
  int64_t last_non_b_time;
  uint32_t time_pp;
  uint32_t time_bp;
  uint32_t low_delay;           // low_delay flage (1 means no B_VOP)
  uint32_t low_delay_default;   // default value for low_delay flag
} DECODER;

#endif
