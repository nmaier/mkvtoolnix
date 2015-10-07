#!/usr/bin/ruby -w

# T_413memory_resize_nonfree_smaller
describe "mkvmerge / memory_c::resize(), non-free, smaller block"
test_merge "data/mkv/h264-nonfree-remove-filler-nalu.mkv"
