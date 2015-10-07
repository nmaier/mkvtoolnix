#!/usr/bin/ruby -w

class T_037X_aac < Test
  def description
    return "mkvextract / AAC with ADTS headers / out(AAC)"
  end

  def run
    xtr_tracks_s 2
  end
end

