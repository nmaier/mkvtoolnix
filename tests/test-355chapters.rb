#!/usr/bin/ruby -w

# T_355chapters
describe "mkvmerge / XML chapters"

source  = "data/srt/ven.srt"
invalid = Dir["data/text/chapters-invalid-*.xml"]

# Valid files:
(Dir["data/text/chapters-*.xml"] - invalid).sort.each do |chapters|
  test_merge "#{source} --chapters #{chapters}"
end

# Invalid files:
invalid.sort.each do |chapters|
  test chapters do
    messages = merge "#{source} --chapters #{chapters}", :exit_code => 2
    messages.detect { |line| /The\s+XML\s+chapter\s+file.*contains\s+an\s+error/i.match line } ? :ok : :bad
  end
end
