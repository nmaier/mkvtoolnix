#!/usr/bin/env ruby1.9

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
  attr_accessor :test_failed, :test_new, :test_date_after, :teset_date_before, :update_failed, :num_failed
  attr_reader   :num_threads

  def initialize
    @results          = Results.new
    @test_failed      = false
    @test_new         = false
    @test_date_after  = nil
    @test_date_before = nil
    @update_failed    = false
    @num_threads      = self.get_num_processors

    @tests            = Array.new
    @dir_entries      = Dir.entries(".")
  end

  def get_num_processors
    IO.readlines("/proc/cpuinfo").collect { |line| /^processor\s+:\s+(\d+)/.match(line) ? $1.to_i : 0 }.max + 1
  end

  def num_threads=(num)
    error_and_exit "Invalid number of threads: must be > 0" if 0 >= num
    @num_threads = num
  end

  def add_test(num)
    @dir_entries.each { |entry| @tests.push(entry) if /^test-#{arg}/.match(entry) }
  end

  def get_tests_to_run
    test_all = !@test_failed && !@test_new

    return (@tests.empty? ? @dir_entries : @tests).collect do |entry|
      if (FileTest.file?(entry) && (entry =~ /^test-.*\.rb$/))
        class_name  = self.class.file_name_to_class_name entry
        test_this   = test_all
        test_this ||= (@results.exist?(class_name) && ((@test_failed      && (@results.status?(class_name) == "failed")) || (@test_new && (@results.status?(class_name) == "new"))) ||
                                                       (@test_date_after  && (@results.date_added?(class_name) < @test_date_after)) ||
                                                       (@test_date_before && (@results.date_added?(class_name) > @test_date_before)))
        test_this ||= !@results.exist?(class_name) && (@test_new || @test_failed)

        test_this ? class_name : nil
      else
        nil
      end
    end.compact.sort
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
        # puts "tnum is #{thread_number} and #{Thread.current.object_id}"

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
      self.add_result class_name, "failed", " Failed to load '#{file_name}'."
      return
    end

    begin
      current_test = eval "#{class_name}.new"
    rescue
      self.add_result class_name, "failed", " Failed to create an instance of class '#{class_name}'."
      return
    end

    if (current_test.description == "INSERT DESCRIPTION")
      show_message "Skipping '#{class_name}': Not implemented yet"
      return
    end

    show_message "Running '#{class_name}': #{current_test.description}"
    result = current_test.run_test
    if (result)
      if (!@results.exist? class_name)
        self.add_result class_name, "passed", "  NEW test. Storing result '#{result}'.", result

      elsif (@results.hash?(class_name) != result)
        msg = "  FAILED: checksum is different. Commands:\n" +
          "    " + current_test.commands.join("\n    ") + "\n"

        if (update_failed)
          self.add_result class_name, "passed", msg + "  UPDATING result\n", result
        else
          self.add_result class_name, "failed", msg
        end
      else
        self.add_result class_name, "passed"
      end

    else
      self.add_result class_name, "failed", "  FAILED: no result from test"
    end
  end

  def add_result(class_name, result, message = nil, new_checksum = nil)
    @results_mutex.lock

    show_message message                       if message
    @results.set      class_name, result
    @results.set_hash class_name, new_checksum if new_checksum
    @num_failed += 1                           if result == "failed"

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
  end

  def description
    return "dummy test class"
  end

  def run
    return error("Trying to run the base class")
  end

  def unlink_tmp_files
    return if (ENV["KEEP_TMPFILES"] == "1")
    n = self.tmp_name_prefix
    Dir.entries("/tmp").each do |e|
      File.unlink("/tmp/#{e}") if ((e =~ /^#{n}/) and File.exists?("/tmp/#{e}"))
    end
  end

  def run_test
    begin
      return run
    rescue RuntimeError => ex
      unlink_tmp_files
      show_message ex.to_s
      return nil
    end
  end

  def error(reason)
    show_message "  Failed. Reason: #{reason}"
    raise "test failed"
  end

  def sys(command, *arg)
    @commands.push(command)
    @debug_commands.push(command)
    command += " >/dev/null 2>/dev/null " unless (/>/.match(command))
    if (!system(command))
      if ((arg.size == 0) || ((arg[0] << 8) != $?))
        error("system command failed: #{command} (" + ($? >> 8).to_s + ")")
      end
    end
  end

  def tmp_name_prefix
    ["/tmp/mkvtoolnix-auto-test-", $$.to_s, Thread.current[:number]].join "-"
  end

  def tmp_name
    @tmp_num_mutex.lock
    @tmp_num ||= 0
    @tmp_num  += 1
    result = self.tmp_name_prefix + @tmp_num.to_s
    @tmp_num_mutex.unlock

    result
  end

  def tmp
    @tmp ||= tmp_name
  end

  def hash_file(name)
    @debug_commands.push("md5sum #{name}")
    return `md5sum #{name}`.chomp.gsub(/\s+.*/, "")
  end

  def hash_tmp(erase = true)
    output = hash_file(@tmp)
    if (erase)
      File.unlink(@tmp) if (File.exists?(@tmp) && (ENV["KEEP_TMPFILES"] != "1"))
      @debug_commands.push("rm #{@tmp}")
      @tmp = nil
    end
    return output
  end

  def merge(*args)
    command = "../src/mkvmerge --engage no_variable_data "
    string_args = Array.new
    retcode = 0
    args.each do |a|
      if (a.class == String)
        string_args.push(a)
      else
        retcode = a
      end
    end

    if ((string_args.size == 0) or (string_args.size > 2))
      raise "Wrong use of the 'merge' function."
    elsif (string_args.size == 1)
      command += "-o " + tmp + " " + string_args[0]
    else
      command += "-o " + string_args[0] + " " + string_args[1]
    end
    sys(command, retcode)
  end

  def xtr_tracks_s(*args)
    command = "../src/mkvextract tracks data/mkv/complex.mkv " +
      "--no-variable-data "
    command += args.join(" ")
    command += ":#{tmp}"
    sys(command, 0)
    return hash_tmp
  end

  def xtr_tracks(*args)
    command = "../src/mkvextract tracks "
    command += args[0]
    command += " --no-variable-data "
    command += args[1..args.size - 1].join(" ")
    sys(command, 0)
  end
end

class Results
  def initialize
    load
  end

  def load
    @results = Hash.new
    return unless (FileTest.exist?("results.txt"))
    IO.readlines("results.txt").each do |line|
      parts = line.chomp.split(/:/)
      parts[3] =~
        /([0-9]{4})([0-9]{2})([0-9]{2})-([0-9]{2})([0-9]{2})([0-9]{2})/
      @results[parts[0]] = {"hash" => parts[1], "status" => parts[2],
        "date_added" => Time.local($1, $2, $3, $4, $5, $6) }
    end
  end

  def save
    f = File.new("results.txt", "w")
    @results.keys.sort.each do |key|
      f.puts("#{key}:" + @results[key]['hash'] + ":" +
              @results[key]['status'] + ":" +
              @results[key]['date_added'].strftime("%Y%m%d-%H%M%S"))
    end
    f.close
  end

  def exist?(name)
    return @results.has_key?(name)
  end

  def hash?(name)
    raise "No such result" unless (exist?(name))
    return @results[name]['hash']
  end

  def status?(name)
    raise "No such result" unless (exist?(name))
    return @results[name]['status']
  end

  def date_added?(name)
    raise "No such result" unless (exist?(name))
    return @results[name]['date_added']
  end

  def add(name, hash)
    raise "Test does already exist" if (exist?(name))
    @results[name] = {"hash" => hash, "status" => "new",
      "date_added" => Time.now }
    save
  end

  def set(name, status)
    return unless (exist?(name))
    @results[name]["status"] = status
    save
  end

  def set_hash(name, hash)
    raise "Test does not exist" unless (exist?(name))
    @results[name]["hash"] = hash
    save
  end
end

def main
  ENV['LC_ALL'] = "en_US.UTF-8"
  ENV['PATH']   = "../src:" + ENV['PATH']

  controller = TestController.new

  ARGV.each do |arg|
    if ((arg == "-f") or (arg == "--failed"))
      controller.test_failed = true
    elsif ((arg == "-n") or (arg == "--new"))
      controller.test_new = true
    elsif ((arg == "-u") or (arg == "--update-failed"))
      controller.update_failed = true
    elsif (arg =~ /-d([0-9]{4})([0-9]{2})([0-9]{2})-([0-9]{2})([0-9]{2})/)
      controller.test_date_after = Time.local($1, $2, $3, $4, $5, $6)
    elsif (arg =~ /-D([0-9]{4})([0-9]{2})([0-9]{2})-([0-9]{2})([0-9]{2})/)
      controller.test_date_before = Time.local($1, $2, $3, $4, $5, $6)
    elsif (arg =~ /^[0-9]{3}$/)
      controller.add_test_case arg
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
