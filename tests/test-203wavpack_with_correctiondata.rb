#!/usr/bin/ruby -w

class T_203wavpack_with_correctiondata < Test
  def description
    return "mkvmerge / audio only / WavPack with correction data (*.wv)"
  end

  def run
    merge("data/wp/with-correction.wv")
    return hash_tmp
  end
end

