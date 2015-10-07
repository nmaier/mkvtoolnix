#!/usr/bin/ruby -w

# T_347h264_misdetected_as_ac3
describe "mkvmerge / h264 misdetected as AC3, bug 723"

test_identify "data/h264/misdetected-as-ac3-bug-723.h264"
