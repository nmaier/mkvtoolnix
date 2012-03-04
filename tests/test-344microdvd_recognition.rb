#!/usr/bin/ruby -w

# T_344microdvd_recognition
describe "mkvmerge / recognize MicroDVD subtitles as unsupported"

test_merge_unsupported "data/subtitles/microdvd/a_nightmare_on_elm_street3.sub"
