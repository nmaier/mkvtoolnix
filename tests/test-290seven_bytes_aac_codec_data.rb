#!/usr/bin/ruby -w

class T_290seven_bytes_aac_codec_data < Test
  def description
    "mkvmerge / AAC with 7 bytes codec data in AVI"
  end

  def run
    merge "data/avi/7_bytes_aac_codec_data.avi"
    hash_tmp
  end
end

