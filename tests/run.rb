#!/usr/bin/env ruby

require "pp"

begin
  require "thread"
rescue
end

$message_mutex = Mutex.new
def show_message(message)
  $message_mutex.lock
  puts message
  $message_mutex.unlock
end

def error_and_exit(text, exit_code = 2)
  puts text
  exit exit_code
end

class TestController
  attr_accessor :test_failed, :test_new, :test_date_after, :teset_date_before, :update_failed, :num_failed, :record_duration
  attr_reader   :num_threads, :results

  def initialize
    @results          = Results.new(self)
    @test_failed      = false
    @test_new         = false
    @test_date_after  = nil
    @test_date_before = nil
    @update_failed    = false
    @num_threads      = self.get_num_processors
    @record_duration  = false

    @tests            = Array.new
    @dir_entries      = Dir.entries(".")
  end

  def get_num_processors
    case RUBY_PLATFORM
    when /darwin/
      np = `/usr/sbin/sysctl -n hw.availcpu`.to_i
    else
      np = IO.readlines("/proc/cpuinfo").collect { |line| /^processor\s+:\s+(\d+)/.match(line) ? $1.to_i : 0 }.max + 1
    end
    return np > 0 ? np : 1
  end

  def num_threads=(num)
    error_and_exit "Invalid number of threads: must be > 0" if 0 >= num
    @num_threads = num
  end

  def add_test_case(num)
    @tests += @dir_entries.select { |entry| /^test-#{num}/.match(entry) }
  end

  def get_tests_to_run
    test_all = !@test_failed && !@test_new

    return (@tests.empty? ? @dir_entries : @tests).collect do |entry|
      if (FileTest.file?(entry) && (entry =~ /^test-.*\.rb$/))
        class_name  = self.class.file_name_to_class_name entry
        test_this   = test_all
        test_this ||= (@results.exist?(class_name) && ((@test_failed      && (@results.status?(class_name) == :failed)) || (@test_new && (@results.status?(class_name) == :new))) ||
                                                       (@test_date_after  && (@results.date_added?(class_name) < @test_date_after)) ||
                                                       (@test_date_before && (@results.date_added?(class_name) > @test_date_before)))
        test_this ||= !@results.exist?(class_name) && (@test_new || @test_failed)

        test_this ? class_name : nil
      else
        nil
      end
    end.compact.sort_by { |class_name| @results.duration? class_name }.reverse
  end

  def self.file_name_to_class_name(file_name)
    "T_" + file_name.gsub(/^test-/, "").gsub(/\.rb$/, "")
  end

  def self.class_name_to_file_name(class_name)
    class_name.gsub(/^T_/, "test-") + ".rb"
  end

  def go
    @num_failed    = 0
    @tests_to_run  = self.get_tests_to_run
    num_tests      = @tests_to_run.size

    @tests_mutex   = Mutex.new
    @results_mutex = Mutex.new

    start          = Time.now

    self.run_threads
    self.join_threads

    duration = Time.now - start

    show_message "#{@num_failed}/#{num_tests} failed (" + (num_tests > 0 ? (@num_failed * 100 / num_tests).to_s : "0") + "%). " + "Tests took #{duration}s."
  end

  def run_threads
    @threads = Array.new

    (1..@num_threads).each do |number|
      @threads << Thread.new(number) do |thread_number|
        Thread.current[:number] = thread_number

        while true
          @tests_mutex.lock
          class_name = @tests_to_run.shift
          @tests_mutex.unlock

          break unless class_name
          self.run_test class_name
        end
      end
    end
  end

  def join_threads
    @threads.each &:join
  end

  def run_test(class_name)
    file_name = self.class.class_name_to_file_name class_name

    if (!require("./#{file_name}"))
      self.add_result class_name, :failed, :message => " Failed to load '#{file_name}'."
      return
    end

    begin
      current_test = eval "#{class_name}.new"
    rescue
      self.add_result class_name, :failed, :message => " Failed to create an instance of class '#{class_name}'."
      return
    end

    if (current_test.description == "INSERT DESCRIPTION")
      show_message "Skipping '#{class_name}': Not implemented yet"
      return
    end

    show_message "Running '#{class_name}': #{current_test.description}"

    start    = Time.now
    result   = current_test.run_test
    duration = Time.now - start

    if (result)
      if (!@results.exist? class_name)
        self.add_result class_name, :passed, :message => "  NEW test. Storing result '#{result}'.", :checksum => result, :duration => duration

      elsif (@results.hash?(class_name) != result)
        msg =  "  #{class_name} FAILED: checksum is different. Commands:\n"
        msg << "    " + current_test.commands.join("\n    ") + "\n"

        if (update_failed)
          self.add_result class_name, :passed, :message => msg + "  UPDATING result\n", :checksum => result, :duration => duration
        else
          self.add_result class_name, :failed, :message => msg
        end
      else
        self.add_result class_name, :passed, :duration => duration
      end

    else
      self.add_result class_name, :failed, :message => "  #{class_name} FAILED: no result from test"
    end
  end

  def add_result(class_name, result, opts = {})
    @results_mutex.lock

    show_message opts[:message] if opts[:message]
    @num_failed += 1            if result == :failed

    if !@results.exist? class_name
      @results.add class_name, opts[:checksum]
    else
      @results.set      class_name, result
      @results.set_hash class_name, opts[:checksum] if opts[:checksum]
    end

    @results.set_duration class_name, opts[:duration] if opts[:duration] && (result == :passed)

    @results_mutex.unlock
  end
end

class SubtestError < RuntimeError
end

class Test
  attr_reader :commands
  attr_reader :debug_commands

  def initialize
    @tmp_num        = 0
    @tmp_num_mutex  = Mutex.new
    @commands       = Array.new
    @debug_commands = Array.new

    # install md5 handler
    case RUBY_PLATFORM
      when /darwin/
        @md5 = lambda do |name|
          @debug_commands << "/sbin/md5 #{name}"
          `/sbin/md5 #{name}`.chomp.gsub(/.*=\s*/, "")
        end
      else
        @md5 = lambda do |name|
          @debug_commands << "md5sum #{name}"
          `md5sum #{name}`.chomp.gsub(/\s+.*/, "")
      end
    end
  end

  def description
    return "dummy test class"
  end

  def run
    return error("Trying to run the base class")
  end

  def unlink_tmp_files
    return if (ENV["KEEP_TMPFILES"] == "1")
    re = /^#{self.tmp_name_prefix}/
    Dir.entries("/tmp").each do |entry|
      file = "/tmp/#{entry}"
      File.unlink(file) if re.match(file) and File.exists?(file)
    end
  end

  def run_test
    result = nil
    begin
      result = run
    rescue RuntimeError => ex
      show_message ex.to_s
    end
    unlink_tmp_files
    result
  end

  def error(reason)
    show_message "  Failed. Reason: #{reason}"
    raise "test failed"
  end

  def sys(command, *arg)
    @commands       << command
    @debug_commands << command
    command         << " >/dev/null 2>/dev/null " unless (/>/.match(command))

    error "system command failed: #{command} (" + ($? >> 8).to_s + ")" if !system(command) && ((arg.size == 0) || ((arg[0] << 8) != $?))
  end

  def tmp_name_prefix
    [ "/tmp/mkvtoolnix-auto-test", $$.to_s, Thread.current[:number] ].join("-") + "-"
  end

  def tmp_name
    @tmp_num_mutex.lock
    @tmp_num ||= 0
    @tmp_num  += 1
    result     = self.tmp_name_prefix + @tmp_num.to_s
    @tmp_num_mutex.unlock

    result
  end

  def tmp
    @tmp ||= tmp_name
  end

  def hash_file(name)
    @md5.call name
  end

  def hash_tmp(erase = true)
    output = hash_file @tmp

    if erase
      File.unlink(@tmp) if File.exists?(@tmp) && (ENV["KEEP_TMPFILES"] != "1")
      @debug_commands << "rm #{@tmp}"
      @tmp = nil
    end

    output
  end

  def merge(*args)
    retcode = args.detect { |a| !a.is_a? String } || 0
    args.reject!          { |a| !a.is_a? String }

    raise "Wrong use of the 'merge' function." if args.empty? || (2 < args.size)

    command = "../src/mkvmerge --engage no_variable_data -o " + (args.size == 1 ? tmp : args.shift) + " " + args.shift
    sys command, retcode
  end

  def xtr_tracks_s(*args)
    command = "../src/mkvextract tracks data/mkv/complex.mkv --no-variable-data " + args.join(" ") + ":#{tmp}"
    sys command, 0
    hash_tmp
  end

  def xtr_tracks(*args)
    command = "../src/mkvextract tracks #{args.shift} --no-variable-data " + args.join(" ")
    sys command, 0
  end
end

class Results
  attr_reader :results

  def initialize(controller)
    @controller = controller

    load
  end

  def load
    @results = Hash.new
    return unless FileTest.exist?("results.txt")

    IO.readlines("results.txt").each do |line|
      parts    =  line.chomp.split(/:/)
      parts[3] =~ /([0-9]{4})([0-9]{2})([0-9]{2})-([0-9]{2})([0-9]{2})([0-9]{2})/
      parts    << 0 if 4 <= parts.size

      @results[parts[0]] = {
        :hash            => parts[1],
        :status          => parts[2].to_sym,
        :date_added      => Time.local($1, $2, $3, $4, $5, $6),
        :duration        => parts[4].to_f,
      }
    end
  end

  def save
    f = File.new "results.txt", "w"
    @results.keys.sort.each do |key|
      f.puts [ key, @results[key][:hash], @results[key][:status], @results[key][:date_added].strftime("%Y%m%d-%H%M%S"), @results[key][:duration] ].collect { |item| item.to_s }.join(":")
    end
    f.close
  end

  def exist?(name)
    @results.has_key? name
  end

  def test_attr(name, attribute, default = nil)
    if !exist? name
      return default if default
      raise "No such result #{name}"
    end
    @results[name][attribute]
  end

  %w{hash status date_added duration}.each do |attribute|
    define_method("#{attribute}?") do |name|
      test_attr name, attribute.to_sym, attribute == "duration" ? 0 : nil
    end
  end

  def add(name, hash, duration = 0)
    raise "Test does already exist" if exist? name
    @results[name] = {
      :hash        => hash,
      :status      => :new,
      :date_added  => Time.now,
      :duration    => duration,
    }
    save
  end

  def set(name, status)
    return unless exist? name
    @results[name][:status] = status
    save
  end

  def set_hash(name, hash)
    raise "Test does not exist" unless exist? name
    @results[name][:hash] = hash
    save
  end

  def set_duration(name, duration)
    return unless exist?(name) && @controller.record_duration
    @results[name][:duration] = duration
    save
  end
end

def main
  ENV[ /darwin/i.match(RUBY_PLATFORM) ? 'LANG' : 'LC_ALL' ] = 'en_US.UTF-8'
  ENV['PATH']   = "../src:" + ENV['PATH']

  controller = TestController.new

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

main
