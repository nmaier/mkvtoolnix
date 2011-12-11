#!/usr/bin/ruby -w

class T_255aspect_ratio_display_dimensions < Test
  def description
    "mkvmerge / Handling of aspect ratio & display dimensions"
  end

  def get_display_dimensions file_name
    `mkvmerge --identify-for-mmg "#{file_name}" | grep 'display_dimensions:' | sed -e 's/.*display_dimensions://' -e 's/ .*//'`.chomp
  end

  def run_test_with_args initial_args
    merge "#{initial_args} -A data/mp4/rain_800.mp4"
    result = get_display_dimensions tmp
    [ "", "--display-dimensions 0:1212x2424", "--aspect-ratio 0:5", "--aspect-ratio-factor 0:5" ].each_with_index do |args, idx|
      merge "#{tmp}#{idx}", "#{args} #{tmp}"
      result += "-" + get_display_dimensions("#{tmp}#{idx}")
    end

    result
  end

  def run
    result = ""

    [ "--display-dimensions 3:4254x815", "--aspect-ratio 3:10", "--aspect-ratio-factor 3:10" ].each_with_index do |args, idx|
      result += "#{idx}[" + run_test_with_args(args) + "]"
    end

    result
  end
end

