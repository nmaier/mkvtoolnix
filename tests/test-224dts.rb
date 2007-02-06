#!/usr/bin/ruby -w

class T_224dts < Test
  def description
    return "mkvmerge / DTS audio / in(DTS)"
  end

  def run
    merge("'data/dts/Mega Audio 2ch DTS 1234 kbps.dts'")
    h = hash_tmp
    merge("'data/dts/Mega Audio 6ch DTS 1234 kbps.dts'")
    return h + "-" + hash_tmp
  end
end

