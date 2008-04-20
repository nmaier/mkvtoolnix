#!/usr/bin/ruby -w

class T_242ogm_with_chapters < Test
  def description
    return "mkvmerge / OGM files with chapters / in(OGM)"
  end

  def run
    merge("--chapter-charset ISO-8859-15 data/ogg/with_chapters.ogm")
    return hash_tmp
  end
end

