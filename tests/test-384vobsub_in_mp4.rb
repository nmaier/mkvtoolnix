#!/usr/bin/ruby -w

# T_384vobsub_in_mp4
describe "mkvmerge / VobSub in MP4"
test_merge "data/mp4/vobsub.mp4", :exit_code => :warning
