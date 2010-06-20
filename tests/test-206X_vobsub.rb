#!/usr/bin/ruby -w

class T_206X_vobsub < Test
  def description
    return "mkvextract / VobSubs / in(MKV)"
  end

  def run
    xtr_tracks("data/mkv/vobsubs.mks", "4:#{tmp}")
    hash = hash_file("#{tmp}.idx") + "-" + hash_file("#{tmp}.sub")
    arg = ""
    1.upto(7) { |i| arg += "#{i}:#{tmp} " }
    xtr_tracks("data/mkv/vobsubs.mks", arg)
    hash += "-" + hash_file("#{tmp}.idx") + "-" + hash_file("#{tmp}.sub")
    merge("#{tmp}.idx")
    hash += "-" + hash_tmp
    return hash
  end
end

