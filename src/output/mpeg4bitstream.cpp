 /*****************************************************************************
  *                                                                           *
  *  This file is part of XviD, a free MPEG-4 video encoder/decoder           *
  *                                                                           *
  *  XviD is an implementation of a part of one or more MPEG-4 Video tools    *
  *  as specified in ISO/IEC 14496-2 standard.  Those intending to use this   *
  *  software module in hardware or software products are advised that its    *
  *  use may infringe existing patents or copyrights, and any such use        *
  *  would be at such party's own risk.  The original developer of this       *
  *  software module and his/her company, and subsequent editors and their    *
  *  companies, will have no liability for use of this software or            *
  *  modifications or derivatives thereof.                                    *
  *                                                                           *
  *  XviD is free software; you can redistribute it and/or modify it          *
  *  under the terms of the GNU General Public License as published by        *
  *  the Free Software Foundation; either version 2 of the License, or        *
  *  (at your option) any later version.                                      *
  *                                                                           *
  *  XviD is distributed in the hope that it will be useful, but              *
  *  WITHOUT ANY WARRANTY; without even the implied warranty of               *
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
  *  GNU General Public License for more details.                             *
  *                                                                           *
  *  You should have received a copy of the GNU General Public License        *
  *  along with this program; if not, write to the Free Software              *
  *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA *
  *                                                                           *
  ****************************************************************************/

 /****************************************************************************
  *                                                                          *
  *  derivered from bitstream.c and bitstram.h from XviD project             *
  *                                                                          *
  *  Copyright (C) 2001-2003 - Peter Ross <pross@cs.rmit.edu.au>             *
  *  Copyright (C) 2001-2003 - gruel                                         *
  *  Copyright (C) 2001-2003 - Isibaar                                       *
  *  Copyright (C) 2002 - MinChen <chenm001@163.com>                         *
  *                                                                          *
  *  For more information visit the XviD homepage: http://www.xvid.org       *
  *                                                                          *
  *  $Id$
  ****************************************************************************/

#include <string.h>
#include <stdio.h>

#include "common.h"
#include "mpeg4parser.h"
#include "mpeg4bitstream.h"

#define PFX "mpeg4_bitstream "
#define PFX_ERR PFX "error: "
#define PFX_HEADER PFX "header: "
#define PFX_STARTCODE PFX "startcode: "
#define PFX_TIMECODE PFX "timecode: "

int check_resync_marker(Bitstream * bs, int addbits);
int bs_get_spritetrajectory(Bitstream * bs);
static void
read_vol_complexity_estimation_header(Bitstream * bs, DECODER * dec);
static void
read_vop_complexity_estimation_header(Bitstream * bs, DECODER * dec,
                                      int coding_type);

int bs_get_spritetrajectory(Bitstream * bs) {
  int i;
  for (i = 0; i < 12; i++) {
    if (BitstreamShowBits(bs, sprite_trajectory_len[i].len) ==
        sprite_trajectory_len[i].code) {
      BitstreamSkip(bs, sprite_trajectory_len[i].len);
      return i;
    }
  }
  return -1;
}

VLC sprite_trajectory_len[] = {
  { 0x00 , 2}, 
  { 0x02 , 3}, { 0x03, 3}, { 0x04, 3}, { 0x05, 3}, { 0x06, 3}, 
  { 0x0E , 4}, { 0x1E, 5}, { 0x3E, 6}, { 0x7F, 7}, { 0xFE, 8},
  { 0x1FE, 9}, {0x3FE,10}, {0x7FE,11}, {0xFFE,12},
  { 21514, 26984}, { 8307, 28531}, { 29798, 24951}, { 25970, 26912},
  { 8307, 25956}, { 26994, 25974},  { 8292, 29286}, { 28015, 29728},
  { 25960, 18208}, { 21838, 18208}, { 19536, 22560}, { 26998,  8260},
  { 28515, 25956},  { 8291, 25640}, { 30309, 27749}, { 11817, 22794},
  { 30063,  8306}, { 28531, 29798}, { 24951, 25970}, { 25632, 29545},
  { 29300, 25193}, { 29813, 29295}, { 26656, 29537}, { 29728,  8303},
  { 26983, 25974}, { 24864, 25443}, { 29541,  8307}, { 28532, 26912},
  { 29556, 29472}, { 30063, 25458},  { 8293, 28515}, { 25956,  2606} 
};


static uint32_t __inline
log2bin(uint32_t value) {
  int n = 0;

  while (value) {
    value >>= 1;
    n++;
  }
  return n;
}

void
bs_get_matrix(Bitstream * bs, uint8_t * matrix) {
  int i = 0, v;
  do {
    i++;
    v = BitstreamGetBits(bs, 8);
  }
  while (v != 0 && i < 64);
}

