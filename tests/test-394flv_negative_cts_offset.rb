#!/usr/bin/ruby -w

# T_394flv_negative_cts_offset
describe "mkvmerge / Flash Video Files with AVC and negative CTS offsets"
test_merge "data/flv/negative-cts-offset.flv"
