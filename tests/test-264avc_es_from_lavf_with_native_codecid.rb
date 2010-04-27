#!/usr/bin/ruby -w

class T_264avc_es_from_lavf_with_native_codecid < Test
  def description
    return "mkvmerge / AVC elementary stream from lavf with native CodecID (bug 486)"
  end

  def run
    merge "data/mkv/avc_es_from_lavf_with_native_codecid.mkv"
    return hash_tmp
  end
end

