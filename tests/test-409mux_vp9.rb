#!/usr/bin/ruby -w

# T_409mux_vp9
describe "mkvmerge / mux VP9 from IVF and Matroska/WebM"

test_merge "data/webm/out-vp9.webm"
test_merge "data/webm/v-vp9.ivf"
