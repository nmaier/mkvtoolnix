#!/usr/bin/ruby -w

# T_381x_alac
describe "mkvextract / ALAC extraction to CAF"

test_merge "data/alac/test-alacconvert.caf", :keep_tmp => true
test "extraction" do
  extract tmp, 0 => "#{tmp}-x"
  hash_file "#{tmp}-x"
end
