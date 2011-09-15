#!/usr/bin/ruby -w

class T_298ts_language < Test
  def description
    "mkvmerge / MPEG transport streams: language tags"
  end

  def run
    merge "data/ts/blue_planet.ts"
    hash_tmp
  end
end

