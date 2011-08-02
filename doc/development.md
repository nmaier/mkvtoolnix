Developer notes

# File system layout #

There's no dedicated include directory. Each include file is located
in the same directory its accompanying cpp file is located in as well.

Directories in alphabetical order:

* `ac`: Macro snippets used for the `configure` script.
* `contrib`: Spec files for RPM creation and other contributed files.
* `doc/guide`: mmg's guide.
* `doc/man`: man pages. They're written as DocBook XML files and
  converted to HTML and man format automatically.
* `examples`: Examples for the end user.
* `installer`: Files for the Windows installer.
* `lib`: External libraries that are required for building. Should not
  be touched but can be used.
* `po`: Translation of the programs (not of the man pages).
* `share`: Image, desktop files etc.
* `src/common`: Common classes used by all tools. This library is
  linked to each program.
* `src/extract`: mkvextract's main files.
* `src/info`: mkvinfo's main files including its GUI.
* `src/input`: Readers (demux source files). Only used in mkvmerge.
* `src/merge`: mkvmerge's main files, e.g. command line handling,
  output control, file creating etc.
* `src/mmg`: mkvmerge's GUI.
* `src/output`: Packetizers (create the tracks in the output
  file). Only used in mkvmerge.
* `src/propedit`: mkvpropedit's main files.
* `src/scripts` and `src/tools`: Scripts and compiled tools that may
  or may not be useful during development.
* `tests`: Test suite (requires external data package not distributed
  freely)

# Strings & encoding #

Internally all strings are encoded in UTF-8. Strings must be re-coded
during input/output.

# Timecodes #

All timecodes and durations are stored as `int64_t` variables with
nanosecond precision.

# Classes #

## General ##

Most classes are nameed `*_c`, and the corresponding counted pointer
implementation is called `*_cptr`. A counted pointer is a reference
counter implementation that automatically deletes its instance once
the counter reaches zero.

## `memory_c` (from `common/memory.h`) ##

Stores a pointer to a buffer and its size. Can free the memory if the
buffer has been malloced.

Allocating memory:

    memory_cptr mem = memory_c::alloc(12345);

Copying/reading something into it:

    memcpy(mem->get_buffer(), source_pointer, mem->get_size());
    file->read(mem, 20);

## `mm_io_c` (from `common/mm_io.h`) ##

Base class for input/output. Several derived classes implement access
to binary files (`mm_file_io_c`), text files (`mm_text_io_c`,
including BOM handling), memory buffers (`mm_mem_io_c`) etc.

## `mm_file_io_c` (from `common/mm_io.h`) ##

File input/output class. Opening a file for reading and retrieving its
size:

    mm_file_io_cptr file(new mm_file_io_c(file_name));
    int64_t size = file->get_size();

Seeking:

    // Seek to absolute position 123:
    file->setFilePointer(123, seek_beginning);
    // Seek forward 61 bytes:
    file->setFilePointer(61, seek_current);

Reading:

    size_t num_read = file->read(some_buffer, 12345);

## `track_info_c` (from `src/merge/pr_generic.h`) ##

A file and track information storage class. It is created by the
command line parser with the source file name and all the command line
options the user has passed in.

The instance is then passed to the reader which stores a copy in the
`m_ti` variable. The reader also passes that copy to each packetizer
it creates which in turn stores its own copy (again in the `m_ti`
variable).

# Readers #

A reader is a class that demuxes a certain file type. It has to parse
the file content, create one packetizer for each track, report the
progress etc. Each reader class is derived from `generic_reader_c`
(from `src/merge/pr_generic.h`).

An example for a rather minimal implementation is the MP3 reader in
`src/input/r_mp3.h`.

## Constructor & virtual destructor ##

The constructor usually only takes a `track_info_c` argument.  A
virtual destructor must be present, even if it is empty. Otherwise
you'll get linker errors.

The constructor must open the file and parse all its headers so that
all track information has been processed. This will most likely be
split up into a separate function in the future.

## `read(generic_packetizer_c *ptzr, bool force = false)` ##

Requests that the reader reads data for a specific track. mkvmerge
uses a pulling model: the core asks each packetizer to provide
data. The packetizers in turn ask the reader they've been created from
to read data by calling this function.

If the reader supports arbitrary access to track data (e.g. for AVI
and MP4/MOV files) then the reader should only read data for the
requested track in order not to consume too much memory. If the file
format doesn't allow for direct access to that data then the reader
can simply read the next packet regardless which track that packet
belongs to. The packetizer will call the reader's `read()` function as
often as necessary until it has enough data.

The reader must return `FILE_STATUS_MOREDATA` if more data is
available and `FILE_STATUS_DONE` when the end has been reached.

Each data packet is stored in an instance of `packet_c`. If the source
container provides a timecode then that timecode should be set in the
packet as well.

## `create_packetizer(int64_t track_id)` ##

Has to create a packetizer for the track with the specific ID. This ID
is the same number that the user uses on the command line as well as
the number used in the call to `id_result_track()` (see below).

This function has to verify that the user actually wants this track
processed. This is checked with the `demuxing_requested()`
function. Example from the MP3 reader (as the MP3 file format can only
contain one track the ID is always 0; see the Matroska reader for more
complex examples):

    // Check if the audio demuxer with ID 0 is requested. Also make
    // sure that the number of packetizers this reader has created is
    // still 0.
    if (!demuxing_requested('a', 0) || (NPTZR() != 0))
      return;

    // Inform the user.
    mxinfo_tid(m_ti.m_fname, 0, Y("Using the MPEG audio output module.\n"));

    // Create the actual packetizer.
    add_packetizer(new mp3_packetizer_c(this, m_ti, mp3header.sampling_frequency, mp3header.channels, false));

A lot of packetizers expect their codec private data to be constructed
completely by the reader. This often requires that the reader
processes at least a few packets. For a rather complex case see the
MPEG PS reader's handling of h264 tracks.

## `get_progress()` ##

Returns the demuxing progress, a value between 0 and 100. Usually
based on the number of bytes processed.

## `identify()` ##

File identification. Has to call `id_result_container("description")`
once for the container type.

For each supported track the function must call `id_result_track(...)`
once.

Both the container and the track identification can contain extended
information. For an extensive example see the Matroska reader's
identification function in `src/input/r_matroska.cpp`.
