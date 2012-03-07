#!/usr/bin/ruby -w

class T_245srt_timecode_formats < Test
  def description
    return "mkvmerge / SRT files with various timecode formats (shortened, spaces etc) / in(SRT)"
  end

  def run
    hashes = Array.new

    merge "--subtitle-charset -1:ISO-8859-5 data/srt/shortened_timecodes.srt"
    hashes << hash_tmp

    merge "--subtitle-charset -1:ISO-8859-5 data/srt/timecodes_with_spaces.srt"
    hashes << hash_tmp

    return hashes.join('-')
  end
end

