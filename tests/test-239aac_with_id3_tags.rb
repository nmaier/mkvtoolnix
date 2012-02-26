#!/usr/bin/ruby -w

class T_239aac_with_id3_tags < Test
  def description
    return "mkvmerge / AAC with ID3v1 and v2 tags / in(AAC)"
  end

  def run
    merge(1, "data/aac/aac-with-id3-tags.aac")
    return hash_tmp
  end
end

