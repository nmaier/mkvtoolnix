#!/usr/bin/ruby -w

class T_318ui_locale_invalid < Test
  def description
    "mkvmerge / UI locale: invalid"
  end

  def run
    sys "../src/mkvmerge --ui-language gnufudel", 2

    return "ok"
  end
end