int readVOLheader(Bitstream * bs, DECODER * dec) { 
  int vol_ver_id;

  BitstreamSkip(bs, 32);  // video_object_layer_start_code

  BitstreamSkip(bs, 1);  // random_accessible_vol

  // video_object_type_indication
  if (BitstreamShowBits(bs, 8) != VIDOBJLAY_TYPE_SIMPLE &&
    BitstreamShowBits(bs, 8) != VIDOBJLAY_TYPE_CORE &&
    BitstreamShowBits(bs, 8) != VIDOBJLAY_TYPE_MAIN &&
    BitstreamShowBits(bs, 8) != VIDOBJLAY_TYPE_ACE &&
    BitstreamShowBits(bs, 8) != VIDOBJLAY_TYPE_ART_SIMPLE &&
    BitstreamShowBits(bs, 8) != 0) {  // BUGGY DIVX
    mxverb(2, PFX_ERR "video_object_type_indication %i not supported\n",
      BitstreamShowBits(bs, 8));
    return -1;
  }
  BitstreamSkip(bs, 8);


  if (BitstreamGetBit(bs)) { // is_object_layer_identifier
    mxverb(2, PFX_HEADER  "+ is_object_layer_identifier");
    vol_ver_id = BitstreamGetBits(bs, 4);  // video_object_layer_verid
    mxverb(2, "ver_id %i\n", vol_ver_id);
    BitstreamSkip(bs, 3);  // video_object_layer_priority
  } else
    vol_ver_id = 1;

  dec->aspect_ratio = BitstreamGetBits(bs, 4);

  if (dec->aspect_ratio == VIDOBJLAY_AR_EXTPAR) { // aspect_ratio_info
    mxverb(2, PFX_HEADER  "+ aspect_ratio_info\n");
    dec->par_width = BitstreamGetBits(bs, 8);  // par_width
    dec->par_height = BitstreamGetBits(bs, 8);  // par_height
  }

  if (BitstreamGetBit(bs)) {  // vol_control_parameters
    mxverb(2, PFX_HEADER  "+ vol_control_parameters");
    BitstreamSkip(bs, 2);  // chroma_format
    dec->low_delay = BitstreamGetBit(bs);  // low_delay
    mxverb(2, "low_delay %i\n", dec->low_delay);
    if (BitstreamGetBit(bs)) { // vbv_parameters
      unsigned int bitrate;
      unsigned int buffer_size;
      unsigned int occupancy;

      mxverb(2, PFX_HEADER "+ vbv_parameters");

      bitrate = BitstreamGetBits(bs,15) << 15;  // first_half_bit_rate
      READ_MARKER();
      bitrate |= BitstreamGetBits(bs,15);    // latter_half_bit_rate
      READ_MARKER();

      buffer_size = BitstreamGetBits(bs, 15) << 3; // first_half_vbv_buffer_size
      READ_MARKER();
      buffer_size |= BitstreamGetBits(bs, 3);    // latter_half_vbv_buffer_size

      occupancy = BitstreamGetBits(bs, 11) << 15;  // first_half_vbv_occupancy
      READ_MARKER();
      occupancy |= BitstreamGetBits(bs, 15);  // latter_half_vbv_occupancy
      READ_MARKER();

      mxverb(2, "bitrate %d (unit=400 bps)", bitrate);
      mxverb(2, "buffer_size %d (unit=16384 bits)", buffer_size);
      mxverb(2, "occupancy %d (unit=64 bits)\n", occupancy);
    }
  } else
    dec->low_delay = 1;

  dec->shape = BitstreamGetBits(bs, 2);  // video_object_layer_shape

  mxverb(2, PFX_HEADER  "shape %i\n", dec->shape);
  if (dec->shape != VIDOBJLAY_SHAPE_RECTANGULAR)
    mxverb(2, PFX_ERR "non-rectangular shapes are not supported\n");

  if (dec->shape == VIDOBJLAY_SHAPE_GRAYSCALE && vol_ver_id != 1)
    BitstreamSkip(bs, 4);  // video_object_layer_shape_extension

  READ_MARKER();

// *************************** decode B-frame time ***********************
  // vop_time_increment_resolution
  dec->time_inc_resolution = BitstreamGetBits(bs, 16);
  mxverb(2, PFX_HEADER "vop_time_increment_resolution %i\n",
         dec->time_inc_resolution);

  dec->time_inc_resolution--;

  if (dec->time_inc_resolution > 0)
    dec->time_inc_bits = log2bin(dec->time_inc_resolution-1);
  else
    // dec->time_inc_bits = 0;
    // for "old" xvid compatibility, set time_inc_bits = 1
    dec->time_inc_bits = 1;

  READ_MARKER();

  if (BitstreamGetBit(bs)) { // fixed_vop_rate
    mxverb(2, PFX_HEADER  "+ fixed_vop_rate\n");
    BitstreamSkip(bs, dec->time_inc_bits);  // fixed_vop_time_increment
  }

  if (dec->shape != VIDOBJLAY_SHAPE_BINARY_ONLY) {

    if (dec->shape == VIDOBJLAY_SHAPE_RECTANGULAR) {
      uint32_t width, height;

      READ_MARKER();
      width = BitstreamGetBits(bs, 13);  // video_object_layer_width
      READ_MARKER();
      height = BitstreamGetBits(bs, 13);  // video_object_layer_height
      READ_MARKER();

      mxverb(2, PFX_HEADER "width %i", width);
      mxverb(2, "height %i\n", height);

      dec->width = width;
      dec->height = height;
    }

    dec->interlacing = BitstreamGetBit(bs);
    mxverb(2, PFX_HEADER "interlacing %i\n", dec->interlacing);

    if (!BitstreamGetBit(bs)) { // obmc_disable
      mxverb(2, PFX_ERR "obmc_disabled==false not supported\n");
      // TODO
      // divx4.02 has this enabled
    }

    // sprite_enable:
    dec->sprite_enable = BitstreamGetBits(bs, (vol_ver_id == 1 ? 1 : 2));

    if (dec->sprite_enable == SPRITE_STATIC ||
        dec->sprite_enable == SPRITE_GMC) {
      int low_latency_sprite_enable;

      if (dec->sprite_enable != SPRITE_GMC) {
        int sprite_width;
        int sprite_height;
        int sprite_left_coord;
        int sprite_top_coord;
        sprite_width = BitstreamGetBits(bs, 13);    // sprite_width
        READ_MARKER();
        sprite_height = BitstreamGetBits(bs, 13);  // sprite_height
        READ_MARKER();
        sprite_left_coord = BitstreamGetBits(bs, 13); // sprite_left_coordinate
        READ_MARKER();
        sprite_top_coord = BitstreamGetBits(bs, 13);  // sprite_top_coordinate
        READ_MARKER();
      }
      // no_of_sprite_warping_points:
      dec->sprite_warping_points = BitstreamGetBits(bs, 6); 
      // sprite_warping_accuracy:
      dec->sprite_warping_accuracy = BitstreamGetBits(bs, 2);
      // brightness_change:
      dec->sprite_brightness_change = BitstreamGetBits(bs, 1);
      if (dec->sprite_enable != SPRITE_GMC)
        // low_latency_sprite_enable:
        low_latency_sprite_enable = BitstreamGetBits(bs, 1);
    }

    if (vol_ver_id != 1 && dec->shape != VIDOBJLAY_SHAPE_RECTANGULAR)
      BitstreamSkip(bs, 1);  // sadct_disable

    if (BitstreamGetBit(bs)) { // not_8_bit
      mxverb(2, PFX_HEADER "not_8_bit==true (ignored)\n");
      dec->quant_bits = BitstreamGetBits(bs, 4);  // quant_precision
      BitstreamSkip(bs, 4);  // bits_per_pixel
    } else
      dec->quant_bits = 5;

    if (dec->shape == VIDOBJLAY_SHAPE_GRAYSCALE) {
      BitstreamSkip(bs, 1);  // no_gray_quant_update
      BitstreamSkip(bs, 1);  // composition_method
      BitstreamSkip(bs, 1);  // linear_composition
    }

    dec->quant_type = BitstreamGetBit(bs);  // quant_type
    mxverb(2, PFX_HEADER "quant_type %i\n", dec->quant_type);

    if (dec->quant_type) {
      if (BitstreamGetBit(bs)) { // load_intra_quant_mat
        uint8_t matrix[64];

        mxverb(2, PFX_HEADER  "load_intra_quant_mat\n");

        bs_get_matrix(bs, matrix);
      }

      if (BitstreamGetBit(bs)) { // load_inter_quant_mat
        uint8_t matrix[64];

        mxverb(2, PFX_HEADER  "load_inter_quant_mat\n");

        bs_get_matrix(bs, matrix);

      }
      if (dec->shape == VIDOBJLAY_SHAPE_GRAYSCALE) {
        mxverb(2, PFX_ERR  "greyscale matrix not supported\n");
        return -1;
      }

    }

    if (vol_ver_id != 1) {
      int quarterpel = BitstreamGetBit(bs);  // quarter_sample
      mxverb(2, PFX_HEADER "quarterpel %i\n", quarterpel);
    }

    /* complexity estimation disable: */
    dec->complexity_estimation_disable = BitstreamGetBit(bs);
    if (!dec->complexity_estimation_disable)
      read_vol_complexity_estimation_header(bs, dec);

    BitstreamSkip(bs, 1);  // resync_marker_disable

    if (BitstreamGetBit(bs)) { // data_partitioned
      mxverb(2, PFX_ERR  "data_partitioned not supported\n");
      BitstreamSkip(bs, 1);  // reversible_vlc
    }

    if (vol_ver_id != 1) {
      dec->newpred_enable = BitstreamGetBit(bs);
      if (dec->newpred_enable) { // newpred_enable
        mxverb(2, PFX_HEADER  "+ newpred_enable\n");
        BitstreamSkip(bs, 2);  // requested_upstream_message_type
        BitstreamSkip(bs, 1);  // newpred_segment_type
      }
      /* reduced_resolution_vop_enable: */
      dec->reduced_resolution_enable = BitstreamGetBit(bs);
      mxverb(2, PFX_HEADER  "reduced_resolution_enable %i\n",
             dec->reduced_resolution_enable);
    } else {
      dec->newpred_enable = 0;
      dec->reduced_resolution_enable = 0;
    }

    dec->scalability = BitstreamGetBit(bs);  /* scalability */
    if (dec->scalability) {
      mxverb(2, PFX_ERR  "scalability not supported\n");
      BitstreamSkip(bs, 1);  /* hierarchy_type */
      BitstreamSkip(bs, 4);  /* ref_layer_id */
      BitstreamSkip(bs, 1);  /* ref_layer_sampling_direc */
      BitstreamSkip(bs, 5);  /* hor_sampling_factor_n */
      BitstreamSkip(bs, 5);  /* hor_sampling_factor_m */
      BitstreamSkip(bs, 5);  /* vert_sampling_factor_n */
      BitstreamSkip(bs, 5);  /* vert_sampling_factor_m */
      BitstreamSkip(bs, 1);  /* enhancement_type */
      if(dec->shape == VIDOBJLAY_SHAPE_BINARY /* && hierarchy_type==0 */) {
        BitstreamSkip(bs, 1);  /* use_ref_shape */
        BitstreamSkip(bs, 1);  /* use_ref_texture */
        BitstreamSkip(bs, 5);  /* shape_hor_sampling_factor_n */
        BitstreamSkip(bs, 5);  /* shape_hor_sampling_factor_m */
        BitstreamSkip(bs, 5);  /* shape_vert_sampling_factor_n */
        BitstreamSkip(bs, 5);  /* shape_vert_sampling_factor_m */
      }
      return -1;
    }
  } else {       // dec->shape == BINARY_ONLY
    if (vol_ver_id != 1) {
      dec->scalability = BitstreamGetBit(bs); /* scalability */
      if (dec->scalability) {
        mxverb(2, PFX_ERR  "scalability not supported\n");
        BitstreamSkip(bs, 4);  /* ref_layer_id */
        BitstreamSkip(bs, 5);  /* hor_sampling_factor_n */
        BitstreamSkip(bs, 5);  /* hor_sampling_factor_m */
        BitstreamSkip(bs, 5);  /* vert_sampling_factor_n */
        BitstreamSkip(bs, 5);  /* vert_sampling_factor_m */
        return -1;
      }
    }
    BitstreamSkip(bs, 1);  // resync_marker_disable

  }

  return 0;  /* VOL */
}

