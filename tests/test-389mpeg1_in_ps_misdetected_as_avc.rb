#!/usr/bin/ruby -w

# T_389mpeg1_in_ps_misdetected_as_avc
describe "mkvmerge / MPEG-1 in program stream misdetected as AVC"
test_merge "data/mpeg12/mpeg1-misdetected-as-avc.mpg"
