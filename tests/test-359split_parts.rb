#!/usr/bin/ruby -w

# T_359split_parts
describe "mkvmerge / --split parts:..."

test "data/avi/v-h264-aac.avi two output files" do
  merge "--split parts:20s-30s,-40s data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-01"), hash_file("#{tmp}-02"), File.exist?("#{tmp}-03") ? "bad" : "ok" ].join('+')
  unlink_tmp_files
  result
end

test "data/avi/v-h264-aac.avi one output file" do
  merge "--split parts:20s-30s,+40s-50s data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-01"), File.exist?("#{tmp}-02") ? "bad" : "ok" ].join('+')
  unlink_tmp_files
  result
end
