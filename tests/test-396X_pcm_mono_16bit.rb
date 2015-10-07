#!/usr/bin/ruby -w

# T_396X_pcm_mono_16bit
describe "mkvextract / PCM mono 16bit"
test "extraction" do
  extract "data/pcm/mono-16bit.mkv", 1 => tmp
  hash_tmp
end
