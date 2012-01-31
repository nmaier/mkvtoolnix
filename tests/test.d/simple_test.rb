class SimpleTest
  @@blocks   = {
    :setup   => [],
    :tests   => [],
    :cleanup => [],
  }

  @@commands       = []
  @@tmp_num        = 0
  @@tmp_num_mutex  = Mutex.new

  def self.commands
    @@commands
  end

  def self.describe description
    @@description = description
  end

  def self.setup &block
    @@blocks[:setup] << block
  end

  def self.cleanup &block
    @@blocks[:cleanup] << block
  end

  def self.tmp_name_prefix
    [ "/tmp/mkvtoolnix-auto-test", $$.to_s, Thread.current[:number] ].join("-") + "-"
  end

  def self.tmp_name
    @@tmp_num_mutex.lock
    @@tmp_num ||= 0
    @@tmp_num  += 1
    result      = self.tmp_name_prefix + @@tmp_num.to_s
    @@tmp_num_mutex.unlock

    result
  end

  def self.tmp
    @@tmp ||= tmp_name
  end

  def self.hash_file name
    md5 name
  end

  def self.hash_tmp erase = true
    output = hash_file @@tmp

    if erase
      File.unlink(@@tmp) if File.exists?(@@tmp) && (ENV["KEEP_TMPFILES"] != "1")
      @@tmp = nil
    end

    output
  end

  def self.unlink_tmp_files
    return if ENV["KEEP_TMPFILES"] == "1"
    re = /^#{self.tmp_name_prefix}/
    Dir.entries("/tmp").each do |entry|
      file = "/tmp/#{entry}"
      File.unlink(file) if re.match(file) and File.exists?(file)
    end
  end

  def self.test name, &block
    @@blocks[:tests] << { :name => name, :block => block }
  end

  def self.test_merge file, *args
    options             = args.extract_options!
    full_command_line   = [ options[:args], file ].flatten.join(' ')
    options[:name]    ||= full_command_line
    @@blocks[:tests] << {
      :name  => full_command_line,
      :block => lambda {
        merge file, :exit_code => options[:exit_code]
        hash_tmp
      },
    }
  end

  def self.description
    @@description || fail("Class #{self.class.name} misses its description")
  end

  def self.run_test
    @@blocks[:setup].each &:call

    results = @@blocks[:tests].collect do |test|
      result = nil
      begin
        result = test[:block].call
      rescue RuntimeError => ex
        show_message "Test case '#{self.name}', sub-test '#{test[:name]}': #{ex}"
      end
      unlink_tmp_files
      result
    end

    @@blocks[:cleanup].each &:call

    results.join '-'
  end

  def self.merge *args
    options = args.extract_options!
    fail ArgumentError if args.empty?

    output  = options[:output] || self.tmp
    command = "../src/mkvmerge --engage no_variable_data -o #{output} #{args.first}"
    self.sys command, :exit_code => options[:exit_code]
  end

  @@exit_code_aliases = {
    :success => 0,
    :warning => 1,
    :error   => 2,
  }

  def self.sys *args
    options             = args.extract_options!
    options[:exit_code] = @@exit_code_aliases[ options[:exit_code] ] || options[:exit_code] || 0
    fail ArgumentError if args.empty?

    command     = args.shift
    @@commands << command
    command    << " >/dev/null 2>/dev/null " unless />/.match(command)

    self.error "system command failed: #{command} (#{$? >> 8})" if !system(command) && (options[:exit_code] != ($? >> 8))
  end

  def self.error reason
    show_message "  Failed. Reason: #{reason}"
    raise "test failed"
  end
end
