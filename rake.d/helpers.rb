$use_tempfile_for_run = defined?(RUBY_PLATFORM) && /mingw/i.match(RUBY_PLATFORM)
require "tempfile"
require "fileutils"

if defined? Mutex
  $message_mutex = Mutex.new
  def puts(message)
    $message_mutex.lock
    $stdout.puts message
    $stdout.flush
    $message_mutex.unlock
  end
end

def error(message)
  puts message
  exit 1
end

def last_exit_code
  $?.respond_to?(:exitstatus) ? $?.exitstatus : $?.to_i
end

def run cmdline, opts = {}
  code = run_wrapper cmdline, opts
  exit code if (code != 0) && !opts[:allow_failure].to_bool
end

def run_wrapper cmdline, opts = {}
  cmdline = cmdline.gsub(/\n/, ' ').gsub(/^\s+/, '').gsub(/\s+$/, '').gsub(/\s+/, ' ')
  code    = nil
  shell   = ENV["RUBYSHELL"].blank? ? "c:/msys/bin/sh" : ENV["RUBYSHELL"]

  puts cmdline unless opts[:dont_echo].to_bool

  output = nil

  if opts[:filter_output]
    output   = Tempfile.new("mkvtoolnix-rake-output")
    cmdline += " > #{output.path} 2>&1"
  end

  if $use_tempfile_for_run
    Tempfile.open("mkvtoolnix-rake-run") do |t|
      t.puts cmdline
      t.flush
      system shell, t.path
      code = last_exit_code
      t.unlink
    end
  else
    system cmdline
    code = last_exit_code
  end

  code = opts[:filter_output].call code, output.readlines if opts[:filter_output]

  return code

ensure
  if output
    output.close
    output.unlink
  end
end

def runq(msg, cmdline, options = {})
  verbose = ENV['V'].to_bool
  puts msg if !verbose
  run cmdline, options.clone.merge(:dont_echo => !verbose)
end

def create_dependency_dirs
  [ $dependency_dir, $dependency_tmp_dir ].each do |dir|
    File.unlink(dir) if  FileTest.exist?(dir) && !FileTest.directory?(dir)
    Dir.mkdir(dir)   if !FileTest.exist?(dir)
  end
end

def dependency_output_name_for file_name
  $dependency_tmp_dir + "/" + file_name.gsub(/[\/\.]/, '_') + '.d'
end

def handle_deps(target, exit_code, skip_abspath=false)
  dep_file = dependency_output_name_for target
  get_out  = lambda do
    File.unlink(dep_file) if FileTest.exist?(dep_file)
    exit exit_code if 0 != exit_code
    return
  end

  FileTest.exist?(dep_file) || get_out.call

  create_dependency_dirs

  File.open("#{$dependency_dir}/" + target.gsub(/[\/\.]/, '_') + '.dep', "w") do |out|
    line = IO.readlines(dep_file).collect { |line| line.chomp }.join(" ").gsub(/\\/, ' ').gsub(/\s+/, ' ')
    if /(.+?):\s*([^\s].*)/.match(line)
      target  = $1
      sources = $2.gsub(/^\s+/, '').gsub(/\s+$/, '').split(/\s+/)

      if skip_abspath
        sources.delete_if { |entry| entry.start_with? '/' }
      end

      out.puts(([ target ] + sources).join("\n"))
    end
  end

  get_out.call
rescue Exception => e
  get_out.call
end

def import_dependencies
  return unless FileTest.directory? $dependency_dir
  Dir.glob("#{$dependency_dir}/*.dep").each do |file_name|
    lines  = IO.readlines(file_name).collect &:chomp
    target = lines.shift
    file target => lines.select { |dep_name| File.exists? dep_name }
  end
end

def arrayify(*args)
  args.collect { |arg| arg.is_a?(Array) ? arg.to_a : arg }.flatten
end

def install_dir(*dirs)
  arrayify(*dirs).each do |dir|
    dir = c(dir) if dir.is_a? Symbol
    run "#{c(:mkinstalldirs)} #{c(:DESTDIR)}#{dir}"
  end
end

def install_program(destination, *files)
  destination = c(destination) + '/' if destination.is_a? Symbol
  arrayify(*files).each do |file|
    run "#{c(:INSTALL_PROGRAM)} #{file} #{c(:DESTDIR)}#{destination}"
  end
end

def install_data(destination, *files)
  destination = c(destination) + '/' if destination.is_a? Symbol
  arrayify(*files).each do |file|
    run "#{c(:INSTALL_DATA)} #{file} #{c(:DESTDIR)}#{destination}"
  end
end

def adjust_to_poedit_style(in_name, out_name)
  File.open(out_name, "w") do |out|
    lines = IO.readlines(in_name).collect { |line| line.chomp.gsub(/\r/, '') }

    no_nl          = false
    state          = :initial
    previous_state = :initial

    lines.each do |line|
      previous_state = state

      if '' == line
        state = :blank
      elsif /^#~/.match(line)
        state = :removed
      end

      out.puts if /^#(?:,|~\s+msgid)/.match(line) && (:removed == state) && (:blank != previous_state)

      if /^#:/.match(line)
        out.puts line.gsub(/(\d) /, '\1' + "\n#: ")
      elsif /^#~/.match(line)
        # no_nl = true
        out.puts line
      elsif !(no_nl && /^\s*$/.match(line))
        out.puts line
      end
    end

    out.puts
  end

  File.unlink in_name
end
