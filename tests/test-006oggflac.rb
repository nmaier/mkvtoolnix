#!/usr/bin/ruby -w

class T_006oggflac < Test
  def initialize
    @description = "mkvmerge / audio only / in(FLAC in Ogg)"
  end

  def run
    sys("mkvmerge --engage no_variable_data -o " + tmp + " data/v.flac.ogg")
    return hash_tmp
  end
end
