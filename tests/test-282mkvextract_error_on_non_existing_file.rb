#!/usr/bin/ruby -w

class T_282mkvextract_error_on_non_existing_file < Test
  def description
    "mkvextract / exit code 2 for non existing files"
  end

  def run
    sys "../src/mkvextract tracks thisfiledoesnotexist982734981q27 1:x.avi", 2
    (($? >> 8) == 2).to_s
  end
end

