#!/usr/bin/ruby -w

# T_367vob_80ms_delay_by_b_frames
describe "mkvmerge / VOB, proper audio/video delay wrt B frames"

%w{2 1605}.each { |name| test_merge "data/mpeg12/bug579/#{name}ms.vob" }
