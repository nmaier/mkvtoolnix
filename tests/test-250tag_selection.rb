#!/usr/bin/ruby -w

class T_250tag_selection < Test
  def description
    "mkvmerge / options for tag copying / in(MKV)"
  end

  def run
    [ "data/mkv/tags.mkv",
      "--no-global-tags data/mkv/tags.mkv",
      "--track-tags 0 data/mkv/tags.mkv",
      "--track-tags 1 data/mkv/tags.mkv",
      "--no-track-tags data/mkv/tags.mkv",
      "-T data/mkv/tags.mkv",
      "--no-global-tags -T data/mkv/tags.mkv"
    ].collect do |cmd|
      merge cmd
      hash_tmp
    end.join('-')
  end
end

