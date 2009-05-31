#!/usr/bin/ruby -w

class T_250tag_selection < Test
  def description
    return "mkvmerge / options for tag copying / in(MKV)"
  end

  def run
    merge("data/mkv/tags.mkv")
    hash = hash_tmp
    merge("--no-global-tags data/mkv/tags.mkv")
    hash += "-" + hash_tmp
    merge("--track-tags 1 data/mkv/tags.mkv")
    hash += "-" + hash_tmp
    merge("--track-tags 2 data/mkv/tags.mkv")
    hash += "-" + hash_tmp
    merge("--no-track-tags data/mkv/tags.mkv")
    hash += "-" + hash_tmp
    merge("-T data/mkv/tags.mkv")
    hash += "-" + hash_tmp
    merge("--no-global-tags -T data/mkv/tags.mkv")
    return hash + "-" + hash_tmp
  end
end

