#!/usr/bin/ruby -w

# T_380split_frames
describe "mkvmerge / splitting by frames/fields"

src = "data/avi/v-h264-aac.avi"
[84, 85, 86].each do |frames|
  test "#{src} split after #{frames} frames" do
    merge "--split frames:#{frames} #{src}", :output => "#{tmp}-%02d"
    result = [ hash_file("#{tmp}-01"), hash_file("#{tmp}-02"), File.exist?("#{tmp}-03") ? "bad" : "ok" ].join('+')
    unlink_tmp_files
    result
  end
end
