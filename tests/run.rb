#!/usr/bin/ruby

class Test
  attr_reader :commands

  def initialize
    @tmp_num = 0
    @commands = Array.new
  end

  def description
    return "dummy test class"
  end

  def run
    return error("Trying to run the base class")
  end

  def run_test
    begin
      return run
    rescue Class => ex
      n = "mkvtoolnix-auto-test-" + $$.to_s + "-"
      Dir.entries("/tmp").each do |e|
        File.unlink("/tmp/#{e}") if (e =~ /^#{n}/)
      end
      p(ex)
      return nil
    end
  end

  def error(reason)
    puts("  Failed. Reason: #{reason}")
    raise "test failed"
  end

  def sys(command, *arg)
    @commands.push(command)
    if (!system(command + " &> /dev/null"))
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
    return `md5sum #{name}`.chomp.gsub(/\s+.*/, "")
  end

  def hash_tmp(erase = true)
    output = hash_file(@tmp)
    if (erase)
      File.unlink(@tmp)
      @tmp = nil
    end
    return output
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

def main
  ENV['LC_ALL'] = "en_US.ISO-8859-1"

  results = Results.new

  test_failed = false
  test_new = false
  test_date_after = nil
  test_date_before = nil
  tests = Array.new
  dir_entries = Dir.entries(".")
  ARGV.each do |arg|
    if ((arg == "-f") or (arg == "--failed"))
      test_failed = true
    elsif ((arg == "-n") or (arg == "--new"))
      test_new = true
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
        results.set(class_name, "failed")
        num_failed += 1
      else
        results.set(class_name, "passed")
      end
    else
      puts("  FAILED: no result from test")
      results.set(class_name, "failed")
      num_failed += 1
    end
  end
  duration = Time.now - start

  puts("#{num_failed}/#{num_tests} failed (" +
      (num_tests > 0 ? (num_failed * 100 / num_tests).to_s : "0") + "%). " +
        "Tests took #{duration}s.")

  exit(num_failed > 0 ? 1 : 0)
end

main
