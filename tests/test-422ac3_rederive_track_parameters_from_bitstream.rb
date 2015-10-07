#!/usr/bin/ruby -w

# T_422ac3_rederive_track_parameters_from_bitstream
describe "mkvmerge / AC3: re-derive track parameters from the bitstream"
test_merge "data/ac3/wrong-sampling-frequency-in-header.mkv", :args => "--no-video"
