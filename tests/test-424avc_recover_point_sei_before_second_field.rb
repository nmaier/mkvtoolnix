#!/usr/bin/ruby -w

# T_424avc_recover_point_sei_before_second_field
describe "mkvmerge / avc/h.264 with recover point SEIs before second fields"
test_merge "data/ts/recovery_point_sei_before_second_field.ts", :args => "--no-audio"
