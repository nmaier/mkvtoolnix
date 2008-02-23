#!/usr/bin/ruby -w

class T_234avi_aac_codecid_0x706d < Test
  def description
    return "mkvmerge / AVI with AAC audio CodecID 0x706d / in(AVI)"
  end

  def run
    merge("data/avi/AVI_WITH_AAC_SOUND.avi")
    return hash_tmp
  end
end

