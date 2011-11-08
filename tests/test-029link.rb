#!/usr/bin/ruby -w

class T_029link < Test
  NextUID = "0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0a 0x0b " +
    "0x0c 0x0d 0x0e 0x0f"
  PreviousUID = "0x0f 0x0e 0x0d 0x0c 0x0b 0x0a 0x09 0x08 0x07 0x06 0x05 " +
    "0x04 0x03 0x02 0x01 0x00"

  def description
    return "mkvmerge / link to previous & next / in(AVI)"
  end

  def valid_next_uid?(part, needs_exact = true, other_ok = false)
    if (nil == part)
      part = ""
    else
      part = "-00#{part}"
    end
    cmd = "../src/mkvinfo #{tmp}#{part} | grep \"Next segment UID:\" " +
      "2> /dev/null"
    @debug_commands << cmd
    uid = `#{cmd}`.chomp
    return true if (needs_exact and /#{NextUID}/.match(uid))
    return true if (other_ok and ("" != uid))
    return "" == uid
  end

  def valid_previous_uid?(part, needs_exact = true, other_ok = false)
    if (nil == part)
      part = ""
    else
      part = "-00#{part}"
    end
    cmd = "../src/mkvinfo #{tmp}#{part} | grep \"Previous segment UID:\" " +
      "2> /dev/null"
    @debug_commands << cmd
    uid = `#{cmd}`.chomp
    return true if (needs_exact and /#{PreviousUID}/.match(uid))
    return true if (other_ok and ("" != uid))
    return "" == uid
  end

  def run
    begin
      # Part 1: --split but no --link. The first file must have a valid
      # previous UID and the last one a valid next UID.
      merge("--split 30s --link-to-previous \"#{PreviousUID}\" " +
             "--link-to-next \"#{NextUID}\" data/avi/v.avi")
      raise SubtestError.new("test 1-1p failed") unless
        (valid_previous_uid?(1, true, false))
      raise SubtestError.new("test 1-1n failed") unless
        (valid_next_uid?(1, false, false))
      raise SubtestError.new("test 1-2p failed") unless
        (valid_previous_uid?(2, false, false))
      raise SubtestError.new("test 1-2n failed") unless
        (valid_next_uid?(2, false, false))
      raise SubtestError.new("test 1-3p failed") unless
        (valid_previous_uid?(3, false, false))
      raise SubtestError.new("test 1-3n failed") unless
        (valid_next_uid?(3, true, false))

      # Part 2: --split and --link. The first file must have a valid
      # previous UID and the last one a valid next UID. All other UIDs must
      # be set as well.
      merge("--split 30s --link --link-to-previous \"#{PreviousUID}\" " +
             "--link-to-next \"#{NextUID}\" data/avi/v.avi")
      raise SubtestError.new("test 1-1p failed") unless
        (valid_previous_uid?(1, true, false))
      raise SubtestError.new("test 1-1n failed") unless
        (valid_next_uid?(1, false, true))
      raise SubtestError.new("test 1-2p failed") unless
        (valid_previous_uid?(2, false, true))
      raise SubtestError.new("test 1-2n failed") unless
        (valid_next_uid?(2, false, true))
      raise SubtestError.new("test 1-3p failed") unless
        (valid_previous_uid?(3, false, true))
      raise SubtestError.new("test 1-3n failed") unless
        (valid_next_uid?(3, true, false))

      # Part 3: Neither --split nor --link. The file must have a valid
      # previous UID and a valid next UID.
      merge("--link-to-previous \"#{PreviousUID}\" --link-to-next " +
             "\"#{NextUID}\" data/avi/v.avi")
      raise SubtestError.new("test 3p failed") unless
        (valid_previous_uid?(nil, true, false))
      raise SubtestError.new("test 3n failed") unless
        (valid_next_uid?(nil, true, false))
    rescue SubtestError => e
      puts("  failed subtest: " + e.to_s)
      return e.to_s
    end
    return "all ok"
  end
end

