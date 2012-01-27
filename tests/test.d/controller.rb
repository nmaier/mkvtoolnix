class Controller
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
      current_test = constantize class_name
      current_test = current_test.new if current_test.ancestors.include?(Test)
    rescue Exception => ex
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

        expected_results = @results.hash?(class_name).split(/-/)
        actual_results   = result.split(/-/)

        current_test.commands.each_with_index do |command, idx|
          msg += "  " + ((expected_results[idx] != actual_results[idx]) ? "(*)" : "   ") + " #{command}\n"
        end

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
