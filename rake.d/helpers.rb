def error(message)
  puts message
  exit 1
end

def last_exit_code
  $?.respond_to?(:exitstatus) ? $?.exitstatus : $?.to_i
end

def run(cmdline, opts = {})
  cmdline = cmdline.gsub(/\n/, ' ').gsub(/^\s+/, '').gsub(/\s+$/, '').gsub(/\s+/, ' ')

  puts cmdline unless opts[:dont_echo].to_bool
  system cmdline

  code = last_exit_code
  exit code if (code != 0) && !opts[:allow_failure].to_bool
end

def runq(msg, cmdline, options = {})
  verbose = ENV['V'].to_bool
  puts msg if !verbose
  run cmdline, options.clone.merge(:dont_echo => !verbose)
end

def handle_deps(target, exit_code)
  dep_file = target.ext 'd'
  get_out  = lambda do
    File.unlink(dep_file) if FileTest.exist?(dep_file)
    exit exit_code if 0 != exit_code
    return
  end

  FileTest.exist?(dep_file) || get_out.call

  FileTest.exist?($dependency_dir) && !FileTest.directory?($dependency_dir) && File.unlink($dependency_dir)
  FileTest.exist?($dependency_dir) || Dir.mkdir($dependency_dir)

  File.open("#{$dependency_dir}/" + dep_file.gsub(/\//, '_').ext('rb'), "w") do |out|
    line = IO.readlines(dep_file).collect { |line| line.chomp }.join(" ").gsub(/\\/, ' ').gsub(/\s+/, ' ')
    if /(.*):\s*(.*)/.match(line)
      target  = $1
      sources = $2.gsub(/^\s+/, '').gsub(/\s+$/, '').split(/\s+/)

      out.puts "file \"#{target}\" => [ " + sources.collect { |entry| "\"#{entry}\"" }.join(", ") + " ]"
    end
  end

  get_out.call
rescue Exception => e
  get_out.call
end

def import_dependencies
  Dir.glob("#{$dependency_dir}/*.rb").each { |file| import file } if FileTest.directory?($dependency_dir)
end
