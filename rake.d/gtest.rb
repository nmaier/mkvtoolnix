#!/usr/bin/env ruby

$gtest_apps     = %w{common propedit}
$gtest_internal = c(:GTEST_TYPE) == "internal"

namespace :tests do
  desc "Build the unit tests"
  task :unit => $gtest_apps.collect { |app| "tests/unit/#{app}/#{app}" }

  desc "Build and run the unit tests"
  task :run_unit => 'tests:unit' do
    $gtest_apps.each { |app| run "./tests/unit/#{app}/#{app}" }
  end
end

$build_system_modules[:gtest] = {
  :setup => lambda do
    if $gtest_internal
      $flags[:cxxflags] += " -Ilib/gtest -Ilib/gtest/include"
      $flags[:ldflags]  += " -Llib/gtest/src"
    end
  end,

  :define_tasks => lambda do
    gtest_libs = {
      'common'   => [],
      'propedit' => [ :mtxpropedit ],
    }

    #
    # Google Test framework
    #
    if $gtest_internal
      Library.
        new('lib/gtest/src/libgtest').
        sources([ 'lib/gtest/src' ], :type => :dir, :except => [ 'gtest-all.cc' ]).
        create
    end

    Library.
      new('tests/unit/libmtxunittest').
      sources('tests/unit', :type => :dir).
      create

    $gtest_apps.each do |app|
      Application.
        new("tests/unit/#{app}/#{app}").
        description("Build the unit tests executable for '#{app}'").
        aliases("unit_tests_#{app}").
        sources([ "tests/unit/#{app}" ], :type => :dir).
        libraries(gtest_libs[app], :mtxunittest, $common_libs, :gtest, :pthread).
        create
    end
  end,
}
