#!/usr/bin/ruby -w

class T_006oggflac < Test
  def initialize
    @description = "mkvmerge / audio only / in(FLAC in Ogg)"
  end

  def run
    merge("data/v.flac.ogg")
    return hash_tmp
  end
end
