#!/usr/bin/ruby -w

# T_364qtmp4_track_with_empty_chunkmap_table
describe "mkvmerge / Quicktime/MP4 tracks with empty chunkmap tables"
test_merge "data/mp4/empty_chunkmap_table.mp4", :exit_code => :warning
