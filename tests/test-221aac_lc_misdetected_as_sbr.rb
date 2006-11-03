#!/usr/bin/ruby -w

class T_221aac_lc_misdetected_as_sbr < Test
  def description
    return "mkvmerge / AAC in MP4 with LC profile misdetected as SBR / in(MP4)"
  end

  def run
    merge("data/mp4/blood_ed3_sample_audio.mp4")
    return hash_tmp
  end
end