int readVOPheader(Bitstream * bs, DECODER * dec) {
  int coding_type;
  int time_incr = 0;
  int time_increment = 0;
  int quant;

  mxverb(2, PFX_STARTCODE  "<vop>\n");

  BitstreamSkip(bs, 32);  // vop_start_code

  coding_type = BitstreamGetBits(bs, 2);  // vop_coding_type
  mxverb(2, PFX_HEADER  "coding_type %i\n", coding_type);

// *************************** for decode B-frame time ***********************
  while (BitstreamGetBit(bs) != 0)  // time_base
    time_incr++;

  READ_MARKER();

  if (dec->time_inc_bits)
    // vop_time_increment
    time_increment = (BitstreamGetBits(bs, dec->time_inc_bits));

  mxverb(2, PFX_HEADER  "time_base %i\n", time_incr);
  mxverb(2, PFX_HEADER  "time_increment %i\n", time_increment);

  mxverb(2, PFX_TIMECODE "%c %i:%i\n",
         coding_type == I_VOP ? 'I' : coding_type == P_VOP ? 'P' :
         coding_type == B_VOP ? 'B' : 'S', time_incr, time_increment);

  if (coding_type != B_VOP) {
    dec->last_time_base = dec->time_base;
    dec->time_base += time_incr;
    dec->time = time_increment;

    dec->time_pp = (uint32_t)
      (dec->time_inc_resolution + dec->time - dec->last_non_b_time) % 
      dec->time_inc_resolution;
    dec->last_non_b_time = dec->time;
  } else {
    dec->time = time_increment;

    dec->time_bp = (uint32_t)
      (dec->time_inc_resolution + dec->last_non_b_time - dec->time) %
      dec->time_inc_resolution;
  }
  mxverb(2, PFX_HEADER "time_pp=%i", dec->time_pp);
  mxverb(2, "time_bp=%i\n", dec->time_bp);

  READ_MARKER();

  if (!BitstreamGetBit(bs)) { // vop_coded
    mxverb(2, PFX_HEADER  "vop_coded==false\n");
    return N_VOP;
  }

  if (dec->newpred_enable) {
    int vop_id;
    int vop_id_for_prediction;

    vop_id = BitstreamGetBits(bs, MIN(dec->time_inc_bits + 3, 15));
    mxverb(2, PFX_HEADER  "vop_id %i\n", vop_id);
    if (BitstreamGetBit(bs)) { /* vop_id_for_prediction_indication */
      vop_id_for_prediction = BitstreamGetBits(bs, MIN(dec->time_inc_bits + 3,
                                                       15));
      mxverb(2, PFX_HEADER  "vop_id_for_prediction %i\n",
             vop_id_for_prediction);
    }
    READ_MARKER();
  }

  // fix a little bug by MinChen <chenm002@163.com>
  if ((dec->shape != VIDOBJLAY_SHAPE_BINARY_ONLY) &&
      ((coding_type == P_VOP) ||
       (coding_type == S_VOP && dec->sprite_enable == SPRITE_GMC)))
    BitstreamGetBit(bs);  // rounding_type

  if (dec->reduced_resolution_enable &&
      dec->shape == VIDOBJLAY_SHAPE_RECTANGULAR &&
      (coding_type == P_VOP || coding_type == I_VOP))
    /**reduced_resolution = */BitstreamGetBit(bs);

  if (dec->shape != VIDOBJLAY_SHAPE_RECTANGULAR) {
    if(!(dec->sprite_enable == SPRITE_STATIC && coding_type == I_VOP)) {

      uint32_t width, height;
      uint32_t horiz_mc_ref, vert_mc_ref;

      width = BitstreamGetBits(bs, 13);
      READ_MARKER();
      height = BitstreamGetBits(bs, 13);
      READ_MARKER();
      horiz_mc_ref = BitstreamGetBits(bs, 13);
      READ_MARKER();
      vert_mc_ref = BitstreamGetBits(bs, 13);
      READ_MARKER();

      mxverb(2, PFX_HEADER  "width %i", width);
      mxverb(2, "height %i", height);
      mxverb(2, "horiz_mc_ref %i", horiz_mc_ref);
      mxverb(2, "vert_mc_ref %i\n", vert_mc_ref);
    }

    BitstreamSkip(bs, 1);  // change_conv_ratio_disable
    if (BitstreamGetBit(bs))  // vop_constant_alpha
      BitstreamSkip(bs, 8);  // vop_constant_alpha_value
  }

  if (dec->shape != VIDOBJLAY_SHAPE_BINARY_ONLY) {

    if (!dec->complexity_estimation_disable)
      read_vop_complexity_estimation_header(bs, dec, coding_type);

    // intra_dc_vlc_threshold
    BitstreamGetBits(bs, 3);

    dec->top_field_first = 0;
    dec->alternate_vertical_scan = 0;

    if (dec->interlacing) {
      dec->top_field_first = BitstreamGetBit(bs);
      mxverb(2, PFX_HEADER  "interlace top_field_first %i\n",
             dec->top_field_first);
      dec->alternate_vertical_scan = BitstreamGetBit(bs);
      mxverb(2, PFX_HEADER  "interlace alternate_vertical_scan %i\n",
             dec->alternate_vertical_scan);

    }
  }

  if ((dec->sprite_enable == SPRITE_STATIC ||
       dec->sprite_enable== SPRITE_GMC) && coding_type == S_VOP) {
    int i;

    for (i = 0 ; i < dec->sprite_warping_points; i++) {
      int length;
      int x = 0, y = 0;

      /* sprite code borowed from ffmpeg;
         thx Michael Niedermayer <michaelni@gmx.at> */
      length = bs_get_spritetrajectory(bs);
      if (length) {
        x= BitstreamGetBits(bs, length);
        if ((x >> (length - 1)) == 0) /* if MSB not set it is negative*/
          x = - (x ^ ((1 << length) - 1));
      }
      READ_MARKER();

      length = bs_get_spritetrajectory(bs);
      if (length) {
        y = BitstreamGetBits(bs, length);
        if ((y >> (length - 1)) == 0) /* if MSB not set it is negative*/
          y = - (y ^ ((1 << length) - 1));
      }
      READ_MARKER();

      mxverb(2, PFX_HEADER "sprite_warping_point[%i] xy=(%i,%i)\n", i, x, y);
    }

    if (dec->sprite_brightness_change) {
      // XXX: brightness_change_factor()
    }
    if (dec->sprite_enable == SPRITE_STATIC) {
      // XXX: todo
    }

  }

  if ((quant = BitstreamGetBits(bs, dec->quant_bits)) < 1)  // vop_quant
    quant = 1;
  mxverb(2, PFX_HEADER  "quant %i\n", quant);

  if (coding_type != I_VOP)
    BitstreamGetBits(bs, 3);  // fcode_forward

  if (coding_type == B_VOP)
    BitstreamGetBits(bs, 3);  // fcode_backward
  if (!dec->scalability) {
    if ((dec->shape != VIDOBJLAY_SHAPE_RECTANGULAR) &&
      (coding_type != I_VOP))
      BitstreamSkip(bs, 1);  // vop_shape_coding_type
  }
  return coding_type;
}

