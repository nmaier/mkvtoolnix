#!/bin/awk -f

BEGIN {
  FS = "|";
}

!/qaa-qtz/ {
  printf("  { %-83s \"%s\", %s, %s },\n", "\""$4"\",", $1, $3 ? "\""$3"\"" : "NULL", $2 ? "\""$2"\"" : "NULL ");
}

END {
  printf("  { %-83s %s,  %s, %s  },\n", "NULL,", "NULL", "NULL", "NULL");
}
