#!/usr/bin/ruby -w

class T_241ac3_with_id3_tags < Test
  def description
    return "mkvmerge / AC3 with ID3 tags / in(AC3)"
  end

  def run
    merge("data/ac3/ac3-with-id3-tags.ac3")
    return hash_tmp
  end
end

