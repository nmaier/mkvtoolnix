#!/usr/bin/env ruby

# Ruby 1.9.x introduce "require_relative" for local requires. 1.9.2
# removes "." from $: and forces us to use "require_relative". 1.8.x
# does not know "require_relative" yet though.
begin
  require_relative()
rescue NoMethodError
  def require_relative *args
    require *args
  end
rescue Exception
end

require "pp"

require_relative "test.d/controller.rb"
require_relative "test.d/results.rb"
require_relative "test.d/test.rb"
require_relative "test.d/util.rb"

begin
  require "thread"
rescue
end

def setup
  ENV[ /darwin/i.match(RUBY_PLATFORM) ? 'LANG' : 'LC_ALL' ] = 'en_US.UTF-8'
  ENV['PATH']                                               = "../src:" + ENV['PATH']
end

def main
  controller = Controller.new

  ARGV.each do |arg|
    if ((arg == "-f") or (arg == "--failed"))
      controller.test_failed = true
    elsif ((arg == "-n") or (arg == "--new"))
      controller.test_new = true
    elsif ((arg == "-u") or (arg == "--update-failed"))
      controller.update_failed = true
    elsif ((arg == "-r") or (arg == "--record-duration"))
      controller.record_duration = true
    elsif (arg =~ /-d([0-9]{4})([0-9]{2})([0-9]{2})-([0-9]{2})([0-9]{2})/)
      controller.test_date_after = Time.local($1, $2, $3, $4, $5, $6)
    elsif (arg =~ /-D([0-9]{4})([0-9]{2})([0-9]{2})-([0-9]{2})([0-9]{2})/)
      controller.test_date_before = Time.local($1, $2, $3, $4, $5, $6)
    elsif /^ (\d{3}) (?: - (\d{3}) )?$/x.match arg
      $1.to_i.upto(($2 || $1).to_i) { |idx| controller.add_test_case sprintf("%03d", idx) }
    elsif %r{^ / (.+) / $}x.match arg
      re = Regexp.new "^T_(\\d+)#{$1}", Regexp::IGNORECASE
      controller.results.results.keys.collect { |e| re.match(e) ? $1 : nil }.compact.each { |e| controller.add_test_case e }
    elsif arg =~ /-j(\d+)/
      controller.num_threads = $1.to_i
    else
      error_and_exit "Unknown argument '#{arg}'."
    end
  end

  controller.go

  exit controller.num_failed > 0 ? 1 : 0
end

setup
main
