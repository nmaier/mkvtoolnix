#!/usr/bin/env ruby

if ARGV.size != 2
  puts "Syntax: bin2h.rb source.ext destination.h"
  exit 1
end

bin_name = File.basename(ARGV[0]).gsub(/[^a-z\d]/i, '_').gsub(/_+/, '_') + '_bin'

File.open(ARGV[1], "w") do |file|
  file.puts <<EOT
// Automatically generated. Do not modify.
#ifndef BIN2H__#{bin_name.upcase}_INCLUDED
#define BIN2H__#{bin_name.upcase}_INCLUDED

static unsigned char #{bin_name}[] = {
EOT

  data = IO.binread(ARGV[0]).unpack("C*")
  data.each_with_index do |byte, idx|
    file.write sprintf("0x%02x", byte)
    file.write(((idx + 1) % 13) != 0 ? ", " : ",\n") unless (idx == data.size - 1)
  end

  file.puts <<EOT

};

#endif // BIN2H__#{bin_name.upcase}_INCLUDED
EOT
end
