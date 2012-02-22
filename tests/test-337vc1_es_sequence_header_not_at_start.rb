#!/usr/bin/ruby -w

# T_337vc1_es_sequence_header_not_at_start
describe "mkvmerge / VC1 elementary stream not starting with sequence headers"

test_merge "data/vc1/MC.track_4113.vc1"
