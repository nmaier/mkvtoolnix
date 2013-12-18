class SimpleTest
  EXIT_CODE_ALIASES = {
    :success => 0,
    :warning => 1,
    :error   => 2,
  }

  def self.instantiate class_name
    file_name = class_name_to_file_name class_name
    content   = IO.readlines(file_name).join("")

    if ! /class\s+.*?\s+<\s+Test/.match(content)
      content = %Q!
        class ::#{class_name} < SimpleTest
          def initialize
            super

            #{content}
          end
        end
!
    else
      content.gsub!(/class\s+/, 'class ::')
    end

    eval content, nil, file_name, 5

    constantize(class_name).new
  end

  def initialize
    @commands      = []
    @tmp_num       = 0
    @tmp_num_mutex = Mutex.new
    @blocks        = {
      :setup       => [],
      :tests       => [],
      :cleanup     => [],
    }
  end

  def commands
    @commands
  end

  def describe description
    @description = description
  end

  def setup &block
    @blocks[:setup] << block
  end

  def cleanup &block
    @blocks[:cleanup] << block
  end

  def tmp_name_prefix
    [ "/tmp/mkvtoolnix-auto-test-#{self.class.name}", $$.to_s, Thread.current[:number] ].join("-") + "-"
  end

  def tmp_name
    @tmp_num_mutex.lock
    @tmp_num ||= 0
    @tmp_num  += 1
    result      = self.tmp_name_prefix + @tmp_num.to_s
    @tmp_num_mutex.unlock

    result
  end

  def tmp
    @tmp ||= tmp_name
  end

  def hash_file name
    md5 name
  end

  def hash_tmp erase = true
    output = hash_file @tmp

    if erase
      File.unlink(@tmp) if File.exists?(@tmp) && (ENV["KEEP_TMPFILES"] != "1")
      @tmp = nil
    end

    output
  end

  def unlink_tmp_files
    return if ENV["KEEP_TMPFILES"] == "1"
    re = /^#{self.tmp_name_prefix}/
    Dir.entries("/tmp").each do |entry|
      file = "/tmp/#{entry}"
      File.unlink(file) if re.match(file) and File.exists?(file)
    end
  end

  def test name, &block
    @blocks[:tests] << { :name => name, :block => block }
  end

  def test_merge file, *args
    options             = args.extract_options!
    full_command_line   = [ options[:args], file ].flatten.join(' ')
    options[:name]    ||= full_command_line
    @blocks[:tests] << {
      :name  => full_command_line,
      :block => lambda {
        output = options[:output] || tmp
        merge full_command_line, :exit_code => options[:exit_code], :output => output
        options[:keep_tmp] ? hash_file(output) : hash_tmp
      },
    }
  end

  def test_identify file, *args
    options             = args.extract_options!
    options[:verbose]   = true if options[:verbose].nil?
    full_command_line   = [ options[:verbose] ? "--identify-verbose" : "--identify", options[:args], file ].flatten.join(' ')
    options[:name]    ||= full_command_line
    @blocks[:tests] << {
      :name  => full_command_line,
      :block => lambda {
        sys "../src/mkvmerge #{full_command_line} > #{tmp}", :exit_code => options[:exit_code]
        if options[:filter]
          text = options[:filter].call(IO.readlines(tmp).join(''))
          File.open(tmp, 'w') { |file| file.puts text }
        end
        options[:keep_tmp] ? hash_file(tmp) : hash_tmp
      },
    }
  end

  def test_merge_unsupported file, *args
    options             = args.extract_options!
    full_command_line   = [ options[:args], file ].flatten.join(' ')
    options[:name]    ||= full_command_line
    @blocks[:tests] << {
      :name  => full_command_line,
      :block => lambda {
        sys "../src/mkvmerge --identify-verbose #{full_command_line} > #{tmp}", :exit_code => 3
        /unsupported container/.match(IO.readlines(tmp).first || '') ? :ok : :bad
      },
    }
  end

  def test_ui_locale locale, *args
    describe "mkvmerge / UI locale: #{locale}"

    options = args.extract_options!
    @blocks[:tests] << {
      :name  => "mkvmerge UI locale #{locale}",
      :block => lambda {
        sys "../src/mkvmerge -o /dev/null --ui-language #{locale} data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-#{locale}"
        result = hash_file "#{tmp}-#{locale}"
        self.error 'Locale #{locale} not supported by MKVToolNix' if result == 'f54ee70a6ad9bfc5f61de5ba5ca5a3c8'
        result
      },
    }
    @blocks[:tests] << {
      :name  => "mkvinfo UI locale #{locale}",
      :block => lambda {
        sys "../src/mkvinfo --ui-language #{locale} data/mkv/complex.mkv | head -n 2 > #{tmp}-#{locale}"
        result = hash_file "#{tmp}-#{locale}"
        self.error 'Locale #{locale} not supported by MKVToolNix' if result == 'f54ee70a6ad9bfc5f61de5ba5ca5a3c8'
        result
      },
    }
  end

  def description
    @description || fail("Class #{self.class.name} misses its description")
  end

  def run_test
    @blocks[:setup].each &:call

    results = @blocks[:tests].collect do |test|
      result = nil
      begin
        result = test[:block].call
      rescue RuntimeError => ex
        show_message "Test case '#{self.class.name}', sub-test '#{test[:name]}': #{ex}"
      end
      result
    end

    @blocks[:cleanup].each &:call

    unlink_tmp_files

    results.join '-'
  end

  def merge *args
    options = args.extract_options!
    fail ArgumentError if args.empty?

    output  = options[:output] || self.tmp
    command = "../src/mkvmerge --engage no_variable_data -o #{output} #{args.first}"
    self.sys command, :exit_code => options[:exit_code]
  end

  def identify *args
    options = args.extract_options!
    fail ArgumentError if args.empty?

    verbose = options[:verbose].nil? ? true : options[:verbose]
    verbose = verbose ? "-verbose" : ""
    command = "../src/mkvmerge --identify#{verbose} --engage no_variable_data #{args.first}"
    self.sys command, :exit_code => options[:exit_code]
  end

  def info *args
    options = args.extract_options!
    fail ArgumentError if args.empty?

    output  = options[:output] || self.tmp
    output  = "> #{output}" unless /^[>\|]/.match(output)
    output  = '' if options[:output] == :return
    command = "../src/mkvinfo --engage no_variable_data --ui-language en_US #{args.first} #{output}"
    self.sys command, :exit_code => options[:exit_code]
  end

  def extract *args
    options = args.extract_options!
    fail ArgumentError if args.empty?

    mode     = options[:mode] || :tracks
    command  = "../src/mkvextract --engage no_variable_data #{mode} #{args.first} " + options.keys.select { |key| key.is_a?(Numeric) }.sort.collect { |key| "#{key}:#{options[key]}" }.join(' ')

    self.sys command, :exit_code => options[:exit_code]
  end

  def propedit *args
    options = args.extract_options!
    fail ArgumentError if args.empty?

    command  = "../src/mkvpropedit --engage no_variable_data #{args.first}"
    self.sys command, :exit_code => options[:exit_code]
  end

  def sys *args
    options             = args.extract_options!
    options[:exit_code] = EXIT_CODE_ALIASES[ options[:exit_code] ] || options[:exit_code] || 0
    fail ArgumentError if args.empty?

    command    = args.shift
    @commands << command

    if !/>/.match command
      temp_file = Tempfile.new('mkvtoolnix-test-output')
      temp_file.close
      command  << " >#{temp_file.path} 2>&1 "
    end

    puts "COMMAND #{command}" if ENV['DEBUG']

    exit_code = 0
    if !system(command)
      exit_code = $? >> 8
      self.error "system command failed: #{command} (#{exit_code})" if options[:exit_code] != exit_code
    end

    return IO.readlines(temp_file.path), exit_code if temp_file
    return exit_code
  end

  def error reason
    show_message "  Failed. Reason: #{reason}"
    raise "test failed"
  end
end
