#!/usr/bin/ruby -w

class T_212ssa_attachments < Test
  def description
    return "mkvmerge / attachments in SSA / in(SSA)"
  end

  def run
    merge("data/ssa-ass/Embedded.ssa")
    sys("../src/mkvextract attachments #{tmp} 1:#{tmp}-1 2:#{tmp}-2 " +
         "3:#{tmp}-3")
    hashes = Array.new
    for i in 1..3
      hashes << hash_file("#{tmp}-#{i}")
      File.unlink("#{tmp}-#{i}")
    end

    h = hash_tmp + "-" + hashes.join("-")

    merge("--no-attachments data/ssa-ass/Embedded.ssa")

    return h + "-" + hash_tmp
  end
end

