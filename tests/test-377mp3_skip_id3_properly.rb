#!/usr/bin/ruby -w

# T_377mp3_skip_id3_properly
describe "mkvmerge / MP3 files with ID3 tags and Xing headers; skipping ID3 tag"
test_merge "data/mp3/id3-tag-and-xing-header.mp3", :exit_code => :warning
