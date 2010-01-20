#!/usr/bin/ruby -w

class T_260version_numbers < Test
  def description
    return "version number of CLI tools"
  end

  def run
    %w{merge info extract propedit}.each do |name|
      sys "../src/mkv#{name} --version | grep -E 'v([0-9]\\.){2}[0-9].*built on'"
    end

    return 'ok'
  end
end

