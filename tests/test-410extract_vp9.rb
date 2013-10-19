#!/usr/bin/ruby -w

# T_410extract_vp9
describe "mkvextract / extract VP9"

test "extraction" do
  extract "data/webm/out-vp9.webm", 0 => tmp
  hash_tmp
end