/* vol estimation header */

static void
read_vol_complexity_estimation_header(Bitstream * bs, DECODER * dec) {
  ESTIMATION * e = &dec->estimation;

  e->method = BitstreamGetBits(bs, 2);  /* estimation_method */
  mxverb(2, PFX_HEADER "+ complexity_estimation_header; method=%i\n",
         e->method);

  if (e->method == 0 || e->method == 1) {
    if (!BitstreamGetBit(bs)) {   /* shape_complexity_estimation_disable */
      e->opaque = BitstreamGetBit(bs);    /* opaque */
      e->transparent = BitstreamGetBit(bs);    /* transparent */
      e->intra_cae = BitstreamGetBit(bs);    /* intra_cae */
      e->inter_cae = BitstreamGetBit(bs);    /* inter_cae */
      e->no_update = BitstreamGetBit(bs);    /* no_update */
      e->upsampling = BitstreamGetBit(bs);    /* upsampling */
    }

    /* texture_complexity_estimation_set_1_disable */
    if (!BitstreamGetBit(bs)) {
      e->intra_blocks = BitstreamGetBit(bs);    /* intra_blocks */
      e->inter_blocks = BitstreamGetBit(bs);    /* inter_blocks */
      e->inter4v_blocks = BitstreamGetBit(bs);    /* inter4v_blocks */
      e->not_coded_blocks = BitstreamGetBit(bs);    /* not_coded_blocks */
    }
  }

  READ_MARKER();

  if (!BitstreamGetBit(bs)) { /* texture_complexity_estimation_set_2_disable */
    e->dct_coefs = BitstreamGetBit(bs);    /* dct_coefs */
    e->dct_lines = BitstreamGetBit(bs);    /* dct_lines */
    e->vlc_symbols = BitstreamGetBit(bs);    /* vlc_symbols */
    e->vlc_bits = BitstreamGetBit(bs);    /* vlc_bits */
  }

  if (!BitstreamGetBit(bs)) { /* motion_compensation_complexity_disable */
    e->apm = BitstreamGetBit(bs);    /* apm */
    e->npm = BitstreamGetBit(bs);    /* npm */
    e->interpolate_mc_q = BitstreamGetBit(bs);    /* interpolate_mc_q */
    e->forw_back_mc_q = BitstreamGetBit(bs);    /* forw_back_mc_q */
    e->halfpel2 = BitstreamGetBit(bs);    /* halfpel2 */
    e->halfpel4 = BitstreamGetBit(bs);    /* halfpel4 */
  }

  READ_MARKER();

  if (e->method == 1) {
    if (!BitstreamGetBit(bs)) { /* version2_complexity_estimation_disable */
      e->sadct = BitstreamGetBit(bs);    /* sadct */
      e->quarterpel = BitstreamGetBit(bs);    /* quarterpel */
    }
  }
}


