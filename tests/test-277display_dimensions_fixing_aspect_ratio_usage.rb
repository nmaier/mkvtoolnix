#!/usr/bin/ruby -w

class T_277display_dimensions_fixing_aspect_ratio_usage < Test
  def description
    return "mkvmerge / fixing DisplayWidth/Height containing aspect ratio only"
  end

  def run
    %w{ac3.mkv ar.mkv Handbrake-3441.mkv}.collect { |file| merge "data/mkv/aspect_ratio/#{file}" ; hash_tmp }.join "-"
  end
end

