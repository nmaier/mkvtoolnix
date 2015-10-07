#!/usr/bin/ruby -w

class T_268X_vp8 < Test
  def description
    return "mkvextract / VP8 from Matroska and WebM"
  end

  def run
    xtr_tracks "data/webm/live-stream.webm", "0:#{tmp}"
    return hash_tmp
  end
end

