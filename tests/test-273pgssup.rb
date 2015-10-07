#!/usr/bin/ruby -w

class T_273pgssup < Test
  def description
    return "mkvmerge & mkvextract / PGS SUP files"
  end

  def run
    hashes = Array.new
    merge "data/subtitles/pgs/x.sup"
    hashes << hash_tmp

    merge "#{tmp}-1", "data/subtitles/pgs/x.sup"
    merge "#{tmp}-2", "#{tmp}-1"
    hashes << hash_file("#{tmp}-2")

    xtr_tracks "#{tmp}-1", "0:#{tmp}-3"
    hashes << hash_file("#{tmp}-3")

    return hashes.join "-"
  end
end

