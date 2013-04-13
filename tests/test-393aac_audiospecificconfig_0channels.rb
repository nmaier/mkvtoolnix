#!/usr/bin/ruby -w

# T_393aac_audiospecificconfig_0channels
describe "mkvmerge / AAC with AudioSpecificConfig indicating 0 channels"
test_merge "data/aac/audiospecificconfig-with-0-channels.m4a"
