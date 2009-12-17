#!/usr/bin/ruby -w

class T_257theora_v1_1 < Test
  def description
    return "mkvmerge / Theora from Ogg created with libtheora v1.1 / in(OGG)"
  end

  def run
    merge("data/ogg/video-1.0.ogv")
    h = hash_tmp
    merge("data/ogg/video-1.1.ogv")
    return h + "-" + hash_tmp
  end
end

