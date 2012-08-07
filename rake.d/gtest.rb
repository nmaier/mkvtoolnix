#!/usr/bin/env ruby

namespace :tests do
  desc "Build and run the unit tests"
  task :unit => [ 'tests/unit/all' ] do
    run './tests/unit/all'
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

    Application.
      new('tests/unit/all').
      description("Build the unit tests executable").
      aliases('unit_tests').
      sources([ 'tests/unit' ], :type => :dir).
      libraries($common_libs, :gtest, :pthread).
      create
  end,
}
