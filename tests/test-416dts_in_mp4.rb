#!/usr/bin/ruby -w

# T_416dts_in_mp4
describe "mkvmerge / reading DTS from MP4"

%w{480p-DTS-5.1 1080p-DTS-HD-7.1}.each do |file_name|
  test_merge "data/mp4/#{file_name}.mp4"
end
