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
  $programs                =  %w{mkvmerge mkvinfo mkvextract mkvpropedit}
  $programs                << "mmg" if c?(:USE_WXWIDGETS)

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
    :applications          =>                        $languages[:applications].collect { |language| "po/#{language}.mo" },
    :guides                =>                        $languages[:guides].collect       { |language| "doc/guide/#{language}/mkvmerge-gui.hhk" },
    :manpages              => c?(:PO4A_WORKS) ? [] : $languages[:manpages].collect    { |language| $manpages.collect { |manpage| manpage.gsub(/man\//, "man/#{language}/") } }.flatten,
  }

  $available_languages     =  {
    :applications          => FileList[ "#{$top_srcdir }/po/*.po"                       ].collect { |name| File.basename name, '.po'        },
    :manpages              => FileList[ "#{$top_srcdir }/doc/man/po4a/po/*.po"          ].collect { |name| File.basename name, '.po'        },
    :guides                => FileList[ "#{$top_srcdir }/doc/guide/*/mkvmerge-gui.html" ].collect { |name| File.basename File.dirname(name) },
  }
end

def define_default_task
  desc "Build everything"

  # The applications themselves
  targets = $applications.clone

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

  runq "     CXX #{sources.first}", "#{c(:CXX)} #{c(:CXXFLAGS)} #{c(:INCLUDES)} #{$system_includes} #{cxxflags} -c -MMD -o #{t.name} #{sources.join(" ")}", :allow_failure => true
  handle_deps t.name, last_exit_code
end

# Precompiled headers
if c?(:USE_PRECOMPILED_HEADERS)
  $all_objects.each { |name| file name => "src/common/common_pch.h.o" }
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

# Qt files
rule '.h' => '.ui' do |t|
  runq "     UIC #{t.source}", "#{c(:UIC)} #{t.sources.join(" ")} > #{t.name}"
end

rule '.moc.cpp' => '.h' do |t|
  runq "     MOC #{t.source}", "#{c(:MOC)} #{c(:QT_CFLAGS)} #{t.sources.join(" ")} > #{t.name}"
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

  [ :applications, :manpages, :guides ].each { |type| task type => $translations[type] }

  desc "Update the program translation files"
  task "update" => [ "po/mkvtoolnix.pot", ] + $languages[:applications].collect { |language| "translations:update:#{language}".to_sym }

  namespace "update" do
    $available_languages[:applications].each do |language|
      task language.to_sym => "po/mkvtoolnix.pot" do |t|
        po     = "po/#{language}.po"
        new_po = "#{po}.new"
        runq "MSGMERGE #{po}", "msgmerge -q -s --no-wrap #{po} po/mkvtoolnix.pot > #{new_po}", :allow_failure => true

        exit_code = last_exit_code
        if 0 != exit_code
          File.unlink new_po
          exit exit_code
        end

        File.open(po, "w") do |out|
          lines = IO.readlines(new_po).collect { |line| line.chomp }

          if %w{es nl ru uk zh_CN zh_TW}.include? language
            no_nl = false

            lines.each do |line|
              if /^#:/.match(line)
                out.puts line.gsub(/(\d) /, '\1' + "\n#: ")
              elsif /^#~/.match(line)
                no_nl = true
                out.puts line
              elsif !(no_nl && /^\s*$/.match(line))
                out.puts line
              end
            end

            out.puts
          else
            out.puts lines.join("\n")
          end
        end

        File.unlink new_po
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

# Installation tasks
desc "Install all applications and support files"
targets  = [ "install:programs", "install:manpages", "install:translations:manpages", "install:translations:applications", "install:translations:guides" ]
targets << "install:shared" if c?(:USE_WXWIDGETS)
task :install => targets

namespace :install do
  task :programs => $applications do
    install_dir :bindir
    install_program :bindir, $applications
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

  task :manpages => $manpages do
    install_dir :man1dir
    install_data :man1dir, $manpages
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
        install_data "#{c(:mandir)}/#{language}/man1/", $manpages.collect { |manpage| manpage.sub(/man\//, "man/#{language}/") }
      end
    end

    task :guides do
      install_dir :pkgdatadir, $languages[:guides].collect { |language| "#{c(:pkgdatadir)}/guide/#{language}/images" }

      $languages[:guides].each do |language|
        install_data "#{c(:pkgdatadir)}/guide/#{language}/",        FileList[ "#{$top_srcdir}/doc/guide/#{language}/mkvmerge-gui.*" ]
        install_data "#{c(:pkgdatadir)}/guide/#{language}/images/", FileList[ "#{$top_srcdir}/doc/guide/#{language}/images/*.gif"   ]
      end
    end
  end
end

# Cleaning tasks
desc "Remove all compiled files"
task :clean do
  run <<-SHELL, :allow_failure => true
    rm -f *.o */*.o */*/*.o */lib*.a */*/lib*.a po/*.mo #{$applications.join(" ")}
      */*.exe */*/*.exe */*/*.dll */*/*.dll.a doc/guide/*/*.hhk
      src/info/ui/*.h src/info/*.moc.cpp src/common/*/*.o
      src/mmg/*/*.o
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
  description("Build the mkvmerge executable").
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
  description("Build the mkvinfo executable").
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
  description("Build the mkvpropedit executable").
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
  description("Build the mkvpropedit executable").
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
    description("Build the mmg executable").
    aliases(:mmg).
    sources("src/mmg", "src/mmg/header_editor", "src/mmg/options", "src/mmg/tabs", :type => :dir).
    sources("src/propedit/mmg-resources.o", :if => c?(:MINGW)).
    libraries(:mtxcommon, :magic, :matroska, :ebml, :avi, :rmff, :vorbis, :ogg, :z, :compression, :expat, :iconv, :intl, :wxwidgets,
               :boost_regex, :boost_filesystem, :boost_system).
    libraries(:rpcrt4, :ole32, :shell32, "-mwindows", :if => c?(:MINGW)).
    create
end
