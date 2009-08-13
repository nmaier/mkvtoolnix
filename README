MKVToolNix 2.9.8
================

With these tools one can get information about (mkvinfo) Matroska
files, extract tracks/data from (mkvextract) Matroska files and create
(mkvmerge) Matroska files from other media files. Matroska is a new
multimedia file format aiming to become THE new container format for
the future. You can find more information about it and its underlying
technology, the Extensible Binary Meta Language (EBML), at

http://www.matroska.org/

The full documentation for each command is now maintained in its
man page only. Type 'mkvmerge -h' to get you started.

This code comes under the GPL (see www.gnu.org or the file COPYING).
Modify as needed.

The newest version can always be found at
http://www.bunkus.org/videotools/mkvtoolnix/

Moritz Bunkus <moritz@bunkus.org>

Installation
------------

If you want to compile the tools yourself then you must first decide
if you want to use a 'proper' release version or the current
development version. As both Matroska and MKVToolNix are under heavy
development there might be features available in the Subversion
repository that are not available in the releases. On the other hand
the Subversion repository version might not even compile.

* Requirements

In order to compile MKVToolNix you need a couple of libraries. Most of
them should be available pre-compiled for your distribution. The
libraries you absolutely need are:

- libebml and libmatroska for low-level access to Matroska files.
  Instructions on how to compile them are a bit further down in this
  file.
- expat ( http://expat.sourceforge.net/ ) -- a light-weight XML
  parser library
- libOgg ( http://downloads.xiph.org/releases/ogg/ ) and libVorbis
  ( http://downloads.xiph.org/releases/vorbis/ ) for access to Ogg/OGM
  files and Vorbis support
- zlib ( http://www.zlib.net/ ) -- a compression library
- Boost's "format" and "RegEx" libraries ( http://www.boost.org/ )

Other libraries are optional and only limit the features that are
built. These include:

- wxWidgets ( http://www.wxwidgets.org/ ) -- a cross-platform GUI
  toolkit. You need this if you want to use mmg (the mkvmerge GUI) or
  mkvinfo's GUI.
- libFLAC ( http://downloads.xiph.org/releases/flac/ ) for FLAC
  support (Free Lossless Audio Codec)
- lzo ( http://www.oberhumer.com/opensource/lzo/ ) and bzip2 (
  http://www.bzip.org/ ) are compression libraries. These are the
  least important libraries as almost no application supports Matroska
  content that is compressed with either of these libs. The
  aforementioned zlib is what every program supports.
- libMagic from the "file" package ( http://www.darwinsys.com/file/ )
  for automatic content type detection

* Building libmatroska and libebml

Start with the two libraries. Either get libebml 0.7.7 from
http://dl.matroska.org/downloads/libebml/ and libmatroska 0.8.0 from
http://dl.matroska.org/downloads/libmatroska/ or a fresh copy from the
Subversion repository:

svn co https://svn.matroska.org/svn/matroska/trunk/libebml
svn co https://svn.matroska.org/svn/matroska/trunk/libmatroska

Change to "libebml/make/linux" and run "make staticlib". If you have
root-access then run "make install_headers install_staticlib" as
"root" in order to install the files. Change to
"libmatroska/make/linux". Once more run "make staticlib". If you have
root-access then run "make install_headers install_staticlib" as
"root" in order to install the files.

Note that if you don't want the libraries to be installed in
/usr/local/lib and the headers in /usr/local/include then you can
alter the prefix (which defaults to /usr/local) by adding an argument
"prefix=/usr" to the install "make" command. Example:

make prefix=/usr install_headers install_staticlib

* Building MKVtoolNix

Either download the current release from
http://www.bunkus.org/videotools/mkvtoolnix/ and unpack it or get a
development snapshot from my Git repository.

 - Getting and building a development snapshot (ignore this subsection
   if you want to build from a release tarball)

   All you need for Git repository access is to download a Git client
   from the Git homepage at http://git-scm.com/ . There are clients
   for both Unix/Linux and Windows.

   First clone my Git repository with this command:

   git clone git://git.bunkus.org/mkvtoolnix.git

   Now change to the MKVtoolNix directory with "cd mkvtoolnix" and run
   "./autogen.sh" which will generate the "configure" script. You need
   the GNU "autoconf" utility for this step.

If you have run "make install" for both libraries then "configure"
should automatically find the libraries' position. Otherwise you need
to tell "configure" where the "libebml" and "libmatroska" include and
library files are:

./configure \
  --with-extra-includes=/where/i/put/libebml\;/where/i/put/libmatroska \
  --with-extra-libs=/where/i/put/libebml/make/linux\;/where/i/put/libmatroska/make/linue

Now run "make" and, as "root", "make install".

Example
-------

Here's a *very* brief example of how you could use mkvmerge
with mencoder in order to rip a DVD:

a) Extract the audio to PCM audio:

mplayer -ao pcm:file=audio.wav -vo null -vc dummy dvd://1

b) Normalize the sound (optional)

normalize audio.wav

c) Encode the audio to Vorbis:

oggenc -q3 -oaudio-q3.ogg audio.wav

d) Somehow calculate the bitrate for your video. Use something like...

video_size = (target_size - audio-size) / 1.005
video_bitrate = video_size / length / 1024 * 8

target_size, audio_size in bytes
length in seconds
1.005 is the overhead caused by putting the streams into an Matroska file
  (about 0.5%, that's correct ;)).
video_bitrate will be in kbit/s

e) Use the two-pass encoding for the video:

mencoder -oac copy -ovc lavc \
  -lavcopts vcodec=mpeg4:vbitrate=1000:vhq:vqmin=2:vpass=1 \
  -vf scale=....,crop=..... \
  -o /dev/null dvd://1

mencoder -oac copy -ovc lavc \
  -lavcopts vcodec=mpeg4:vbitrate=1000:vhq:vqmin=2:vpass=2 \
  -vf scale=....,crop=..... \
  -o movie.avi dvd://1

f) Merge:

mkvmerge -o movie.mkv -A movie.avi audio-q3.ogg

-A is necessary in order to avoid copying the raw PCM (or MP3) audio as well.

Bug reports
-----------

If you're sure you've found a bug - e.g. if one of my programs crashes
with an obscur error message, or if the resulting file is missing part
of the original data, then by all means submit a bug report.

I use Bugzilla (https://www.bunkus.org/bugzilla/) as my bug
database. You can submit your bug reports there. Please be as verbose
as possible - e.g. include the command line, if you use Windows or
Linux etc.pp. If at all possible please include sample files as well
so that I can reproduce the issue. If they are larger than 1M then
please upload them somewhere and post a link in the Bugzilla bug
report.
