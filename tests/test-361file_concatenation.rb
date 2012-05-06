#!/usr/bin/ruby

# T_361file_concatenation
describe "mkvmerge / file concatenation, multi file I/O"

files = (1..3).collect { |i| "data/vob/VTS_01_#{i}.VOB" }

test_identify files[0], :verbose => true, :filter => lambda { |text| text.gsub(/other_file:[^ ]+\//, 'other_file:') }

# These two must be equal.
test_merge files[0],                     :exit_code => 1
test_merge "'(' #{files.join(' ')} ')'", :exit_code => 1

# These three must be equal.
test_merge "'=' #{files[0]}"
test_merge "'=#{files[0]}'"
test_merge "'(' #{files[0]} ')'"

# Separate case.
test_merge "'(' #{files[0]} #{files[1]} ')'"

files = (0..3).collect { |i| "data/ts/0000#{i}.m2ts" }

test_identify files[0], :verbose => true

# These four must be equal.
test_merge files[0]
test_merge "'=' #{files[0]}"
test_merge "'=#{files[0]}'"
test_merge "'(' #{files[0]} ')'"

# Two separate cases.
test_merge "'(' #{files[0..1].join(' ')} ')'"
test_merge "'(' #{files[0..2].join(' ')} ')'"

# These two must be equal.
test_merge "'(' #{files.join(' ')} ')'"
test_merge "data/ts/hd_distributor_regency.m2ts"
