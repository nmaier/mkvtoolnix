#!/usr/bin/ruby -w

class T_332eac3_misdetected_as_avc < SimpleTest
  describe "mkvmerge / EAC3 misdetected as AVC ES"

  test_identify "data/simple/misdetected_as_avc.ac3"
end

