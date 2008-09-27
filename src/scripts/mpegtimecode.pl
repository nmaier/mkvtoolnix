#!/usr/bin/perl -w

use strict 'vars';

use Data::Dumper;

#               pts =
#                 ((int64_t)((buf[0] & 0x0e) << 29 | buf[1] << 22 |
#                            (buf[2] & 0xfe) << 14 | buf[3] << 7 |
#                            (buf[4] >> 1))) * 100000 / 9;

if (!@ARGV) {
  print "need arg\n";
  exit 1;
}

my $str    =  shift;
$str       =~ s/[^0-9a-zA-Z]//g;
my @values =  map { ord } split(m//, pack("H*", $str));

if (scalar(@values) != 5) {
  print "need 5 numbers\n";
  exit 1;
}

print Dumper(\@values);

my $pts = ((($values[0] & 0x0e) << 29) | ($values[1] << 22) | (($values[2] & 0xfe) << 14) | ($values[3] << 7) | ($values[4] >> 1)) / 9;

print "pts ${pts}\n";
