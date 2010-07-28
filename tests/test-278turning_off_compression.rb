#!/usr/bin/ruby -w

class T_278turning_off_compression < Test
  def description
    return "mkvmerge / Matroska with compression, turning off compression"
  end

  def run
    merge "--compression -1:none data/mkv/compression/16.nocomp.mkv + --compression -1:none data/mkv/compression/17.comp.mkv"
    result = hash_tmp
    merge "--compression -1:none data/mkv/compression/16.comp.mkv + --compression -1:none data/mkv/compression/17.nocomp.mkv"
    result + "-" + hash_tmp
  end
end

