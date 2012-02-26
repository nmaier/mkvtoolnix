#!/usr/bin/ruby -w

class T_240dts_hd < Test
  def description
    return "mkvmerge / DTS-HD / in(*)"
  end

  def run
    hashes = Array.new

    merge("data/dts/dts-hd.dts")
    hashes << hash_tmp

    merge("-D data/vob/VC1WithDTSHD.EVO")
    hashes << hash_tmp

    return hashes.join("-")
  end
end

