#!/usr/bin/ruby -w

class T_006oggflac < Test
  def description
    return "mkvmerge / audio only / in(FLAC in Ogg)"
  end

  def run
    merge("data/ogg/v.flac.ogg")
    return hash_tmp
  end
end
