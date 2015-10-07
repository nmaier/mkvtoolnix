#!/usr/bin/ruby -w

# T_405packet_ordering_and_default_duration
describe "mkvmerge / packet ordering and --default-duration"
test "merging" do
  merge "--default-duration 0:25000/1001fps -A -S -d 0 data/mkv/complex.mkv data/simple/v.mp3"
  hash_tmp
end
