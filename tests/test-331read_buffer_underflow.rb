#!/usr/bin/ruby -w

class T_331read_buffer_underflow < Test
  def description
    "mkvmerge / read buffer integer underflow on incomplete files"
  end

  def run
    merge "data/mkv/underflow.mkv"
    hash_tmp
  end
end

