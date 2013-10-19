#!/usr/bin/ruby -w

class T_261line_endings_in_text_files < Test
  def description
    return "mkvmerge / different line endings in text files"
  end

  def run
    checksums = %w{unix dos mac}.collect do |style|
      merge "data/subtitles/srt/line-endings/subs-#{style}.srt"
      hash_tmp
    end

    return checksums.join "-"
  end
end

