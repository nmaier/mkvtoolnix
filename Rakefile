#!/usr/bin/env rake
# -*- mode: ruby; -*-

require "pp"

require "rake.d/extensions"
require "rake.d/config"
require "rake.d/helpers"
require "rake.d/target"
require "rake.d/application"
require "rake.d/library"

def setup_globals
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

  $dependency_dir        = "#{c(:top_srcdir)}/rake.d/dependecy.d"
end

# main
read_config
adjust_config
setup_globals
import_dependencies

# Default task
desc "Build all applications"
task :default => [ $manpages_dep, $tagsfile, $applications, $translations_mos, $htmlhelpbooks ].flatten.compact

# Installation tasks
desc "Install all applications and support files"
task :install => [ :install_programs, $target_install_shared, :install_mans, :install_translated_mans, :install_trans, :install_guide ].compact

# Store compiler block for re-use
cxx_compiler = lambda do |t|
  cxxflags = case
             when !c?(:LIBMTXCOMMONDLL)       then ''
             when /src\/common/.match(t.name) then c(:CXXFLAGS_SRC_COMMON)
             else                                  c(:CXXFLAGS_NO_SRC_COMMON)
             end

  if t.sources.empty?
    puts "EMPTY SOURCES! #{t.name}"
    pp t
    exit 1
  end

  runq "     CXX #{t.source}", "#{c(:CXX)} #{c(:CXXFLAGS)} #{c(:INCLUDES)} #{$system_includes} #{cxxflags} -c -MMD -o #{t.name} #{t.sources.join(" ")}", :allow_failure => true
  handle_deps t.name, last_exit_code
end

# Precompiled headers
if c?(:USE_PRECOMPILED_HEADERS)
  file $all_objects => "src/common/common_pch.h.o"
  file "src/common/common_pch.h.o" => "src/common/common_pch.h", &cxx_compiler
end

# Pattern rules
rule '.o' => '.cpp', &cxx_compiler

rule '.o' => '.c' do |t|
  runq "      CC #{t.source}", "#{c(:CC)} #{c(:CFLAGS)} #{c(:INCLUDES)} #{$system_includes} -c -MMD -o #{t.name} #{t.sources.join(" ")}", :allow_failure => true
  handle_deps t.name, last_exit_code
end

rule '.o' => '.rc' do |t|
  runq " WINDRES #{t.source}", "#{c(:WINDRES)} #{c(:WXWIDGETS_INCLUDES)} -Isrc/mmg -o #{t.name} #{t.sources.join(" ")}"
end

rule '.mo' => '.po' do |t|
  runq "  MSGFMT #{t.source}", "msgfmt -o #{t.name} #{t.sources.join(" ")}"
end

# HTML help book stuff
rule '.hhk' => '.hhc' do |t|
  runq "    GREP #{t.source}", "#{c(:GREP)} -v 'name=\"ID\"' #{t.sources.join(" ")} > #{t.name}"
end

# man pages from DocBook XML
rule '.1' => '.xml' do |t|
  runq "XSLTPROC #{t.source}", "#{c(:XSLTPROC)} #{c(:XSLTPROC_FLAGS)} -o #{t.name} #{c(:DOCBOOK_MANPAGES_STYLESHEET)} #{t.sources.join(" ")}"
end

# translated DocBook XML
# @MANPAGES_TRANSLATED_XML_RULE@

# Qt files
rule '.h' => '.ui' do |t|
  runq "     UIC #{t.source}", "#{c(:UIC)} #{t.sources.join(" ")} > #{t.name}"
end

rule '.moc.cpp' => '.h' do |t|
  runq "     MOC #{t.source}", "#{c(:MOC)} #{c(:QT_CFLAGS)} #{t.sources.join(" ")} > #{t.name}"
end

# Cleaning tasks
task :clean do
  run <<-SHELL, :allow_failure => true
    rm -f *.o */*.o */*/*.o */lib*.a */*/lib*.a po/*.mo #{$applications.join(" ")}
      */*.exe */*/*.exe */*/*.dll */*/*.dll.a doc/*.hhk
      src/info/ui/*.h src/info/*.moc.cpp src/common/*/*.o
      src/mmg/*/*.o
  SHELL
end

[:distclean, :dist_clean].each do |name|
  task name => :clean do
    run "rm -f config.h config.log config.cache build-config Makefile */Makefile */*/Makefile TAGS", :allow_failure => true
    run "rm -rf #{$dependency_dir}", :allow_failure => true
  end
end

task :maintainer_clean => :distclean do
  run "rm -f configure config.h.in", :allow_failure => true
end

task :clean_libs do
  run "rm -f */lib*.a */*/lib*.a */*/*.dll */*/*.dll.a", :allow_failure => true
end

[:clean_apps, :clean_applications, :clean_exe].each do |name|
  task name do
    run "rm -f #{$applications.join(" ")} */*.exe */*/*.exe", :allow_failure => true
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
