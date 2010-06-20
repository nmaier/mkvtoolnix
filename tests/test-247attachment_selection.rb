#!/usr/bin/ruby -w

class T_247attachment_selection < Test
  def description
    return "mkvmerge / --attachments option"
  end

  def run
    src = "data/mkv/attachments.mkv"
    merge(src)
    result = hash_tmp

    merge("--attachments 1,3,5 #{src}")
    result += "-" + hash_tmp

    merge("--attachments 2,4 #{src}")
    result += "-" + hash_tmp

    merge("--split 40s --split-max-files 2 --attachments 1:all,2:first,3:first,4:first,5:all #{src}")
    result += "-" + hash_file("#{tmp}-001") + "+" + hash_file("#{tmp}-002")

    return result
  end
end
