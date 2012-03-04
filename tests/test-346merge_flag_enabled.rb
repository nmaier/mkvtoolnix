#!/usr/bin/ruby -w

# T_346merge_flag_enabled
describe "mkvmerge / merging Matroska with flag enabled"

test_merge "data/"
test_merge "data/FIXTHIS", :args => "FIXTHIS"

test "data/FIXTHIS" do
  # INSERT COMMANDS
  hash_tmp
end
