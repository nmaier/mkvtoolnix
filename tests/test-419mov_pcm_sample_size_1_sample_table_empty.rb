#!/usr/bin/ruby -w

# T_419mov_pcm_sample_size_1_sample_table_empty
describe "mkvmerge / MOV with PCM, sample_size == 1 and empty sample_table"
test_merge "-D data/pcm/sample-size-1-sample-table-empty.mov"
