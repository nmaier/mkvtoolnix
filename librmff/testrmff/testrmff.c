#include <stdio.h>
#include <string.h>

#include "librmff.h"

char buffer[128000];

void
test_reading(const char *file_name) {
  rmff_file_t *file;
  rmff_frame_t *frame;
  int frame_no;

  file = rmff_open_file(file_name, MB_OPEN_MODE_READING);
  if (file == NULL) {
    printf("Could not open %s\n", file_name);
    return;
  }
  printf("Opened %s, reading headers... ", file_name);
  if (rmff_read_headers(file) != RMFF_ERR_OK) {
    printf("failed. Error code: %d, error message: %s\n",
           rmff_last_error, rmff_last_error_msg);
    return;
  }
  printf("done.\nNumber of tracks: %u, number of packets: %u\nNow reading "
         "all frames.\n", file->num_tracks, file->num_packets_in_chunk);
  frame_no = 0;
  do {
    printf("frame %d expected size: %d", frame_no,
           rmff_get_next_frame_size(file));
    if ((frame = rmff_read_next_frame(file, buffer)) != NULL) {
      printf(", read ok with size %d", frame->size);
      rmff_release_frame(frame);
    }
    printf("\n");
    frame_no++;
  } while (frame != NULL);
  rmff_close_file(file);
}

int
main(int argc,
     char *argv[]) {
  int i;

  if (argc < 2)
    test_reading("readtestvideo.rm");
  else
    for (i = 1; i < argc; i++)
      test_reading(argv[i]);

  return 0;
}
