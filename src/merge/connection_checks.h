/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   connection_check* macros

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MERGE_CONNECTION_CHECKS_H
#define MTX_MERGE_CONNECTION_CHECKS_H

#define connect_check_a_samplerate(a, b)                                                                                       \
  if ((a) != (b)) {                                                                                                            \
    error_message = (boost::format(Y("The sample rate of the two audio tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                          \
  }
#define connect_check_a_channels(a, b)                                                                                                \
  if ((a) != (b)) {                                                                                                                   \
    error_message = (boost::format(Y("The number of channels of the two audio tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                                 \
  }
#define connect_check_a_bitdepth(a, b)                                                                                                       \
  if ((a) != (b)) {                                                                                                                          \
    error_message = (boost::format(Y("The number of bits per sample of the two audio tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                                        \
  }
#define connect_check_v_width(a, b)                                                                                \
  if ((a) != (b)) {                                                                                                \
    error_message = (boost::format(Y("The width of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                              \
  }
#define connect_check_v_height(a, b)                                                                                \
  if ((a) != (b)) {                                                                                                 \
    error_message = (boost::format(Y("The height of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                               \
  }
#define connect_check_v_dwidth(a, b)                                                                                       \
  if ((a) != (b)) {                                                                                                        \
    error_message = (boost::format(Y("The display width of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                      \
  }
#define connect_check_v_dheight(a, b)                                                                                       \
  if ((a) != (b)) {                                                                                                         \
    error_message = (boost::format(Y("The display height of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                       \
  }
#define connect_check_codec_id(a, b)                                                                                 \
  if ((a) != (b)) {                                                                                                  \
    error_message = (boost::format(Y("The CodecID of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                \
  }
#define connect_check_codec_private(b)                                                                                                                                                        \
  if (                                   (!!this->m_ti.m_private_data             != !!b->m_ti.m_private_data)                                                                                \
      || (  this->m_ti.m_private_data && (  this->m_ti.m_private_data->get_size() !=   b->m_ti.m_private_data->get_size()))                                                                   \
      || (  this->m_ti.m_private_data &&   memcmp(this->m_ti.m_private_data->get_buffer(), b->m_ti.m_private_data->get_buffer(), this->m_ti.m_private_data->get_size()))) {                   \
    error_message = (boost::format(Y("The codec's private data does not match (lengths: %1% and %2%).")) % this->m_ti.m_private_data->get_size() % b->m_ti.m_private_data->get_size()).str(); \
    return CAN_CONNECT_MAYBE_CODECPRIVATE;                                                                                                                                                    \
  }

#endif // MTX_MERGE_CONNECTION_CHECKS_H
