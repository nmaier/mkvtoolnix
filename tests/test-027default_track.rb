#!/usr/bin/ruby -w

class T_027default_track < Test
  def description
    return "mkvmerge / default track flag / in(MKV)"
  end

  def run
    merge("--default-track 1 --default-track 5 --default-track 10 " +
           "data/complex.mkv")
    return hash_tmp
  end
end

