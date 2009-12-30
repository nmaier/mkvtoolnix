#!/usr/bin/ruby -w

class T_259mp4_chapters_text_trak < Test
  def description
    return "mkvmerge / chapters in 'text' tracks / in(MP4)"
  end

  def run
    merge "data/mp4/o12-short.m4v"
    hash = [ hash_tmp ]
    merge "--no-chapters data/mp4/o12-short.m4v"
    hash << hash_tmp

    return hash.join "-"
  end
end

