#!/usr/bin/ruby -w

# T_414vc1_no_sequence_headers_before_key_frames
describe "mkvmerge / VC1, no sequence headers before key frames"
test_merge "data/mkv/vc1-no-sequence-headers-before-key-frames.mkv"
