#!/usr/bin/ruby -w

class T_042X_vorbis < Test
  def description
    return "mkvextract / Vorbis to Ogg / out(OggVorbis)"
  end

  def run
    return xtr_tracks_s(7)
  end
end

