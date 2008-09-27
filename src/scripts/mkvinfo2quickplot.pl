#!/usr/bin/perl -w

use strict 'vars';

use Getopt::Long;
use IO::File;

if (!scalar @ARGV) {
  print "File name missing.\n";
  exit 1;
}

my $filename = shift @ARGV;

my $in = IO::File->new("mkvinfo -s \"${filename}\" |");
if (!$in) {
  print "Could not spawn mkvinfo: !$\n";
  exit 1;
}

my %datapoints = ();
my $pos        = 0;

while (my $line = <$in>) {
  next unless ($line =~ m/frame,\s+track\s+(\d+),\s+timecode\s+(\d+).*,\s+size\s+(\d+)/i);

  $datapoints{$1} ||= [];
  push @{ $datapoints{$1} }, [$2, $pos];
  $pos += $3;
}

$in->close();

foreach my $track (keys %datapoints) {
  my $out = IO::File->new("pl${track}.txt", "w") || next;
  foreach my $data (@{ $datapoints{$track} }) {
    print $out $data->[1], " ", $data->[0], "\n";
  }
  $out->close();
}
