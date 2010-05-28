#!/usr/bin/ruby -w

class T_271ogg_flac_1_1_1 < Test
  def description
    return "mkvmerge / FLAC in Ogg spec version v1.1.1"
  end

  def run
    merge "data/ogg/oggflac-post-1-1-1.ogg"
    return hash_tmp
  end
end

