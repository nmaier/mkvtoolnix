#!/usr/bin/ruby -w

class T_293aac_adif_misdetected_as_video < Test
  def description
    "mkvmerge / AAC with ADIF headers misdetected as video"
  end

  def run
    sys "../src/mkvmerge --identify data/simple/aac_adif.aac", 3
    ($? >> 8).to_s
  end
end

