#!/usr/bin/env ruby

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

# Extensions have to be loaded before certain functions that don't
# exist in Ruby 1.8.x are used, e.g. Dir.exists?
require_relative "rake.d/extensions"

$build_system_modules = {}
$have_gtest           = Dir.exists? 'lib/gtest'

require_relative "rake.d/config"
require_relative "rake.d/helpers"
require_relative "rake.d/target"
require_relative "rake.d/application"
require_relative "rake.d/library"
require_relative 'rake.d/gtest' if $have_gtest

def setup_globals
  $build_mkvtoolnix_gui  ||=  c?(:USE_QT) && c?(:BUILD_MKVTOOLNIX_GUI)

  $programs                =  %w{mkvmerge mkvinfo mkvextract mkvpropedit}
  $programs                << "mmg" if c?(:USE_WXWIDGETS)
  $programs                << "mkvtoolnix-gui" if $build_mkvtoolnix_gui
  $tools                   =  %w{ac3parser base64tool diracparser ebml_validator vc1parser}
  $mmg_bin                 =  c(:MMG_BIN)
  $mmg_bin                 =  "mmg" if $mmg_bin.empty?

  $application_subdirs     =  { "mmg" => "mmg/", "mkvtoolnix-gui" => "mkvtoolnix-gui/" }
  $applications            =  $programs.collect { |name| "src/#{$application_subdirs[name]}#{name}" + c(:EXEEXT) }
  $manpages                =  $programs.collect { |name| "doc/man/#{name}.1" }

  $system_includes         = "-I. -Ilib -Ilib/avilib-0.6.10 -Ilib/utf8-cpp/source -Ilib/pugixml/src -Isrc"
  $system_libdirs          = "-Llib/avilib-0.6.10 -Llib/librmff -Llib/pugixml/src -Lsrc/common"

  $source_directories      =  %w{lib/avilib-0.6.10 lib/librmff src src/input src/output src/common src/common/chapters src/common/compression src/common/strings src/common/tags src/common/xml
                                 src/mmg src/mmg/header_editor src/mmg/options src/mmg/tabs src/extract src/propedit src/merge src/info src/mpegparser}
  $all_sources             =  $source_directories.collect { |dir| FileList[ "#{dir}/*.c", "#{dir}/*.cpp" ].to_a }.flatten.sort
  $all_headers             =  $source_directories.collect { |dir| FileList[ "#{dir}/*.h",                ].to_a }.flatten.sort
  $all_objects             =  $all_sources.collect { |file| file.ext('o') }

  $top_srcdir              = c(:top_srcdir)
  $dependency_dir          = "#{$top_srcdir}/rake.d/dependency.d"
  $dependency_tmp_dir      = "#{$dependency_dir}/tmp"

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
  $build_unit_tests      ||=  c?(:UNIT_TESTS)

  cflags_common            = "-Wall -Wno-comment #{c(:OPTIMIZATION_CFLAGS)} -D_FILE_OFFSET_BITS=64 #{c(:MATROSKA_CFLAGS)} #{c(:EBML_CFLAGS)} #{c(:EXTRA_CFLAGS)} #{c(:DEBUG_CFLAGS)} #{c(:PROFILING_CFLAGS)} #{c(:USER_CPPFLAGS)} "
  cflags_common           += "-DPACKAGE=\\\"#{c(:PACKAGE)}\\\" -DVERSION=\\\"#{c(:VERSION)}\\\" -DMTX_LOCALE_DIR=\\\"#{c(:localedir)}\\\" -DMTX_PKG_DATA_DIR=\\\"#{c(:pkgdatadir)}\\\" -DMTX_DOC_DIR=\\\"#{c(:docdir)}\\\""
  ldflags_extra            = c?(:MINGW) ? '' : "-Wl,--enable-auto-import"
  $flags                   = {
    :cflags                => "#{cflags_common} #{c(:USER_CFLAGS)}",
    :cxxflags              => "#{cflags_common} #{c(:STD_CXX0X)} -Wnon-virtual-dtor -Woverloaded-virtual -Wextra #{c(:WXWIDGETS_CFLAGS)} #{c(:QT_CFLAGS)} #{c(:BOOST_CPPFLAGS)} #{c(:CURL_CFLAGS)} #{c(:USER_CXXFLAGS)}",
    :cppflags              => "#{c(:USER_CPPFLAGS)}",
    :ldflags               => "#{c(:EBML_LDFLAGS)} #{c(:MATROSKA_LDFLAGS)} #{c(:EXTRA_LDFLAGS)} #{c(:PROFILING_LIBS)} #{c(:USER_LDFLAGS)} #{c(:LDFLAGS_RPATHS)} #{c(:BOOST_LDFLAGS)}",
    :windres               => c?(:USE_WXWIDGETS) ? c(:WXWIDGETS_INCLUDES) : '-DNOWXWIDGETS',
  }

  $build_system_modules.values.each { |bsm| bsm[:setup].call if bsm[:setup] }
