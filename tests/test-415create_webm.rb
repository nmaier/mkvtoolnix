#!/usr/bin/ruby -w

# T_415create_webm
describe "mkvmerge / create WebM files from various formats"

[ 'webm/yt3.ivf',    # VP8
  'webm/v-vp9.ivf',  # VP9
  'ogg/v.ogg',       # Vorbis
  'opus/v-opus.ogg', # Opus
].each do |file_name|
  test_merge "data/#{file_name}", :args => "--webm"
end

[ [ 'AAC',       'aac/v.aac'                            ],
  [ 'AC3',       'ac3/v.ac3'                            ],
  [ 'ALAC',      'alac/test-alacconvert.caf'            ],
  [ 'DivX',      'avi/divxFaac51.avi'                   ],
  [ 'h.264/AVC', 'h254/opengop.h264'                    ],
  [ 'Dirac',     'dirac/v.drc'                          ],
  [ 'DTS',       'dts/dts-hd.dts'                       ],
  [ 'FLV',       'flv/flv1-video-no-dimensions.flv'     ],
  [ 'MP3',       'mp3/v.mp3'                            ],
  [ 'MPEG1',     'mpeg12/mpeg1-misdetected-as-avc.mpg'  ],
  [ 'MPEG2',     'mpeg12/changing_sequence_headers.m2v' ],
  [ 'PCM',       'pcm/mono-16bit.mkv'                   ],
  [ 'RV4',       'rm/rv4.rm'                            ],
  [ 'SSA',       'ssa-ass/11.Magyar.ass'                ],
  [ 'PGS',       'subtitles/pgs/x.sup'                  ],
  [ 'SRT',       'subtitles/srt/ven.srt'                ],
  [ 'USF',       'subtitles/usf/u.usf'                  ],
  [ 'VC1',       'vc1/MC.track_4113.vc1'                ],
  [ 'WavPack4',  'wavpack4/v.wv'                        ],
].each do |pair|
  test pair[1] do
    output, exit_code = merge(pair[1], :args => "--webm", :exit_code => :error)
    pair[0] + '@' + (exit_code == 2 ? 'ok' : 'bad')
  end
end
