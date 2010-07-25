#!/usr/bin/env rake
# -*- mode: ruby; -*-

require "pp"

class NilClass
  def to_bool
    false
  end
end

class String
  def to_bool
    %w{1 true yes}.include? self.downcase
  end
end

class TrueClass
  def to_bool
    self
  end
end

class FalseClass
  def to_bool
    self
  end
end

class Target
  def initialize(target)
    @target       = target
    @aliases      = []
    @sources      = []
    @objects      = []
    @libraries    = []
    @dependencies = []
    @only_if      = true
    @debug        = {}
  end

  def debug(category)
    @debug[category] = !@debug[category]
    self
  end

  def only_if(condition)
    @only_if = condition
    self
  end

  def end_if
    only_if true
  end

  def extract_options(list)
    options = list.empty? || !list.last.is_a?(Hash) ? {} : list.pop
    return list, options
  end

  def aliases(*list)
    @aliases += list.compact
    self
  end

  def sources(*list)
    list, options = extract_options list

    if @debug[:sources]
      puts "Target::sources: only_if #{@only_if}; list & options:"
      pp list
      pp options
    end

    return self if !@only_if || (options.include?(:if) && !options[:if])

    list           = list.collect { |e| e.respond_to?(:to_a) ? e.to_a : e }.flatten
    file_mode      = (options[:type] || :file) == :file
    new_sources    = list.collect { |entry| file_mode ? (entry.respond_to?(:to_ia) ? entry.to_a : entry) : FileList["#{entry}/*.c", "#{entry}/*.cpp"].to_a }.flatten
    new_objects    = new_sources.collect { |file| file.ext('o') }
    @sources      += new_sources
    @objects      += new_objects
    @dependencies += new_objects
    self
  end

  def dependencies(*list)
    @dependencies += list.select { |entry| !entry.blank? } if @only_if
    self
  end

  def libraries(*list)
    list, options = extract_options list

    return self if !@only_if || (options.include?(:if) && !options[:if])

    @dependencies += list.collect do |entry|
      case entry
      when :mtxcommon  then "src/common/libmtxcommon." + c(:LIBMTXCOMMONEXT)
      when :mtxinput   then "src/input/libmtxinput.a"
      when :mtxoutput  then "src/output/libmtxoutput.a"
      when :avi        then "lib/avilib-0.6.10/libavi.a"
      when :rmff       then "lib/librmff/librmff.a"
      when :mpegparser then "src/mpegparser/libmpegparser.a"
      else                  nil
      end
    end.compact

    @libraries += list.collect do |entry|
      case entry
      when nil               then nil
      when :magic            then c(:MAGIC_LIBS)
      when :flac             then c(:FLAC_LIBS)
      when :compression      then c(:COMPRESSION_LIBRARIES)
      when :iconv            then c(:ICONV_LIBS)
      when :intl             then c(:LIBINTL_LIBS)
      when :boost_regex      then c(:BOOST_REGEX_LIB)
      when :boost_filesystem then c(:BOOST_FILESYSTEM_LIB)
      when :boost_system     then c(:BOOST_SYSTEM_LIB)
      when :qt               then c(:QT_LIBS)
      when :wxwidgets        then c(:WXWIDGETS_LIBS)
      when String            then entry
      else                        "-l#{entry}"
      end
    end.compact

    self
  end

  def dump
    %w{aliases sources objects dependencies libraries}.each do |type|
      puts "@#{type}:"
      pp instance_variable_get("@#{type}")
    end
    self
  end

  def create
    @aliases.each { |name| task name => @target }
    create_specific
    self
  end
end

class Application < Target
  def initialize(exe)
    super exe + c(:EXEEXT)
  end

  def create_specific
    file @target => @dependencies do |t|
      runq "    LINK #{t.name}", "#{c(:CXX)} #{c(:LDFLAGS)} #{c(:LIBDIRS)} #{$system_libdirs} -o #{t.name} #{@objects.join(" ")} #{@libraries.join(" ")}"
    end
    self
  end
end