end

def setup_overrides
  [ :applications, :manpages, :guides ].each do |type|
    value                      = c("AVAILABLE_LANGUAGES_#{type.to_s.upcase}")
    $available_languages[type] = value.split(/\s+/) unless value.empty?
  end
end

def define_default_task
  desc "Build everything"

  # The applications themselves
  targets = $applications.clone

  # Build the stuff in the 'src/tools' directory only if requested
  targets << "apps:tools" if $build_tools

  # Build the unit tests only if requested
  targets << "tests:unit" if $have_gtest && $build_unit_tests

  # The tags file -- but only if it exists already
  if File.exists?("TAGS")
    targets << "TAGS"   if !c(:ETAGS).empty?
    targets << "BROWSE" if !c(:EBROWSE).empty?
  end

  # Build developer documentation?
  targets << "doc/development.html" if !c(:PANDOC).empty?

  # Build man pages and translations?
  targets += [ "manpages", "translations:manpages" ] if c?(:XSLTPROC_WORKS)

  # Build translations for the programs
  targets << "translations:applications"

  # The GUI help
  targets << "translations:guides" if c?(:USE_WXWIDGETS)

  task :default => targets do
    puts "Done. Enjoy :)"
  end
end

# main
read_config
setup_globals
setup_overrides
import_dependencies

# Default task
define_default_task

desc "Build all applications"
task :apps => $applications

desc "Build all command line applications"
namespace :apps do
  task :cli => %w{apps:mkvmerge apps:mkvinfo apps:mkvextract apps:mkvpropedit}
end

# Store compiler block for re-use
cxx_compiler = lambda do |t|
  create_dependency_dirs

  # t.sources is empty for a 'file' task (common_pch.h.o).
  sources = t.sources.empty? ? [ t.prerequisites.first ] : t.sources
  dep     = dependency_output_name_for t.name

  runq "     CXX #{sources.first}", "#{c(:CXX)} #{$flags[:cxxflags]} #{$system_includes} -c -MMD -MF #{dep} -o #{t.name} -x c++ #{sources.join(" ")}", :allow_failure => true
  handle_deps t.name, last_exit_code, true
end

# Precompiled headers
if c?(:USE_PRECOMPILED_HEADERS)
  $all_objects.each { |name| file name => "src/common/common_pch.h.gch" }
  file "src/common/common_pch.h.gch" => "src/common/common_pch.h", &cxx_compiler
end

# Pattern rules
rule '.o' => '.cpp', &cxx_compiler
rule '.o' => '.cc',  &cxx_compiler

rule '.o' => '.c' do |t|
  create_dependency_dirs
  dep = dependency_output_name_for t.name

  runq "      CC #{t.source}", "#{c(:CC)} #{$flags[:cflags]} #{$system_includes} -c -MMD -MF #{dep} -o #{t.name} #{t.sources.join(" ")}", :allow_failure => true
  handle_deps t.name, last_exit_code
end

rule '.o' => '.rc' do |t|
  runq " WINDRES #{t.source}", "#{c(:WINDRES)} #{$flags[:windres]} -Isrc/mmg -o #{t.name} #{t.sources.join(" ")}"
end

rule '.h' => '.png' do |t|
  runq "   BIN2H #{t.source}", "#{c(:top_srcdir)}/rake.d/bin/bin2h.rb #{t.source} #{t.name}"
end

# Resources depend on the manifest.xml file for Windows builds.
if c?(:MINGW)
  $programs.each do |program|
    path = program.gsub(/^mkv/, '')
    icon = program == 'mkvinfo' ? 'share/icons/windows/mkvinfo.ico' : 'share/icons/windows/mkvmergeGUI.ico'
    file "src/#{path}/resources.o" => [ "src/#{path}/manifest.xml", "src/#{path}/resources.rc", icon ]
  end
