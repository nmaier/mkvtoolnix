#!/usr/bin/ruby -w

# T_390timecode_info_on_resync
describe 'mkvmerge / output timecodes on resync'

test 'data/mkv/resync-after-broken.mkv' do
  output = merge('data/mkv/resync-after-broken.mkv', :args => '--ui-language en_US').join('')
  [/last.*timecode.*before.*error/, /first.*cluster.*timecode.*resync/].collect { |re| re.match(output) ? 'ok' : 'BAD' }.join('+')
end
