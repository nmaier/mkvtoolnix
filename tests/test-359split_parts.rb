#!/usr/bin/ruby -w

# T_359split_parts
describe "mkvmerge / --split parts:..."

test "data/avi/v-h264-aac.avi" do
  merge "--split parts:20s-30s,-40s data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  [ hash_file("#{tmp}-01"), hash_file("#{tmp}-02"), File.exist?("#{tmp}-03") ? "bad" : "ok" ].join('-')
end
