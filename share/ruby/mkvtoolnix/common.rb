#!/usr/bin/env ruby

module MKVToolNix
  module Common
    def self.unescape string
      map = {
        '2' => '"',
        's' => ' ',
        'c' => ':',
        'h' => '#',
      }

      return string.gsub(/\\(.)/) { |match| map[$1] || $1 }
    end
  end
end
