#!/usr/bin/env ruby

gtest_apps = %w{common}
gtest_libs = {
  'common' => [],
}

namespace :tests do
  desc "Build and run the unit tests"
  task :unit => gtest_apps.collect { |app| "tests/unit/#{app}/#{app}" } do
    gtest_apps.each { |app| run "./tests/unit/#{app}/#{app}" }
  end
end

$build_system_modules[:gtest] = {
  :setup => lambda do
    $flags[:cxxflags] += " -Ilib/gtest -Ilib/gtest/include"
    $flags[:ldflags]  += " -Llib/gtest/src"
  end,

  :define_tasks => lambda do
    #
    # Google Test framework
    #
    Library.
      new('lib/gtest/src/libgtest').
      sources([ 'lib/gtest/src' ], :type => :dir).
      create

    gtest_apps.each do |app|
      Application.
        new("tests/unit/#{app}/#{app}").
        description("Build the unit tests executable for '#{app}'").
        aliases("unit_tests_#{app}").
        sources([ "tests/unit/#{app}" ], :type => :dir).
        libraries($common_libs, gtest_libs[app], :gtest, :pthread).
        create
    end
  end,
}