end

rule '.mo' => '.po' do |t|
  runq "  MSGFMT #{t.source}", "msgfmt -c -o #{t.name} #{t.sources.join(" ")}"
end

# HTML help book stuff
rule '.hhk' => '.hhc' do |t|
  runq "    GREP #{t.source}", "#{c(:GREP)} -v 'name=\"ID\"' #{t.sources.join(" ")} > #{t.name}"
end

# man pages from DocBook XML
if c?(:XSLTPROC_WORKS)
  rule '.1' => '.xml' do |t|
    filter = lambda do |code, lines|
      puts lines.join('')
      if (0 == code) && lines.any? { |line| /^error/i.match(line) }
        File.unlink(t.name) if File.exists?(t.name)
        1
      else
        0
      end
    end

    runq "XSLTPROC #{t.source}", "#{c(:XSLTPROC)} #{c(:XSLTPROC_FLAGS)} -o #{t.name} #{c(:DOCBOOK_MANPAGES_STYLESHEET)} #{t.sources.join(" ")}", :filter_output => filter
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

rule '.cpp' => '.qrc' do |t|
  runq "     RCC #{t.source}", "#{c(:RCC)} #{t.sources.join(" ")} > #{t.name}"
end

rule '.moc' => '.h' do |t|
  runq "     MOC #{t.prerequisites.first}", "#{c(:MOC)} #{c(:QT_CFLAGS)} -nw #{t.prerequisites.join(" ")} > #{t.name}"
end

rule '.moco' => '.moc' do |t|
  cxx_compiler.call t
end

# Tag files
desc "Create tags file for Emacs"
task :tags => "TAGS"

desc "Create browse file for Emacs"
task :browse => "BROWSE"

file "TAGS" => $all_sources do |t|
  runq '   ETAGS', "#{c(:ETAGS)} -o #{t.name} #{t.prerequisites.join(" ")}"
end

file "BROWSE" => ($all_sources + $all_headers) do |t|
  runq ' EBROWSE', "#{c(:EBROWSE)} -o #{t.name} #{t.prerequisites.join(" ")}"
end

file "doc/development.html" => [ "doc/development.md", "doc/pandoc-template.html" ] do |t|
  runq "  PANDOC #{t.prerequisites.first}", "#{c(:PANDOC)} -o #{t.name} --standalone --from markdown --to html --strict --number-sections --table-of-contents " +
    "--css=pandoc.css --template=doc/pandoc-template.html doc/development.md"
end

file "po/mkvtoolnix.pot" => $all_sources + $all_headers + %w{Rakefile} do |t|
  sources = t.prerequisites.dup - %w{Rakefile}
  runq "XGETTEXT #{t.name}", <<-COMMAND
    xgettext --keyword=YT --keyword=Y --keyword=Z --keyword=TIP --keyword=NY:1,2 --keyword=NZ:1,2 --default-domain=mkvtoolnix --from-code=UTF-8 -s --omit-header --boost -o #{t.name} #{sources.join(" ")}
  COMMAND
end

task :manpages => $manpages

# Translations for the programs
namespace :translations do
  desc "Create a template for translating the programs"
  task :pot => "po/mkvtoolnix.pot"

  desc "Create a new .po file with an empty template"
  task "new-po" => "po/mkvtoolnix.pot" do
    %w{LANGUAGE EMAIL}.each { |e| fail "Variable '#{e}' is not set" if ENV[e].blank? }

    require 'rexml/document'
    node   = REXML::XPath.first REXML::Document.new(File.new("/usr/share/xml/iso-codes/iso_639.xml")), "//iso_639_entry[@name='#{ENV['LANGUAGE']}']"
    locale = node ? node.attributes['iso_639_1_code'] : nil
    fail "Unknown language/ISO-639-1 code not found" if locale.blank?

    puts "  CREATE po/#{locale}.po"
    File.open "po/#{locale}.po", "w" do |out|
      now = Time.now
      out.puts <<EOT
