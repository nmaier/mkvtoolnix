#!/usr/bin/ruby -w

class T_287mkvextract_exit_codes < Test
  def description
    "mkvextract / exit codes in various situations"
  end

  def run
    result = []

    [ "",                       # Show help if no mode given
      "gnufudel",               # Unknown mode
      "tracks data/mkv/complex.mkv",                         # No track ID given
      "tracks data/mkv/complex.mkv 12345:doesnotexist",      # Non-existing track ID
      "attachments data/mkv/complex.mkv",                    # No attachment ID given
      "attachments data/mkv/complex.mkv 12345:doesnotexist", # Non-existing attachment ID
    ].each do |args|
      sys "../src/mkvextract #{args}", 2
      result << ($? >> 8)
    end

    result.collect(&:to_s).join('-')
  end
end

