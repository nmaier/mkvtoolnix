#!/usr/bin/ruby -w

# T_391fix_bitstream_frame_rate
describe "mkvmerge / new option --fix-bitstream-timing-information"

test_merge "data/avi/v-h264-aac.avi", :args => "-A --default-duration 0:20ms --fix-bitstream-timing-information 0:1"
