#!/usr/bin/ruby -w

# T_352timecode_scale_auto_libmatroska_assert
describe "mkvmerge / wrong calculation of max ns per cluster with --timecode-scale -1"

test_merge "data/mp4/timecode-scale-1.mp4 data/mp4/timecode-scale-1.ogg", :args => "--timecode-scale -1"
