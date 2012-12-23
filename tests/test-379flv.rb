#!/usr/bin/ruby -w

# T_379flv
describe "mkvmerge / Macromedia Flash Video files (FLV)"

%w{flv1-mp3-400-240 flv1-mp3-2 h264-aac-640-360}.each do |file_name|
  file_name = "data/flv/#{file_name}.flv"

  ['', '-A', '-D'].each { |args| test_merge file_name, :args => args }
  test_merge file_name, :args => "-a 2", :exit_code => :warning
end
