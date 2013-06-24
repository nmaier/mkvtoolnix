#!/usr/bin/ruby -w

# T_398flv1_no_pixel_dimensions
describe "mkvmerge / FLV1 in FLV without video width & height"
test_merge "data/flv/flv1-video-no-dimensions.flv"