class Library < Target
  def initialize(name)
    super name
    @build_dll = false
  end

  def build_dll(build_dll_as_well = true)
    @build_dll = build_dll_as_well
    self
  end

  def create_specific
    file "#{@target}.a" => @objects do |t|
      rm_f t.name
      runq "      AR #{t.name}", "#{c(:AR)} rcu #{t.name} #{@objects.join(" ")}"
      runq "  RANLIB #{t.name}", "#{c(:RANLIB)} #{t.name}"
    end

    return self unless @build_dll

    file "#{@target}.dll" => @objects do |t|
      runq "  LD/DLL #{t.name}", <<-COMMAND
        #{c(:CXX)} #{c(:LDFLAGS)} #{c(:LIBDIRS)} #{$system_libdirs}
           -shared -Wl,--export-all -Wl,--out-implib=#{t.name}.a
           -o #{t.name} #{@objects.join(" ")} #{@libraries.join(" ")}
      COMMAND
    end

    self
  end
end

def error(message)
  puts message
  exit 1
end

def last_exit_code
  $?.respond_to?(:exitstatus) ? $?.exitstatus : $?.to_i
end

def read_config
  error "build-config not found: please run ./configure" unless File.exists?("build-config")

  $config = Hash[ *IO.readlines("build-config").collect { |line| line.chomp.gsub(/#.*/, "") }.select { |line| !line.empty? }.collect do |line|
                     parts = line.split(/\s*=\s*/, 2).collect { |part| part.gsub(/^\s+/, '').gsub(/\s+$/, '') }
                     [ parts[0].to_sym, parts[1] || '' ]
                   end.flatten ]
  $config.default = ''
end

def adjust_config
  if c?(:LIBMTXCOMMONDLL)
    $config[:LIBMTXCOMMONEXT]         = 'dll'
    $config[:CXXFLAGS_SRC_COMMON]    += '-DMTX_DLL_EXPORT'
    $config[:CXXFLAGS_NO_SRC_COMMON] += '-DMTX_DLL'
  else
    $config[:LIBMTXCOMMONEXT]         = 'a'
  end

  $config[:LDFLAGS] += "-Wl,--enable-auto-import" if c?(:MINGW)
  [:CFLAGS, :CXXFLAGS].each do |idx|
    $config[idx] += '-DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\" -DMTX_LOCALE_DIR=\"$(localedir)\" -DMTX_PKG_DATA_DIR=\"$(pkgdatadir)\"'
  end
end

def c(idx)
  idx_s = idx.to_s
  var   = (ENV[idx_s].nil? ? $config[idx.to_sym] : ENV[idx_s]).to_s
  var.gsub(/\$[\({](.*?)[\)}]/) { c($1) }
end

def c?(idx)
  c(idx).to_bool
end

def run(cmdline, opts = {})
  cmdline = cmdline.gsub(/\n/, ' ').gsub(/^\s+/, '').gsub(/\s+$/, '').gsub(/\s+/, ' ')

  puts cmdline unless opts[:dont_echo].to_bool
  system cmdline

  code = last_exit_code
  exit code if (code != 0) && !opts[:allow_failure].to_bool
end

def runq(msg, cmdline)
  verbose = ENV['V'].to_bool
  puts msg if !verbose
  run cmdline, :dont_echo => !verbose
end

# main
read_config
adjust_config

$programs              =  %w{mkvmerge mkvinfo mkvextract mkvpropedit}
$programs              << "mmg" if c?(:USE_WXWIDGETS)

$application_subdirs   =  { "mmg" => "mmg/" }
$applications          =  $programs.collect { |name| "src/#{$application_subdirs[name]}#{name}" + c(:EXEEXT) }
$manpages              =  $programs.collect { |name| "doc/man/#{name}.1" }

$system_includes       = "-I. -Ilib -Ilib/avilib-0.6.10 -Ilib/utf8-cpp/source -Isrc"
$system_libdirs        = "-Llib/avilib-0.6.10 -Llib/librmff -Lsrc/common -Lsrc/input -Lsrc/output -Lsrc/mpegparser"

$target_install_shared =  c?(:USE_WXWIDGETS) ? :install_shared : nil

$source_directories    =  %w{lib/avilib-0.6.10 lib/librmff src src/input src/output src/common src/common/chapters src/common/strings src/common/tags src/common/xml
                             src/mmg src/mmg/header_editor src/mmg/options src/mmg/tabs src/extract src/propedit src/merge src/info src/mpegparser}
$all_sources           =  $source_directories.collect { |dir| FileList[ "#{dir}/*.c", "#{dir}/*.cpp" ].to_a }.flatten
$all_headers           =  $source_directories.collect { |dir| FileList[ "#{dir}/*.h",                ].to_a }.flatten
$all_objects           =  $all_sources.collect { |file| file.ext('o') }

# Default task
task :default => [ $manpages_dep, $tagsfile, $applications, $translations_mos, $htmlhelpbooks ].flatten.compact

# Installation tasks
task :install => [ :install_programs, $target_install_shared, :install_mans, :install_translated_mans, :install_trans, :install_guide ].compact

# Store compiler block for re-use
cxx_compiler = lambda do |t|
  cxxflags = case
             when !c?(:LIBMTXCOMMONDLL)       then ''
             when /src\/common/.match(t.name) then c(:CXXFLAGS_SRC_COMMON)
             else                                  c(:CXXFLAGS_NO_SRC_COMMON)
             end

  runq "     CXX #{t.source}", "#{c(:CXX)} #{c(:CXXFLAGS)} #{c(:INCLUDES)} #{$system_includes} #{cxxflags} -c -MMD -o #{t.name} #{t.prerequisites.join(" ")}"
  run "#{c(:top_srcdir)}/handle_deps #{t.name} #{last_exit_code}", :dont_echo => true
end

# Precompiled headers
if c?(:USE_PRECOMPILED_HEADERS)
  file $all_objects => "src/common/common_pch.h.o"
  file "src/common/common_pch.h.o" => "src/common/common_pch.h", &cxx_compiler
end

# Pattern rules
rule '.o' => '.cpp', &cxx_compiler

rule '.o' => '.c' do |t|
  runq "      CC #{t.source}", "#{c(:CC)} #{c(:CFLAGS)} #{c(:INCLUDES)} #{$system_includes} -c -MMD -o #{t.name} #{t.prerequisites.join(" ")}"
  run "#{c(:top_srcdir)}/handle_deps #{t.name} #{last_exit_code}", :dont_echo => true
end

rule '.o' => '.rc' do |t|
  runq " WINDRES #{t.source}", "#{c(:WINDRES)} #{c(:WXWIDGETS_INCLUDES)} -Isrc/mmg -o #{t.name} #{t.prerequisites.join(" ")}"
end

rule '.mo' => '.po' do |t|
  runq "  MSGFMT #{t.source}", "msgfmt -o #{t.name} #{t.prerequisites.join(" ")}"
end

# HTML help book stuff
rule '.hhk' => '.hhc' do |t|
  runq "    GREP #{t.source}", "#{c(:GREP)} -v 'name=\"ID\"' #{t.prerequisites.join(" ")} > #{t.name}"
end

# man pages from DocBook XML
rule '.1' => '.xml' do |t|
  runq "XSLTPROC #{t.source}", "#{c(:XSLTPROC)} #{c(:XSLTPROC_FLAGS)} -o #{t.name} #{c(:DOCBOOK_MANPAGES_STYLESHEET)} #{t.prerequisites.join(" ")}"
end

# translated DocBook XML
# @MANPAGES_TRANSLATED_XML_RULE@

# Qt files
rule '.h' => '.ui' do |t|
  runq "     UIC #{t.source}", "#{c(:UIC)} #{t.prerequisites.join(" ")} > #{t.name}"
end

rule '.moc.cpp' => '.h' do |t|
  runq "     MOC #{t.source}", "#{c(:MOC)} #{c(:QT_CFLAGS)} #{t.prerequisites.join(" ")} > #{t.name}"
end

# Cleaning tasks
task :clean do
  run <<-SHELL, true
    rm -f *.o */*.o */*/*.o */lib*.a */*/lib*.a po/*.mo #{$applications.join(" ")}
      */*.exe */*/*.exe */*/*.dll */*/*.dll.a doc/*.hhk
      src/info/ui/*.h src/info/*.moc.cpp src/common/*/*.o
      src/mmg/*/*.o src/*/*.gch
  SHELL
end

[:distclean, :dist_clean].each do |name|
  task name => :clean do
    run "rm -f config.h config.log config.cache Makefile */Makefile */*/Makefile TAGS", true
    run "rm -rf .deps", true
  end
end

task :maintainer_clean => :distclean do
  run "rm -f configure config.h.in", true
end

task :clean_libs do
  run "rm -f */lib*.a */*/lib*.a */*/*.dll */*/*.dll.a", true
end

[:clean_apps, :clean_applications, :clean_exe].each do |name|
  task name do
    run "rm -f #{$applications.join(" ")} */*.exe */*/*.exe", true
  end
end

#
# avilib-0.6.10
# librmff
# spyder's MPEG parser
# src/common
# src/input
# src/output
#

[ { :name => 'avi',        :dir => 'lib/avilib-0.6.10'                                                                                },
  { :name => 'rmff',       :dir => 'lib/librmff'                                                                                      },
  { :name => 'mpegparser', :dir => 'src/mpegparser'                                                                                   },
  { :name => 'mtxcommon',  :dir => [ 'src/common', 'src/common/chapters', 'src/common/strings', 'src/common/tags', 'src/common/xml' ] },
  { :name => 'mtxinput',   :dir => 'src/input'                                                                                        },
  { :name => 'mtxoutput',  :dir => 'src/output'                                                                                       },
].each do |lib|
  Library.
    new("#{[ lib[:dir] ].flatten.first}/lib#{lib[:name]}").
    sources(*[ lib[:dir] ].flatten, :type => :dir).
    build_dll(lib[:name] == 'mtxcommon').
    libraries(:iconv, :z, :compression, :matroska, :ebml, :expat, :rpcrt4).
    create
end

#
# mkvmerge
#

Application.new("src/mkvmerge").
  aliases(:mkvmerge).
  sources("src/merge", :type => :dir).
  sources("src/merge/resources.o", :if => c?(:MINGW)).
  libraries(:mtxinput, :mtxoutput, :mtxcommon, :magic, :matroska, :ebml, :avi, :rmff, :mpegparser, :flac, :vorbis, :ogg, :z, :compression, :expat, :iconv, :intl,
             :boost_regex, :boost_filesystem, :boost_system).
  libraries(:rpcrt4, :if => c?(:MINGW)).
  create

#
# mkvinfo
#

$mkvinfo_ui_files = FileList["src/info/ui/*.ui"].to_a
file "src/info/qt_ui.o" => $mkvinfo_ui_files

Application.new("src/mkvinfo").
  aliases(:mkvinfo).
  sources(FileList["src/info/*.cpp"].exclude("src/info/qt_ui.cpp", "src/info/wxwidgets_ui.cpp")).
  sources("src/info/resources.o", :if => c?(:MINGW)).
  libraries(:mtxcommon, :magic, :matroska, :ebml, :boost_regex, :boost_filesystem, :boost_system).
  only_if(c?(:USE_QT)).
  sources("src/info/qt_ui.cpp", "src/info/qt_ui.moc.cpp", "src/info/rightclick_tree_widget.moc.cpp", $mkvinfo_ui_files).
  libraries(:qt).
  only_if(!c?(:USE_QT) && c?(:USE_WXWIDGETS)).
  sources("src/info/wxwidgets_ui.cpp").
  libraries(:wxwidgets).
  create

#
# mkvextract
#

Application.new("src/mkvextract").
  aliases(:mkvextract).
  sources("src/extract", :type => :dir).
  sources("src/extract/resources.o", :if => c?(:MINGW)).
  libraries(:mtxcommon, :magic, :matroska, :ebml, :avi, :rmff, :vorbis, :ogg, :z, :compression, :expat, :iconv, :intl, :boost_regex, :boost_filesystem, :boost_system).
  libraries(:rpcrt4, :if => c?(:MINGW)).
  create

#
# mkvpropedit
#

Application.new("src/mkvpropedit").
  aliases(:mkvpropedit).
  sources("src/propedit", :type => :dir).
  sources("src/propedit/resources.o", :if => c?(:MINGW)).
  libraries(:mtxcommon, :magic, :matroska, :ebml, :avi, :rmff, :vorbis, :ogg, :z, :compression, :expat, :iconv, :intl, :boost_regex, :boost_filesystem, :boost_system).
  libraries(:rpcrt4, :if => c?(:MINGW)).
  create

#
# mmg
#

if c?(:USE_WXWIDGETS)
  Application.new("src/mmg/mmg").
    aliases(:mmg).
    sources("src/mmg", "src/mmg/header_editor", "src/mmg/options", "src/mmg/tabs", :type => :dir).
    sources("src/propedit/mmg-resources.o", :if => c?(:MINGW)).
    libraries(:mtxcommon, :magic, :matroska, :ebml, :avi, :rmff, :vorbis, :ogg, :z, :compression, :expat, :iconv, :intl, :wxwidgets,
               :boost_regex, :boost_filesystem, :boost_system).
    libraries(:rpcrt4, :ole32, :shell32, "-mwindows", :if => c?(:MINGW)).
    create
end
