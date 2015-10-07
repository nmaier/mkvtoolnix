#!/usr/bin/ruby -w

# T_345flag_enabled
describe "mkvmerge / identification output and muxing with flag enabled"

test_identify "data/mkv/flag_enabled.mkv"
test_merge "data/mkv/flag_enabled.mkv"
