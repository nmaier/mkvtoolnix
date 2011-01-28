#!/usr/bin/perl -w

use strict 'vars';

use Digest::SHA1 qw(sha1_base64);

use constant ST_INITIAL   => 0;
use constant ST_IN_DECL   => 1;
use constant ST_IN_STRING => 2;

our %all_formats   = ();
our %ignore        = ();
our $print_numbers = 0;
our $print_tests   = 0;
our @errors        = ();
our @numbers       = ();

sub scan_file {
  my $file_name   = shift;

  my $line_number = 0;
  my $state       = ST_INITIAL;
  my $paren_depth = 0;
  my $start_line  = 0;
  my $nth_arg     = 0;
  my $format_cont = '';

  my $num_formats = 0;
  my $num_fishs   = 0;

  local *IN;

  open(IN, $file_name) || die "$@";

  while (my $line = <IN>) {
    $line_number++;
    my $read_next = 0;
    my $pos       = 0;
    my $escaped   = 0;

    while (!$read_next) {
      if (ST_INITIAL == $state) {
        if ($line =~ m/boost::format\s*\(/) {
          $state       = ST_IN_DECL;
          $paren_depth = 1;
          $pos         = $+[0];
          $start_line  = $line_number;
          $nth_arg     = 0;
          $format_cont = '';
          substr($line, $-[0], 1) = 'm';
          $num_formats++;

        } else {
          $read_next   = 1;
        }

      } elsif (ST_IN_DECL == $state) {
        last if ($pos >= length $line);

        my $char = substr $line, $pos, 1;
        $pos++;

        if ($escaped) {
          $escaped = 0;

        } elsif ($char eq '\\') {
          $escaped = 1;

        } elsif ($char eq '(') {
          $paren_depth++;

        } elsif ($char eq ')') {
          $paren_depth--;
          if (0 == $paren_depth) {
            $state = ST_INITIAL;
          }

        } elsif ($char eq '"') {
          $state = ST_IN_STRING;

        } elsif (($char eq '?') || ($char eq ':')) {
          $nth_arg = 0;

        }

      } elsif (ST_IN_STRING == $state) {
        last if ($pos >= length $line);

        my $char = substr $line, $pos, 1;
        $pos++;

        if ($escaped) {
          $format_cont .= $char;
          $escaped      = 0;

        } elsif ($char eq '\\') {
          $format_cont .= $char;
          $escaped = 1;

        } elsif ($char eq '"') {
          $state = ST_IN_DECL;

        } elsif ($char eq '%') {
          $format_cont .= $char;
          my $rest      = substr $line, $pos;

          my $fishy     = 1;

          $fishy        = 0 if (   $rest =~ m/^\%/
                                || $rest =~ m/^\d+\%/
                                || $rest =~ m/^\|\d+\$\d*[duxX]\|/
                                || $rest =~ m/^\|\d+\$[\+\-]?\d+[fs]\|/
                                || $rest =~ m/^\|\d+\$[\+\-]?\d*\.\d+[fs]\|/);

          my $format   = $&;
          my $checksum = sha1_base64($file_name . $format_cont);
          if ($fishy) {
            push @errors, "Fishy format at ${file_name}:${line_number} (" . ($pos - 1) . ") $checksum\n";
            $pos++;
            $num_fishs++;

          } else {
            $nth_arg++ if ($format ne '%');
            if (($format =~ m/^(\d+)\%$/) || ($format =~ m/^\|(\d+)\$/)) {
              if (($nth_arg != ($1 * 1)) && !$ignore{$checksum}) {
                push @errors, "Bad arg at ${file_name}:${line_number} (" . ($pos - 1) . ") (expected $nth_arg, got $1) $checksum\n";
              }
              $format =~ s/^\|\d+\$/|1\$/;
            }
            $all_formats{'%' . $format} =  1;
            $pos += $+[0];
          }

        } else {
          $format_cont .= $char;
        }

      } else {
        die;
      }
    }
  }

  close IN;

  push @numbers, {
    'file_name'   => $file_name,
    'num_formats' => $num_formats,
    'num_fishs'   => $num_fishs,
  };
}

sub parse_files {
  my @all_files = map { (glob("src/$_/*.cpp"), glob("src/$_/*.h")) } qw(common input output merge info extract mmg tools);

  $| = 1;
  print "0\%\r";
  foreach my $i (0 .. scalar(@all_files) - 1) {
    scan_file($all_files[$i]);
    print "" . int($i * 100 / (scalar(@all_files) - 1)) . "\%\r";
  }
  print "100\%\n";
}

sub parse_args {
  while (scalar @ARGV) {
    if (($ARGV[0] eq '-n') || ($ARGV[0] eq '--numbers')) {
      $print_numbers = 1;

    } elsif (($ARGV[0] eq '-t') || ($ARGV[0] eq '--tests')) {
      $print_tests   = 1;

    } elsif (($ARGV[0] eq '-i') || ($ARGV[0] eq '--ignore')) {
      shift @ARGV;
      scalar(@ARGV) || die 'Missing argument for "-i"';
      $ignore{$ARGV[0]} = 1;

    }

    shift @ARGV;
  }
}

sub print_tests {
  my $i = 0;
  foreach (sort keys %all_formats) {
    next if (m/^\%\d+\%$/ || m/^\%\%$/);
    print "  my_out(boost::format(\"test$i $_\") % );\n";
    $i++;
  }
}

sub print_numbers {
  map { print "Numbers $_->{file_name}:$_->{num_formats}:$_->{num_fishs}\n" } @numbers;
}

sub print_errors {
  map { print } @errors;
}

parse_args();
parse_files();
print_errors();
print_numbers() if ($print_numbers);
print_tests()   if ($print_tests);
