#!/usr/bin/ruby -w

# T_365qtmp4_constant_sample_size
describe "mkvmerge / QuickTime/MP4 files with constant sample size and empty sample tables"
1.upto(2) { |idx| test_merge "data/mp4/fixed_sample_size#{idx}.mp4", :exit_code => :warning }
