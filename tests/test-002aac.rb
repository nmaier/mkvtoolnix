#!/usr/bin/ruby -w

class T_002aac < Test
  def initialize
    @description = "mkvmerge / audio only / in(AAC)"
  end

  def run
    sys("mkvmerge --engage no_variable_data -o " + tmp + " data/v.aac", 1)
    return hash_tmp
  end
end