# translation of mkvtoolnix.pot to #{ENV['LANGUAGE']}
# Copyright (C) #{now.year} Moritz Bunkus
# This file is distributed under the same license as the mkvtoolnix package.
#
msgid ""
msgstr ""
"Project-Id-Version: #{locale}\\n"
"Report-Msgid-Bugs-To: Moritz Bunkus <moritz@bunkus.org>\\n"
"POT-Creation-Date: #{now.strftime('%Y-%m-%d %H:%M%z')}\\n"
"PO-Revision-Date: #{now.strftime('%Y-%m-%d %H:%M%z')}\\n"
"Last-Translator: YOUR NAME <#{ENV['EMAIL']}>\\n"
"Language-Team: #{ENV['LANGUAGE']} <moritz@bunkus.org>\\n"
"Language: #{locale}\\n"
"MIME-Version: 1.0\\n"

EOT
      out.puts IO.readlines("po/mkvtoolnix.pot")
    end
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

          adjust_to_poedit_style tmp_file, po
        end
      end
    end

    desc "Update the man pages' translation files"
    task :manpages do
      runq "    PO4A doc/man/po4a/po4a.cfg", "#{c(:PO4A)} #{c(:PO4A_FLAGS)} --msgmerge-opt=--no-wrap doc/man/po4a/po4a.cfg"
      %w{nl uk zh_CN}.each do |language|
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

# HTML generation for the man pages
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

