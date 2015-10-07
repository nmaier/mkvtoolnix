#!/usr/bin/ruby -w

class T_280replace_one_byte_with_ebmlvoid < Test
  def description
    return "mkvpropedit / Replace one byte with en EbmlVoid"
  end

  def run
    sys "cp data/mkv/sample-bug536.mkv #{tmp}"
    propedit "--edit track:v1 --set display-width=1600 --set display-height=768"
    hash_tmp
  end
end

