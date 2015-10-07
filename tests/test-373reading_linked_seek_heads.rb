#!/usr/bin/ruby -w

# T_373reading_linked_seek_heads
describe "mkvmerge / reading files with linked seek heads"

test_merge "data/mkv/linked-seek-heads.mkv"
