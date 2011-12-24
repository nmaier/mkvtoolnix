#!/usr/bin/ruby -w

class T_269X_vp8_without_default_duration < Test
  def description
    return "mkvextract / VP8 from Matroska and WebM missing the default duration element"
  end

  def run
    xtr_tracks "data/webm/yt3.webm", "0:#{tmp}"
    return hash_tmp
  end
end

