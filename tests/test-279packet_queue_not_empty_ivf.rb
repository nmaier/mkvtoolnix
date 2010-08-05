#!/usr/bin/ruby -w

class T_279packet_queue_not_empty_ivf < Test
  def description
    "mkvmerge / 'packet queue not empty' with IVF files"
  end

  def run
    merge "--default-duration 0:25fps data/webm/packet-queue-not-empty.ivf"
    hash_tmp
  end
end

