#!/usr/bin/ruby

class SubtestError < RuntimeError
end

class Test
  attr_reader :commands
  attr_reader :debug_commands

  def initialize
    @tmp_num = 0
    @commands = Array.new
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
    n = "mkvtoolnix-auto-test-" + $$.to_s + "-"
    Dir.entries("/tmp").each do |e|
      File.unlink("/tmp/#{e}") if ((e =~ /^#{n}/) and
                                    File.exists?("/tmp/#{e}"))
    end
  end

  def run_test
    begin
      return run
    rescue RuntimeError => ex
      unlink_tmp_files
      puts(ex.to_s)
      return nil
    end
  end

  def error(reason)
    puts("  Failed. Reason: #{reason}")
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

  def tmp_name
    @tmp_num ||= 0
    @tmp_num += 1
    return "/tmp/mkvtoolnix-auto-test-" + $$.to_s + "-" + @tmp_num.to_s
  end

  def tmp
    @tmp ||= tmp_name
    return @tmp
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

  results = Results.new

  test_failed = false
  test_new = false
  test_date_after = nil
  test_date_before = nil
  update_failed = false
  tests = Array.new
  dir_entries = Dir.entries(".")
  ARGV.each do |arg|
    if ((arg == "-f") or (arg == "--failed"))
      test_failed = true
    elsif ((arg == "-n") or (arg == "--new"))
      test_new = true
    elsif ((arg == "-u") or (arg == "--update-failed"))
      update_failed = true
    elsif (arg =~ /-d([0-9]{4})([0-9]{2})([0-9]{2})-([0-9]{2})([0-9]{2})/)
      test_date_after = Time.local($1, $2, $3, $4, $5, $6)
    elsif (arg =~ /-D([0-9]{4})([0-9]{2})([0-9]{2})-([0-9]{2})([0-9]{2})/)
      test_date_before = Time.local($1, $2, $3, $4, $5, $6)
    elsif (arg =~ /^[0-9]{3}$/)
      dir_entries.each { |e| tests.push(e) if (e =~ /^test-#{arg}/) }
    else
      puts("Unknown argument '#{arg}'.")
      exit(2)
    end
  end
  test_all = (!test_failed && !test_new)
  tests = dir_entries unless (tests.size > 0)

  ENV['PATH'] = "../src:" + ENV['PATH']

  if (ENV["DEBUG"] == "1")
    $stderr.puts("#!/bin/bash\n\n")
    $stderr.puts("export LD_LIBRARY_PATH=/usr/local/lib\n\n")
    $stderr.puts("rm new_results.txt &> /dev/null")
    $stderr.puts("touch new_results.txt\n\n")
  end

  num_tests = 0
  num_failed = 0
  start = Time.now
  tests.sort.each do |entry|
    next unless (FileTest.file?(entry) and (entry =~ /^test-.*\.rb$/))

    class_name = "T_" + entry.gsub(/^test-/, "").gsub(/\.rb$/, "")
    test_this = test_all
    if (results.exist?(class_name))
      if (test_failed and (results.status?(class_name) == "failed"))
        test_this = true
      elsif (test_new and (results.status?(class_name) == "new"))
        test_this = true
      end
    elsif (test_new || test_failed)
      test_this = true
    end
    if (results.exist?(class_name))
      if (test_date_after and
         (results.date_added?(class_name) < test_date_after))
        test_this = false
      elsif (test_date_before and
            (results.date_added?(class_name) > test_date_before))
        test_this = false
      end
    end
    next unless (test_this)

    num_tests += 1

    if (!require("./" + entry))
      puts(" Failed to load '#{entry}'.")
      results.set(class_name, "failed")
      num_failed += 1
      next
    end

    begin
      current_test = eval(class_name + ".new")
    rescue
      puts(" Failed to create an instance of class '#{class_name}'.")
      results.set(class_name, "failed")
      num_failed += 1
      next
    end

    if (current_test.description == "INSERT DESCRIPTION")
      puts("Skipping '#{class_name}': Not implemented yet")
      next
    end
    puts("Running '#{class_name}': #{current_test.description}")
    result = current_test.run_test
    if (result)
      if (!results.exist?(class_name))
        puts("  NEW test. Storing result '#{result}'.")
        results.add(class_name, result)
      elsif (results.hash?(class_name) != result)
        puts("  FAILED: checksum is different. Commands:")
        puts("    " + current_test.commands.join("\n    "))
        if (update_failed)
          puts("  UPDATING result")
          results.set_hash(class_name, result)
          results.set(class_name, "passed")
        else
          results.set(class_name, "failed")
          num_failed += 1
        end
      else
        results.set(class_name, "passed")
      end
    else
      puts("  FAILED: no result from test")
      results.set(class_name, "failed")
      num_failed += 1
    end

    if (ENV["DEBUG"] == "1")
      $stderr.puts("echo -n Running #{class_name}")
      $stderr.puts("FAILED=0")
      $stderr.puts("echo -n #{class_name}: >> new_results.txt")
      first = true
      current_test.debug_commands.each do |c|
        c.gsub!(/\/tmp\//, "$HOME/tmp/")
        if (c =~ /^md5sum/)
          $stderr.puts("echo -n - >> new_results.txt") unless (first)
          first = false
          $stderr.puts("SUM=`" + c + " | gawk '{print $1}'`")
          $stderr.puts("echo -n $SUM >> new_results.txt")
        else
          if (c =~ />/)
            $stderr.puts(c)
          else
            $stderr.puts(c + " >/dev/null 2>/dev/null")
          end
          $stderr.puts("if [ $? -ne 0 -a $? -ne 1 ]; then")
          $stderr.puts("  FAILED=1")
          $stderr.puts("fi")
        end
      end
      da = results.date_added?(class_name).strftime("%Y%m%d-%H%M%S")
      $stderr.puts("if [ $FAILED -eq 1 ]; then")
      $stderr.puts("  echo :failed:#{da} >> new_results.txt")
      $stderr.puts("  echo '  FAILED'")
      $stderr.puts("else")
      $stderr.puts("  echo :passed:#{da} >> new_results.txt")
      $stderr.puts("  echo '  ok'")
      $stderr.puts("fi")
      $stderr.puts("rm ~/tmp/mkvtoolnix-auto-test-* >/dev/null 2>/dev/null\n\n")
    end

  end
  duration = Time.now - start

  puts("#{num_failed}/#{num_tests} failed (" +
      (num_tests > 0 ? (num_failed * 100 / num_tests).to_s : "0") + "%). " +
        "Tests took #{duration}s.")

  exit(num_failed > 0 ? 1 : 0)
end

main
