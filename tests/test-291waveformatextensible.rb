#!/usr/bin/ruby -w

class T_291waveformatextensible < Test
  def description
    "mkvmerge / WAVs & AVIs using WAVEFORMATEXTENSIBLE structs with format tag 0xfffe"
  end

  def run
    %w{wav avi}.each { |ext| merge "data/avi/8point1.#{ext}" ; hash_tmp }.join('-')
  end
end

