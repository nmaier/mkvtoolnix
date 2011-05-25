#!/usr/bin/ruby -w

class T_295vc1_rederiving_frame_types < Test
  def description
    "mkvmerge / Rederiving frame types in VC1 video tracks"
  end

  def run
    [ "data/mkv/vc1-from-makemkv.mkv", "data/mkv/vc1-bug-636.mkv" ].collect do |file|
      merge "-A -S #{file}"
      hash_tmp
    end.join('-')
  end
end

