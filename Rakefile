#!/usr/bin/env rake
# -*- mode: ruby; -*-

# Change to base directory before doing anything
if FileUtils.pwd != File.dirname(__FILE__)
  new_dir = File.absolute_path(File.dirname(__FILE__))
  puts "Entering directory `#{new_dir}'"
  Dir.chdir new_dir
end

# Set number of threads to use if it is unset and we're running with
# drake
if Rake.application.options.respond_to?(:threads) && [nil, 0, 1].include?(Rake.application.options.threads) && !ENV['DRAKETHREADS'].nil?
  Rake.application.options.threads = ENV['DRAKETHREADS'].to_i
end

# Ruby 1.9.x introduce "require_relative" for local requires. 1.9.2
# removes "." from $: and forces us to use "require_relative". 1.8.x
# does not know "require_relative" yet though.
begin
  require_relative()
rescue NoMethodError
  def require_relative *args
    require *args
  end
rescue Exception
end

require "pp"

require_relative "rake.d/extensions"
require_relative "rake.d/config"
require_relative "rake.d/helpers"
require_relative "rake.d/target"
require_relative "rake.d/application"
require_relative "rake.d/library"

def setup_globals
  $programs                =  %w{mkvmerge mkvinfo mkvextract mkvpropedit}
  $programs                << "mmg" if c?(:USE_WXWIDGETS)
  $tools                   =  %w{base64tool diracparser ebml_validator vc1parser}
  $mmg_bin                 =  c(:MMG_BIN)
  $mmg_bin                 =  "mmg" if $mmg_bin.empty?

  $application_subdirs     =  { "mmg" => "mmg/" }
  $applications            =  $programs.collect { |name| "src/#{$application_subdirs[name]}#{name}" + c(:EXEEXT) }
  $manpages                =  $programs.collect { |name| "doc/man/#{name}.1" }

  $system_includes         = "-I. -Ilib -Ilib/avilib-0.6.10 -Ilib/utf8-cpp/source -Isrc"
  $system_libdirs          = "-Llib/avilib-0.6.10 -Llib/librmff -Lsrc/common -Lsrc/input -Lsrc/output -Lsrc/mpegparser"

  $source_directories      =  %w{lib/avilib-0.6.10 lib/librmff src src/input src/output src/common src/common/chapters src/common/strings src/common/tags src/common/xml
                                 src/mmg src/mmg/header_editor src/mmg/options src/mmg/tabs src/extract src/propedit src/merge src/info src/mpegparser}
  $all_sources             =  $source_directories.collect { |dir| FileList[ "#{dir}/*.c", "#{dir}/*.cpp" ].to_a }.flatten
  $all_headers             =  $source_directories.collect { |dir| FileList[ "#{dir}/*.h",                ].to_a }.flatten
  $all_objects             =  $all_sources.collect { |file| file.ext('o') }

  $top_srcdir              = c(:top_srcdir)
  $dependency_dir          = "#{$top_srcdir}/rake.d/dependecy.d"

  $languages               =  {
    :applications          => c(:TRANSLATIONS).split(/\s+/),
    :manpages              => c(:MANPAGES_TRANSLATIONS).split(/\s+/),
    :guides                => c(:GUIDE_TRANSLATIONS).split(/\s+/),
  }

  $translations            =  {
    :applications          =>                         $languages[:applications].collect { |language| "po/#{language}.mo" },
    :guides                =>                         $languages[:guides].collect       { |language| "doc/guide/#{language}/mkvmerge-gui.hhk" },
    :manpages              => !c?(:PO4A_WORKS) ? [] : $languages[:manpages].collect     { |language| $manpages.collect { |manpage| manpage.gsub(/man\//, "man/#{language}/") } }.flatten,
  }

  $available_languages     =  {
    :applications          => FileList[ "#{$top_srcdir }/po/*.po"                       ].collect { |name| File.basename name, '.po'        },
    :manpages              => FileList[ "#{$top_srcdir }/doc/man/po4a/po/*.po"          ].collect { |name| File.basename name, '.po'        },
    :guides                => FileList[ "#{$top_srcdir }/doc/guide/*/mkvmerge-gui.html" ].collect { |name| File.basename File.dirname(name) },
  }

  $build_tools           ||=  c?(:TOOLS)

  cflags_common            = "-Wall -Wno-comment #{c(:OPTIMIZATION_CFLAGS)} -D_FILE_OFFSET_BITS=64 #{c(:MATROSKA_CFLAGS)} #{c(:EBML_CFLAGS)} #{c(:EXTRA_CFLAGS)} #{c(:DEBUG_CFLAGS)} #{c(:PROFILING_CFLAGS)} #{c(:USER_CPPFLAGS)} -DPACKAGE=\\\"#{c(:PACKAGE)}\\\" -DVERSION=\\\"#{c(:VERSION)}\\\" -DMTX_LOCALE_DIR=\\\"#{c(:localedir)}\\\" -DMTX_PKG_DATA_DIR=\\\"#{c(:pkgdatadir)}\\\" -DMTX_DOC_DIR=\\\"#{c(:docdir)}\\\""
  ldflags_extra            = c?(:MINGW) ? '' : "-Wl,--enable-auto-import"
  $flags                   = {
    :cflags                => "#{cflags_common} #{c(:USER_CFLAGS)}",
    :cxxflags              => "#{cflags_common} #{c(:WXWIDGETS_CFLAGS)} #{c(:QT_CFLAGS)} #{c(:BOOST_CPPFLAGS)} #{c(:CURL_CFLAGS)} #{c(:USER_CXXFLAGS)}",
    :cppflags              => "#{c(:USER_CPPFLAGS)}",
    :ldflags               => "#{c(:EBML_LDFLAGS)} #{c(:MATROSKA_LDFLAGS)} #{c(:EXTRA_LDFLAGS)} #{c(:PROFILING_LIBS)} #{c(:USER_LDFLAGS)} #{c(:LDFLAGS_RPATHS)} #{c(:BOOST_LDFLAGS)}",
  }
end

def define_default_task
  desc "Build everything"

  # The applications themselves
  targets = $applications.clone

  # Build the stuff in the 'src/tools' directory only if requested
  targets << "apps:tools" if $build_tools

  # The tags file -- but only if it exists already
  targets << "TAGS" if File.exist? "TAGS"

  # Build man pages and translations?
  targets += [ "manpages", "translations:manpages" ] if c?(:XSLTPROC_WORKS)

  # Build translations for the programs
  targets << "translations:applications"

  # The GUI help
  targets << "translations:guides"

  task :default => targets do
    puts "Done. Enjoy :)"
  end
end

# main
read_config
adjust_config
setup_globals
import_dependencies

# Default task
define_default_task

desc "Build all applications"
task :apps => $applications

# Store compiler block for re-use
cxx_compiler = lambda do |t|
  cxxflags = case
             when !c?(:LIBMTXCOMMONDLL)       then ''
             when /src\/common/.match(t.name) then c(:CXXFLAGS_SRC_COMMON)
             else                                  c(:CXXFLAGS_NO_SRC_COMMON)
             end

  # t.sources is empty for a 'file' task (common_pch.h.o).
  sources = t.sources.empty? ? [ t.prerequisites.first ] : t.sources

  runq "     CXX #{sources.first}", "#{c(:CXX)} #{$flags[:cxxflags]} #{$system_includes} #{cxxflags} -c -MMD -o #{t.name} #{sources.join(" ")}", :allow_failure => true
  handle_deps t.name, last_exit_code
end

# Precompiled headers
if c?(:USE_PRECOMPILED_HEADERS)
  $all_objects.each { |name| file name => "src/common/common.h.gch" }
  file "src/common/common.h.gch" => "src/common/common.h", &cxx_compiler
end

# Pattern rules
rule '.o' => '.cpp', &cxx_compiler

rule '.o' => '.c' do |t|
  runq "      CC #{t.source}", "#{c(:CC)} #{$flags[:cflags]} #{$system_includes} -c -MMD -o #{t.name} #{t.sources.join(" ")}", :allow_failure => true
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
if c?(:XSLTPROC_WORKS)
  rule '.1' => '.xml' do |t|
    runq "XSLTPROC #{t.source}", "#{c(:XSLTPROC)} #{c(:XSLTPROC_FLAGS)} -o #{t.name} #{c(:DOCBOOK_MANPAGES_STYLESHEET)} #{t.sources.join(" ")}"
  end

  $manpages.each do |manpage|
    file manpage => manpage.ext('xml')
    $available_languages[:manpages].each do |language|
      localized_manpage = manpage.gsub(/.*\//, "doc/man/#{language}/")
      file localized_manpage => localized_manpage.ext('xml')
    end
  end
end

# Qt files
rule '.h' => '.ui' do |t|
  runq "     UIC #{t.source}", "#{c(:UIC)} #{t.sources.join(" ")} > #{t.name}"
end

# Rake does not support rules like '.moc.cpp' => '.h'. Therefore
# Target creats 'file' entries for all .moc.cpp entries submitted via
# Target#sources.
$moc_compiler = lambda do |t|
  runq "     MOC #{t.prerequisites.first}", "#{c(:MOC)} #{c(:QT_CFLAGS)} #{t.prerequisites.join(" ")} > #{t.name}"
end

# Tag files
desc "Create tags file for Emacs"
task :tags => "TAGS"

file "TAGS" => $all_sources do |t|
  runq '   ETAGS', "#{c(:ETAGS)} -o #{t.name} #{t.prerequisites.join(" ")}"
end

task :manpages => $manpages

# Translations for the programs
namespace :translations do
  desc "Create a template for translating the programs"
  task :pot => "po/mkvtoolnix.pot"
  file "po/mkvtoolnix.pot" => $all_sources + $all_headers do |t|
    runq 'XGETTEXT', <<-COMMAND
      xgettext --keyword=YT --keyword=Y --keyword=Z --keyword=TIP --default-domain=mkvtoolnix --from-code=UTF-8 -s --omit-header -o #{t.name} #{t.prerequisites.join(" ")}
    COMMAND
  end

  desc "Verify format strings in translations"
  task "verify-format-strings" do
    files = $available_languages[:applications].collect { |language| "po/#{language}.po" }.join(' ')
    runq 'VERIFY', <<-COMMAND
      ./src/scripts/verify_format_srings_in_translations.rb #{files}
    COMMAND
  end

  [ :applications, :manpages, :guides ].each { |type| task type => $translations[type] }

  $available_languages[:manpages].each do |language|
    $manpages.each do |manpage|
      name = manpage.gsub(/man\//, "man/#{language}/")
      file name            => [ name.ext('xml'),     "doc/man/po4a/po/#{language}.po" ]
      file name.ext('xml') => [ manpage.ext('.xml'), "doc/man/po4a/po/#{language}.po" ] do |t|
	runq "    PO4A #{manpage.ext('.xml')} (#{language})", "#{c(:PO4A_TRANSLATE)} #{c(:PO4A_TRANSLATE_FLAGS)} -m #{manpage.ext('.xml')} -p doc/man/po4a/po/#{language}.po -l #{t.name}"
      end
    end
  end

  desc "Update all translation files"
  task :update => [ "translations:update:applications", "translations:update:manpages" ]

  namespace :update do
    desc "Update the program's translation files"
    task :applications => [ "po/mkvtoolnix.pot", ] + $available_languages[:applications].collect { |language| "translations:update:applications:#{language}" }

    namespace :applications do
      $available_languages[:applications].each do |language|
        task language => "po/mkvtoolnix.pot" do |t|
          po       = "po/#{language}.po"
          tmp_file = "#{po}.new"
          runq "MSGMERGE #{po}", "msgmerge -q -s --no-wrap #{po} po/mkvtoolnix.pot > #{tmp_file}", :allow_failure => true

          exit_code = last_exit_code
          if 0 != exit_code
            File.unlink tmp_file
            exit exit_code
          end

          if %w{es nl ru uk zh_CN zh_TW}.include? language
            adjust_to_poedit_style tmp_file, po
          else
            FileUtils.mv tmp_file, po
          end
        end
      end
    end

    desc "Update the man pages' translation files"
    task :manpages do
      runq "    PO4A doc/man/po4a/po4a.cfg", "#{c(:PO4A)} #{c(:PO4A_FLAGS)} --msgmerge-opt=--no-wrap doc/man/po4a/po4a.cfg"
      %w{nl zh_CN}.each do |language|
        name = "doc/man/po4a/po/#{language}.po"
        FileUtils.cp name, "#{name}.tmp"
        adjust_to_poedit_style "#{name}.tmp", name
      end
    end
  end

  [ :stats, :statistics ].each_with_index do |name, idx|
    desc "Generate statistics about translation coverage" if 0 == idx
    task name do
      FileList["po/*.po", "doc/man/po4a/po/*.po"].each do |name|
        command = "msgfmt --statistics -o /dev/null #{name} 2>&1"
        if ENV["V"].to_bool
          runq "  MSGFMT #{name}", command, :allow_failure => true
        else
          puts "#{name} : " + `#{command}`.split(/\n/).first
        end
      end
    end
  end
end

# HTMLO generation for the man pages
targets = ([ 'en' ] + $languages[:manpages]).collect do |language|
  dir = language == 'en' ? '' : "/#{language}"
  FileList[ "doc/man#{dir}/*.xml" ].collect { |name| "man2html:#{language}:#{File.basename(name, '.xml')}" }
end.flatten

%w{manpages-html man2html}.each_with_index do |task_name, idx|
  desc "Create HTML files for the man pages" if 0 == idx
  task task_name => targets
end

namespace :man2html do
  ([ 'en' ] + $languages[:manpages]).collect do |language|
    namespace language do
      dir = language == 'en' ? '' : "/#{language}"
      FileList[ "doc/man#{dir}/*.xml" ].each do |name|
        task File.basename(name, '.xml') => %w{manpages translations:manpages} do
          runq "XSLTPROC #{name}", "xsltproc --nonet -o #{name.ext('html')} /usr/share/xml/docbook/stylesheet/nwalsh/html/docbook.xsl #{name}"
        end
      end
    end
  end
end

# Installation tasks
desc "Install all applications and support files"
targets  = [ "install:programs", "install:manpages", "install:translations:manpages", "install:translations:applications", "install:translations:guides" ]
targets << "install:shared" if c?(:USE_WXWIDGETS)
task :install => targets

namespace :install do
  application_name_mapper = lambda do |name|
    base = File.basename name
    base == "mmg" ? $mmg_bin : base
  end

  task :programs => $applications do
    install_dir :bindir
    $applications.each { |application| install_program "#{c(:bindir)}/#{application_name_mapper[application]}", application }
  end

  task :shared do
    install_dir :desktopdir, :mimepackagesdir
    install_data :desktopdir, FileList[ "#{$top_srcdir}/share/desktop/*.desktop" ]
    install_data :mimepackagesdir, FileList[ "#{$top_srcdir}/share/mime/*.xml" ]

    FileList[ "#{$top_srcdir}/share/icons/*" ].collect { |dir| File.basename dir }.select { |dir| dir != "windows" }.each do |dir|
      install_dir "#{c(:icondir)}/#{dir}/apps"
      install_data "#{c(:icondir)}/#{dir}/apps/", FileList[ "#{$top_srcdir}/share/icons/#{dir}/*.png" ]
    end
  end

  man_page_name_mapper = lambda do |name|
    base = File.basename name
    base == "mmg.1" ? "#{$mmg_bin}.1" : base
  end

  task :manpages => $manpages do
    install_dir :man1dir
    $manpages.each { |manpage| install_data "#{c(:man1dir)}/#{man_page_name_mapper[manpage]}", manpage }
  end

  namespace :translations do
    task :applications do
      install_dir $languages[:applications].collect { |language| "#{c(:localedir)}/#{language}/LC_MESSAGES" }
      $languages[:applications].each do |language|
        install_data "#{c(:localedir)}/#{language}/LC_MESSAGES/mkvtoolnix.mo", "po/#{language}.mo"
      end
    end

    task :manpages do
      install_dir $languages[:manpages].collect { |language| "#{c(:mandir)}/#{language}/man1" }
      $languages[:manpages].each do |language|
        $manpages.each { |manpage| install_data "#{c(:mandir)}/#{language}/man1/#{man_page_name_mapper[manpage]}", manpage.sub(/man\//, "man/#{language}/") }
      end
    end

    task :guides do
      install_dir :docdir, $languages[:guides].collect { |language| "#{c(:docdir)}/guide/#{language}/images" }

      $languages[:guides].each do |language|
        install_data "#{c(:docdir)}/guide/#{language}/",        FileList[ "#{$top_srcdir}/doc/guide/#{language}/mkvmerge-gui.*" ]
        install_data "#{c(:docdir)}/guide/#{language}/images/", FileList[ "#{$top_srcdir}/doc/guide/#{language}/images/*.gif"   ]
      end
    end
  end
end

# Cleaning tasks
desc "Remove all compiled files"
task :clean do
  tools = $tools.collect { |name| "src/tools/#{name}" }.join " "
  run <<-SHELL, :allow_failure => true
    rm -f *.o */*.o */*/*.o */lib*.a */*/lib*.a */*/*.gch po/*.mo
      */*.exe */*/*.exe */*/*.dll */*/*.dll.a doc/guide/*/*.hhk
      src/info/ui/*.h src/info/*.moc.cpp src/common/*/*.o
      src/mmg/*/*.o #{$applications.join(" ")} #{tools}
      lib/libebml/src/*.o lib/libmatroska/src/*.o
      lib/libebml/src/lib*.a lib/libmatroska/src/lib*.a
  SHELL
end

namespace :clean do
  desc "Remove all compiled and generated files ('tarball' clean)"
  task :dist => :clean do
    run "rm -f config.h config.log config.cache build-config Makefile */Makefile */*/Makefile TAGS", :allow_failure => true
    run "rm -rf #{$dependency_dir}", :allow_failure => true
  end

  desc "Remove all compiled and generated files ('git' clean)"
  task :maintainer => "clean:dist" do
    run "rm -f configure config.h.in", :allow_failure => true
  end

  desc "Remove all compiled libraries"
  task :libs do
    run "rm -f */lib*.a */*/lib*.a */*/*.dll */*/*.dll.a", :allow_failure => true
  end

  [:apps, :applications, :exe].each_with_index do |name, idx|
    desc "Remove all compiled applications" if 0 == idx
    task name do
      run "rm -f #{$applications.join(" ")} */*.exe */*/*.exe", :allow_failure => true
    end
  end

  %w{manpages-html man2html}.each do |name|
    task name do
      run "rm -f doc/man/*.html doc/man/*/*.html"
    end
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
  { :name => 'ebml',       :dir => 'lib/libebml/src'                                                                                  },
  { :name => 'matroska',   :dir => 'lib/libmatroska/src'                                                                              },
].each do |lib|
  Library.
    new("#{[ lib[:dir] ].flatten.first}/lib#{lib[:name]}").
    sources([ lib[:dir] ].flatten, :type => :dir).
    build_dll(lib[:name] == 'mtxcommon').
    libraries(:iconv, :z, :compression, :matroska, :ebml, :expat, :rpcrt4).
    create
end

#
# mkvmerge
#

Application.new("src/mkvmerge").
  description("Build the mkvmerge executable").
  aliases(:mkvmerge).
  sources("src/merge", :type => :dir).
  sources("src/merge/resources.o", :if => c?(:MINGW)).
  libraries(:mtxinput, :mtxoutput, :mtxcommon, :magic, :matroska, :ebml, :avi, :rmff, :mpegparser, :flac, :vorbis, :ogg, :z, :compression, :expat, :iconv, :intl, :curl,
             :boost_regex, :boost_filesystem, :boost_system).
  create

#
# mkvinfo
#

$mkvinfo_ui_files = FileList["src/info/ui/*.ui"].to_a
file "src/info/qt_ui.o" => $mkvinfo_ui_files

Application.new("src/mkvinfo").
  description("Build the mkvinfo executable").
  aliases(:mkvinfo).
  sources(FileList["src/info/*.cpp"].exclude("src/info/qt_ui.cpp", "src/info/wxwidgets_ui.cpp")).
  sources("src/info/resources.o", :if => c?(:MINGW)).
  libraries(:mtxcommon, :magic, :matroska, :ebml, :expat, :intl, :iconv, :curl, :boost_regex, :boost_filesystem, :boost_system).
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
  description("Build the mkvpropedit executable").
  aliases(:mkvextract).
  sources("src/extract", :type => :dir).
  sources("src/extract/resources.o", :if => c?(:MINGW)).
  libraries(:mtxcommon, :magic, :matroska, :ebml, :avi, :rmff, :vorbis, :ogg, :z, :compression, :expat, :iconv, :intl, :curl,
             :boost_regex, :boost_filesystem, :boost_system).
  create

#
# mkvpropedit
#

Application.new("src/mkvpropedit").
  description("Build the mkvpropedit executable").
  aliases(:mkvpropedit).
  sources("src/propedit", :type => :dir).
  sources("src/propedit/resources.o", :if => c?(:MINGW)).
  libraries(:mtxcommon, :magic, :matroska, :ebml, :avi, :rmff, :vorbis, :ogg, :z, :compression, :expat, :iconv, :intl, :curl,
             :boost_regex, :boost_filesystem, :boost_system).
  create

#
# mmg
#

if c?(:USE_WXWIDGETS)
  Application.new("src/mmg/mmg").
    description("Build the mmg executable").
    aliases(:mmg).
    sources("src/mmg", "src/mmg/header_editor", "src/mmg/options", "src/mmg/tabs", :type => :dir).
    sources("src/mmg/mmg-resources.o", :if => c?(:MINGW)).
    libraries(:mtxcommon, :magic, :matroska, :ebml, :avi, :rmff, :vorbis, :ogg, :z, :compression, :expat, :iconv, :intl, :wxwidgets, :curl,
               :boost_regex, :boost_filesystem, :boost_system).
    libraries(:ole32, :shell32, "-mwindows", :if => c?(:MINGW)).
    create
end

#
# Applications in src/tools
#
if $build_tools
  namespace :apps do
    task :tools => $tools.collect { |name| "apps:tools:#{name}" }
  end

  #
  # tools: base64tool
  #
  Application.new("src/tools/base64tool").
    description("Build the base64tool executable").
    aliases("tools:base64tool").
    sources("src/tools/base64tool.cpp").
    libraries(:mtxcommon, :magic, :matroska, :ebml, :expat, :iconv, :intl, :boost_regex, :curl).
    create

  #
  # tools: diracparser
  #
  Application.new("src/tools/diracparser").
    description("Build the diracparser executable").
    aliases("tools:diracparser").
    sources("src/tools/diracparser.cpp").
    libraries(:mtxcommon, :magic, :matroska, :ebml, :expat, :iconv, :intl, :boost_regex, :curl).
    create

  #
  # tools: ebml_validator
  #
  Application.new("src/tools/ebml_validator").
    description("Build the ebml_validator executable").
    aliases("tools:ebml_validator").
    sources("src/tools/ebml_validator.cpp", "src/tools/element_info.cpp").
    libraries(:mtxcommon, :magic, :matroska, :ebml, :expat, :iconv, :intl, :boost_regex, :curl).
    create

  #
  # tools: vc1parser
  #
  Application.new("src/tools/vc1parser").
    description("Build the vc1parser executable").
    aliases("tools:vc1parser").
    sources("src/tools/vc1parser.cpp").
    libraries(:mtxcommon, :magic, :matroska, :ebml, :expat, :iconv, :intl, :boost_regex, :curl).
    create
end
