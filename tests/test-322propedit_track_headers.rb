#!/usr/bin/ruby -w

class T_322propedit_track_headers < Test
  def description
    "mkvpropedit / propedit track headers"
  end

  def run
    hashes = []
    sys "cp data/mkv/complex.mkv #{tmp}"
    hashes << hash_tmp(false)

    propedit "--edit track:v1 --set display-width=1600 --set display-height=768"
    hashes << hash_tmp(false)

    propedit "--edit track:a1 --set flag-default=0 --set name=audio"
    hashes << hash_tmp(false)

    propedit "--edit track:v1 --set name=video --edit track:2 --set language=ger"
    hashes << hash_tmp(false)

    propedit "--edit track:=620187839 --set codec-name='Some codec' --edit track:@1 --set flag-enabled=1"
    hashes << hash_tmp

    hashes.join '-'
  end
end

