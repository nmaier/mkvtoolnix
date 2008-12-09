#!/usr/bin/ruby -w

class T_246theora_pixel_aspect_ratio < Test
  def description
    return "mkvmerge / Theora's pixel aspect ratio header fields / in(OGG)"
  end

  def run
    merge("data/ogg/pixel_aspect_ratio.ogg")
    return hash_tmp
  end
end

