#!/usr/bin/ruby -w

class T_323propedit_segment_info < Test
  def description
    "mkvpropedit / segment info"
  end

  def run
    hashes = []
    sys "cp data/mkv/complex.mkv #{tmp}"
    hashes << hash_tmp(false)

    propedit "--edit info --set title='Some Wuffy' --set segment-filename='#{"long " * 200}'"
    hashes << hash_tmp(false)

    propedit "--edit info --set title='Some Wuffy' --delete segment-filename"
    hashes << hash_tmp

    hashes.join '-'
  end
end