/* vop estimation header */
static void
read_vop_complexity_estimation_header(Bitstream *bs, DECODER *dec,
                                      int coding_type) {
  ESTIMATION * e = &dec->estimation;

  if (e->method == 0 || e->method == 1) {
    if (coding_type == I_VOP) {
      if (e->opaque)    BitstreamSkip(bs, 8);  /* dcecs_opaque */
      if (e->transparent) BitstreamSkip(bs, 8);  /* */
      if (e->intra_cae)  BitstreamSkip(bs, 8);  /* */
      if (e->inter_cae)  BitstreamSkip(bs, 8);  /* */
      if (e->no_update)  BitstreamSkip(bs, 8);  /* */
      if (e->upsampling)  BitstreamSkip(bs, 8);  /* */
      if (e->intra_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->not_coded_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->dct_coefs)  BitstreamSkip(bs, 8);  /* */
      if (e->dct_lines)  BitstreamSkip(bs, 8);  /* */
      if (e->vlc_symbols) BitstreamSkip(bs, 8);  /* */
      if (e->vlc_bits)  BitstreamSkip(bs, 8);  /* */
      if (e->sadct)    BitstreamSkip(bs, 8);  /* */
    }

    if (coding_type == P_VOP) {
      if (e->opaque) BitstreamSkip(bs, 8);    /* */
      if (e->transparent) BitstreamSkip(bs, 8);  /* */
      if (e->intra_cae)  BitstreamSkip(bs, 8);  /* */
      if (e->inter_cae)  BitstreamSkip(bs, 8);  /* */
      if (e->no_update)  BitstreamSkip(bs, 8);  /* */
      if (e->upsampling) BitstreamSkip(bs, 8);  /* */
      if (e->intra_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->not_coded_blocks)  BitstreamSkip(bs, 8);  /* */
      if (e->dct_coefs)  BitstreamSkip(bs, 8);  /* */
      if (e->dct_lines)  BitstreamSkip(bs, 8);  /* */
      if (e->vlc_symbols) BitstreamSkip(bs, 8);  /* */
      if (e->vlc_bits)  BitstreamSkip(bs, 8);  /* */
      if (e->inter_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->inter4v_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->apm)      BitstreamSkip(bs, 8);  /* */
      if (e->npm)      BitstreamSkip(bs, 8);  /* */
      if (e->forw_back_mc_q) BitstreamSkip(bs, 8);  /* */
      if (e->halfpel2)  BitstreamSkip(bs, 8);  /* */
      if (e->halfpel4)  BitstreamSkip(bs, 8);  /* */
      if (e->sadct)    BitstreamSkip(bs, 8);  /* */
      if (e->quarterpel)  BitstreamSkip(bs, 8);  /* */
    }
    if (coding_type == B_VOP) {
      if (e->opaque)    BitstreamSkip(bs, 8);  /* */
      if (e->transparent)  BitstreamSkip(bs, 8);  /* */
      if (e->intra_cae)  BitstreamSkip(bs, 8);  /* */
      if (e->inter_cae)  BitstreamSkip(bs, 8);  /* */
      if (e->no_update)  BitstreamSkip(bs, 8);  /* */
      if (e->upsampling)  BitstreamSkip(bs, 8);  /* */
      if (e->intra_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->not_coded_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->dct_coefs)  BitstreamSkip(bs, 8);  /* */
      if (e->dct_lines)  BitstreamSkip(bs, 8);  /* */
      if (e->vlc_symbols)  BitstreamSkip(bs, 8);  /* */
      if (e->vlc_bits)  BitstreamSkip(bs, 8);  /* */
      if (e->inter_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->inter4v_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->apm)      BitstreamSkip(bs, 8);  /* */
      if (e->npm)      BitstreamSkip(bs, 8);  /* */
      if (e->forw_back_mc_q) BitstreamSkip(bs, 8);  /* */
      if (e->halfpel2)  BitstreamSkip(bs, 8);  /* */
      if (e->halfpel4)  BitstreamSkip(bs, 8);  /* */
      if (e->interpolate_mc_q) BitstreamSkip(bs, 8);  /* */
      if (e->sadct)    BitstreamSkip(bs, 8);  /* */
      if (e->quarterpel)  BitstreamSkip(bs, 8);  /* */
    }

    if (coding_type == S_VOP && dec->sprite_enable == SPRITE_STATIC) {
      if (e->intra_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->not_coded_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->dct_coefs)  BitstreamSkip(bs, 8);  /* */
      if (e->dct_lines)  BitstreamSkip(bs, 8);  /* */
      if (e->vlc_symbols)  BitstreamSkip(bs, 8);  /* */
      if (e->vlc_bits)  BitstreamSkip(bs, 8);  /* */
      if (e->inter_blocks) BitstreamSkip(bs, 8);  /* */
      if (e->inter4v_blocks)  BitstreamSkip(bs, 8);  /* */
      if (e->apm)      BitstreamSkip(bs, 8);  /* */
      if (e->npm)      BitstreamSkip(bs, 8);  /* */
      if (e->forw_back_mc_q)  BitstreamSkip(bs, 8);  /* */
      if (e->halfpel2)  BitstreamSkip(bs, 8);  /* */
      if (e->halfpel4)  BitstreamSkip(bs, 8);  /* */
      if (e->interpolate_mc_q) BitstreamSkip(bs, 8);  /* */
    }
  }
}


