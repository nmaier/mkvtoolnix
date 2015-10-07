#!/usr/bin/ruby -w

# T_386flv_vp6f
describe "mkvmerge / subtitle compression / in(VobSub)"
test_merge "--compression 1:none --compression 2:zlib data/vobsub/ally1-short.idx"
