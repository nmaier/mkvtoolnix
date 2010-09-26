#!/usr/bin/ruby -w

class T_284merging_chapter_editions_when_appending < Test
  def description
    "mkvmerge / merging chapter editions when appending files"
  end

  def run
    merge "#{tmp}-1", "data/avi/v.avi --chapters data/text/chap1.txt"
    merge "#{tmp}-2", "data/avi/v.avi --chapters data/text/chap2.txt"
    merge "#{tmp}-1 + #{tmp}-2"
    hash_tmp
  end
end