/*
decode headers
returns coding_type, or -1 if error
*/

#define VIDOBJ_START_CODE_MASK    0x0000001f
#define VIDOBJLAY_START_CODE_MASK  0x0000000f

int
BitstreamReadHeaders(Bitstream * bs, DECODER * dec) {
  uint32_t start_code;

  do {

    BitstreamByteAlign(bs);
    start_code = BitstreamShowBits(bs, 32);


/************ visual_object_sequence start */
    if (start_code == VISOBJSEQ_START_CODE) {

      int profile;

      mxverb(2, PFX_STARTCODE  "<visual_object_sequence>\n");

      BitstreamSkip(bs, 32);  // visual_object_sequence_start_code
      /*profile = */BitstreamGetBits(bs, 8);  // profile_and_level_indication

      mxverb(2, PFX_HEADER  "profile_and_level_indication %i\n", profile);
/************ visual_object_sequence stop */
    } else if (start_code == VISOBJSEQ_STOP_CODE) {

      BitstreamSkip(bs, 32);  // visual_object_sequence_stop_code

      mxverb(2, PFX_STARTCODE  "</visual_object_sequence>\n");

/************ visual_object start */
    } else if (start_code == VISOBJ_START_CODE) {
      int visobj_ver_id;

      mxverb(2, PFX_STARTCODE  "<visual_object>\n");

      BitstreamSkip(bs, 32);  // visual_object_start_code
      if (BitstreamGetBit(bs)) { // is_visual_object_identified
        visobj_ver_id = BitstreamGetBits(bs, 4);  // visual_object_ver_id
        mxverb(2, PFX_HEADER "visobj_ver_id %i\n", visobj_ver_id);
        BitstreamSkip(bs, 3);  // visual_object_priority
      } else
        visobj_ver_id = 1;

      // visual_object_type
      if (BitstreamShowBits(bs, 4) != VISOBJ_TYPE_VIDEO) {
        mxverb(2, PFX_ERR "visual_object_type != video\n");
        return -1;
      }
      BitstreamSkip(bs, 4);

      if (BitstreamGetBit(bs)) { // video_signal_type
        mxverb(2, PFX_HEADER "+ video_signal_type\n");
        BitstreamSkip(bs, 3);  // video_format
        BitstreamSkip(bs, 1);  // video_range
        if (BitstreamGetBit(bs)) { // color_description
          mxverb(2, PFX_HEADER "+ color_description\n");
          BitstreamSkip(bs, 8);  // color_primaries
          BitstreamSkip(bs, 8);  // transfer_characteristics
          BitstreamSkip(bs, 8);  // matrix_coefficients
        }
      }
/************ video_object start */
    } else if ((start_code & ~VIDOBJ_START_CODE_MASK) == VIDOBJ_START_CODE) {

      mxverb(2, PFX_STARTCODE  "<video_object>\n");
      mxverb(2, PFX_HEADER  "vo id %i\n", start_code & VIDOBJ_START_CODE_MASK);

      BitstreamSkip(bs, 32);  // video_object_start_code

/************ VOL */
    } else if ((start_code & ~VIDOBJLAY_START_CODE_MASK) ==
               VIDOBJLAY_START_CODE) {

      mxverb(2, PFX_STARTCODE  "<video_object_layer>\n");
      mxverb(2, PFX_HEADER  "vol id %i\n",
             start_code & VIDOBJLAY_START_CODE_MASK);

      readVOLheader(bs, dec);

/************ group of VOP */
    } else if (start_code == GRPOFVOP_START_CODE) {

      mxverb(2, PFX_STARTCODE  "<group_of_vop>\n");

      BitstreamSkip(bs, 32);
      {
        int hours, minutes, seconds;

        hours = BitstreamGetBits(bs, 5);
        minutes = BitstreamGetBits(bs, 6);
        READ_MARKER();
        seconds = BitstreamGetBits(bs, 6);

        mxverb(2, PFX_HEADER  "time %ih%im%is\n", hours,minutes,seconds);
      }
      BitstreamSkip(bs, 1);  // closed_gov
      BitstreamSkip(bs, 1);  // broken_link

/************ VOP */
    } else if (start_code == VOP_START_CODE) {

      return readVOPheader(bs, dec);

    } else if (start_code == USERDATA_START_CODE) {
      char tmp[256];
      int i, version, build;
      char packed;

      BitstreamSkip(bs, 32);  // user_data_start_code

      tmp[0] = BitstreamShowBits(bs, 8);

      for (i = 1; i < 256; i++) {
        tmp[i] = (BitstreamShowBits(bs, 16) & 0xFF);

        if(tmp[i] == 0)
          break;

        BitstreamSkip(bs, 8);
      }

      mxverb(2, PFX_STARTCODE  "<user_data>: %s\n", tmp);

      /* divx detection */
      i = sscanf(tmp, "DivX%dBuild%d%c", &version, &build, &packed);
      if (i < 2)
        i = sscanf(tmp, "DivX%db%d%c", &version, &build, &packed);

      if (i >= 2) {
        dec->packed_mode = (i == 3 && packed == 'p');
        mxverb(2, PFX_HEADER  "divx version=%i, build=%i packed=%i\n",
            version, build, dec->packed_mode);
      }

    } else { // start_code == ?
      if (BitstreamShowBits(bs, 24) == 0x000001)
        mxverb(2, PFX_STARTCODE  "<unknown: %x>\n", BitstreamShowBits(bs, 32));
      BitstreamSkip(bs, 8);
    }
  }
  while ((BitstreamPos(bs) >> 3) < bs->length);

  return -1;          /* ignore it */
}
