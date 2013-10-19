#!/usr/bin/ruby -w

# T_356tags
describe "mkvmerge / XML tags"

source  = "data/subtitles/srt/ven.srt"
invalid = Dir["data/text/tags-invalid-*.xml"]

# Valid files:
(Dir["data/text/tags-*.xml"] - invalid).sort.each do |tags|
  test_merge "--tags 0:#{tags} #{source}"
end

# Invalid files:
invalid.sort.each do |tags|
  test tags do
    messages = merge "--tags 0:#{tags} #{source}", :exit_code => 2
    messages.detect { |line| /The\s+XML\s+tag\s+file.*contains\s+an\s+error/i.match line } ? :ok : :bad
  end
end