# Developer helper tasks
namespace :dev do
  if $build_mkvtoolnix_gui
    desc "Update Qt resource files"
    task "update-qt-resources" do
      require 'rexml/document'

      qrc      = "src/mkvtoolnix-gui/qt_resources.qrc"
      doc      = REXML::Document.new File.new(qrc)
      existing = Hash.new

      doc.elements.to_a("/RCC/qresource/file").each do |node|
        if File.exists? "src/mkvtoolnix-gui/#{node.text}"
          existing[node.text] = true
        else
          puts "Removing <file> for non-existing #{node.text}"
          node.remove
        end
      end

      parent = doc.elements.to_a("/RCC/qresource")[0]
      FileList["share/icons/*/*.png"].select { |file| !existing["../../#{file}"] }.each do |file|
        puts "Adding <file> for #{file}"
        node                     = REXML::Element.new "file"
        node.attributes["alias"] = file.gsub(/share\//, '')
        node.text                = "../../#{file}"
        parent << node
      end

      formatter         = REXML::Formatters::Pretty.new 1
      formatter.compact = true
      formatter.width   = 9999999
      formatter.write doc, File.open(qrc, "w")
    end
  end
end

# Installation tasks
desc "Install all applications and support files"
targets  = [ "install:programs", "install:manpages", "install:translations:manpages", "install:translations:applications" ]
targets += [ "install:translations:guides", "install:shared" ] if c?(:USE_WXWIDGETS)
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

    wanted_apps     = %w{mkvmerge mkvmergeGUI mkvinfo mkvextract mkvpropedit}.collect { |e| "#{e}.png" }.to_hash_by
    wanted_dirs     = %w{16x16 24x24 32x32 48x48 64x64 96x96 128x128 256x256}.to_hash_by
    dirs_to_install = FileList[ "#{$top_srcdir}/share/icons/*"   ].select { |dir|  wanted_dirs[ dir.gsub(/.*icons\//, '').gsub(/\/.*/, '') ] }.sort.uniq

    dirs_to_install.each do |dir|
      dest_dir = "#{c(:icondir)}/#{dir.gsub(/.*icons\//, '')}/apps"
      install_dir dest_dir
      install_data "#{dest_dir}/", FileList[ "#{dir}/*" ].to_a.select { |file| wanted_apps[ file.gsub(/.*\//, '') ] }
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
  puts "   CLEAN"

  patterns = %w{
    src/**/*.o lib/**/*.o src/**/*.a lib/**/*.a src/**/*.gch
    src/**/*.exe src/**/*.dll src/**/*.dll.a
    src/info/ui/*.h src/mkvtoolnix-gui/forms/*.h src/**/*.moc src/**/*.moco src/mkvtoolnix-gui/qt_resources.cpp
    tests/unit/*.o tests/unit/all
    po/*.mo doc/guide/**/*.hhk
  }
  patterns += $applications + $tools.collect { |name| "src/tools/#{name}" }
  verbose   = ENV['V'].to_bool

  patterns.collect { |pattern| FileList[pattern].to_a }.flatten.uniq.select { |file_name| File.exists? file_name }.each do |file_name|
    puts "      rm #{file_name}" if verbose
    File.unlink file_name
  end

  if Dir.exists? $dependency_dir
    puts "  rm -rf #{$dependency_dir}" if verbose
    FileUtils.rm_rf $dependency_dir
  end
end

namespace :clean do
  desc "Remove all compiled and generated files ('tarball' clean)"
  task :dist => :clean do
    run "rm -f config.h config.log config.cache build-config Makefile */Makefile */*/Makefile TAGS", :allow_failure => true
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

# Tests
desc "Run all tests"
task :tests => [ 'tests:products' ]
namespace :tests do
  desc "Run prodct tests from 'tests' sub-directory (requires data files to be present)"
  task :products do
    run "cd tests && ./run.rb"
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

[ { :name => 'avi',         :dir => 'lib/avilib-0.6.10'                                                                             },
  { :name => 'rmff',        :dir => 'lib/librmff'                                                                                   },
  { :name => 'pugixml',     :dir => 'lib/pugixml/src'                                                                               },
  { :name => 'mpegparser',  :dir => 'src/mpegparser'                                                                                },
  { :name => 'mtxcommon',   :dir => [ 'src/common' ] + %w{chapters compression strings tags xml }.collect { |e| "src/common/#{e}" } },
  { :name => 'mtxinput',    :dir => 'src/input'                                                                                     },
  { :name => 'mtxoutput',   :dir => 'src/output'                                                                                    },
  { :name => 'mtxmerge',    :dir => 'src/merge',    :except => [ 'mkvmerge.cpp' ],                                                  },
  { :name => 'mtxinfo',     :dir => 'src/info',     :except => %w{qt_ui.cpp wxwidgets_ui.cpp mkvinfo.cpp},                          },
  { :name => 'mtxextract',  :dir => 'src/extract',  :except => [ 'mkvextract.cpp' ],                                                },
  { :name => 'mtxpropedit', :dir => 'src/propedit', :except => [ 'mkvpropedit.cpp' ],                                               },
  { :name => 'ebml',        :dir => 'lib/libebml/src'                                                                               },
  { :name => 'matroska',    :dir => 'lib/libmatroska/src'                                                                           },
].each do |lib|
  Library.
    new("#{[ lib[:dir] ].flatten.first}/lib#{lib[:name]}").
    sources([ lib[:dir] ].flatten, :type => :dir, :except => lib[:except]).
    build_dll(lib[:name] == 'mtxcommon').
    libraries(:iconv, :z, :compression, :matroska, :ebml, :rpcrt4).
    create
end

# libraries required for all programs via mtxcommon
$common_libs = [
  :mtxcommon,
  :magic,
  :matroska,
  :ebml,
  :z,
  :compression,
  :pugixml,
  :iconv,
  :intl,
  :curl,
  :boost_regex,
  :boost_filesystem,
  :boost_system,
]

#
# mkvmerge
#

Application.new("src/mkvmerge").
  description("Build the mkvmerge executable").
  aliases(:mkvmerge).
  sources("src/merge/mkvmerge.cpp").
  sources("src/merge/resources.o", :if => c?(:MINGW)).
  libraries(:mtxmerge, :mtxinput, :mtxoutput, $common_libs, :avi, :rmff, :mpegparser, :flac, :vorbis, :ogg).
  create

#
# mkvinfo
#

$mkvinfo_ui_files = FileList["src/info/ui/*.ui"].to_a
file "src/info/qt_ui.o" => $mkvinfo_ui_files.collect { |file| file.ext('h') }

Application.new("src/mkvinfo").
  description("Build the mkvinfo executable").
  aliases(:mkvinfo).
  sources("src/info/mkvinfo.cpp").
  sources("src/info/resources.o", :if => c?(:MINGW)).
  libraries(:mtxinfo, $common_libs).
  only_if(c?(:USE_QT)).
  sources("src/info/qt_ui.cpp", "src/info/qt_ui.moc", "src/info/rightclick_tree_widget.moc", $mkvinfo_ui_files).
  libraries(:qt).
  end_if.
  only_if(!c?(:USE_QT) && c?(:USE_WXWIDGETS)).
  sources("src/info/wxwidgets_ui.cpp").
  png_icon("share/icons/64x64/mkvinfo.png").
  libraries(:wxwidgets).
  end_if.
  create

#
# mkvextract
#

Application.new("src/mkvextract").
  description("Build the mkvextract executable").
  aliases(:mkvextract).
  sources("src/extract/mkvextract.cpp").
  sources("src/extract/resources.o", :if => c?(:MINGW)).
  libraries(:mtxextract, $common_libs, :avi, :rmff, :vorbis, :ogg).
  create

#
# mkvpropedit
#

Application.new("src/mkvpropedit").
  description("Build the mkvpropedit executable").
  aliases(:mkvpropedit).
  sources("src/propedit/propedit.cpp").
  sources("src/propedit/resources.o", :if => c?(:MINGW)).
  libraries(:mtxpropedit, $common_libs).
  create

#
# mmg
#

if c?(:USE_WXWIDGETS)
  Application.new("src/mmg/mmg").
    description("Build the mmg executable").
    aliases(:mmg).
    sources("src/mmg", "src/mmg/header_editor", "src/mmg/options", "src/mmg/tabs", :type => :dir).
    sources("src/mmg/resources.o", :if => c?(:MINGW)).
    png_icon("share/icons/64x64/mkvmergeGUI.png").
    libraries($common_libs, :wxwidgets).
    libraries(:ole32, :shell32, "-mwindows", :if => c?(:MINGW)).
    create
end

#
# mkvtoolnix-gui
#

if $build_mkvtoolnix_gui
  ui_files = FileList["src/mkvtoolnix-gui/forms/**/*.ui"].to_a
  ui_files.each do |ui|
    file ui.gsub(/forms\/(.*)\.ui$/, '\1/\1.cpp') => ui.ext('h')
  end

  ui_static_deps = {
    "main_window" => [ "merge_widget/merge_widget" ]
  }
  ui_static_deps.each do |ui_h, objects|
    objects.each { |object| file "src/mkvtoolnix-gui/#{object}.o" => "src/mkvtoolnix-gui/forms/#{ui_h}.h" }
  end

  Application.new("src/mkvtoolnix-gui/mkvtoolnix-gui").
    description("Build the mkvtoolnix-gui executable").
    aliases("mkvtoolnix-gui").
    sources(FileList["src/mkvtoolnix-gui/**/*.cpp"], ui_files, 'src/mkvtoolnix-gui/qt_resources.cpp').
    sources((FileList["src/mkvtoolnix-gui/**/*.h"].to_a - ui_files.collect { |ui| ui.ext 'h' }).collect { |h| h.ext 'moc' }).
    sources("src/mkvtoolnix-gui/resources.o", :if => c?(:MINGW)).
    libraries($common_libs, :qt).
    png_icon("share/icons/64x64/mkvmergeGUI.png").
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
  # tools: ac3parser
  #
  Application.new("src/tools/ac3parser").
    description("Build the ac3parser executable").
    aliases("tools:ac3parser").
    sources("src/tools/ac3parser.cpp").
    libraries($common_libs).
    create

  #
  # tools: base64tool
  #
  Application.new("src/tools/base64tool").
    description("Build the base64tool executable").
    aliases("tools:base64tool").
    sources("src/tools/base64tool.cpp").
    libraries($common_libs).
    create

  #
  # tools: diracparser
  #
  Application.new("src/tools/diracparser").
    description("Build the diracparser executable").
    aliases("tools:diracparser").
    sources("src/tools/diracparser.cpp").
    libraries($common_libs).
    create

  #
  # tools: ebml_validator
  #
  Application.new("src/tools/ebml_validator").
    description("Build the ebml_validator executable").
    aliases("tools:ebml_validator").
    sources("src/tools/ebml_validator.cpp", "src/tools/element_info.cpp").
    libraries($common_libs).
    create

  #
  # tools: vc1parser
  #
  Application.new("src/tools/vc1parser").
    description("Build the vc1parser executable").
    aliases("tools:vc1parser").
    sources("src/tools/vc1parser.cpp").
    libraries($common_libs).
    create
end

$build_system_modules.values.each { |bsm| bsm[:define_tasks].call if bsm[:define_tasks] }

# Local Variables:
# mode: ruby
# End:
