#!/usr/bin/ruby -w

class T_035X_vfw_video < Test
  def description
    return "mkvextract / Video (V_MS/VFW/FOURCC) to AVI / out(AVI)"
  end

  def run
    return xtr_tracks_s(1)
  end
end

