# This script converts the output of ebml_validator to match the
# debugging output of the kax_analyzer_c::debug_dump_elements()
# function.

# Usage: ebml_validator -M file.mkv | awk -f validator_to_kax_analyzer_dump.awk > output.txt

# Format of kax_analyzer_c::dump_elements():
# 10: Cluster size 358802 at 1489318

# Format of ebml_validator:
#  pos 5719 id 0x1f43b675 size 395532 header size 7 (Cluster)

BEGIN {
  idx = 0;
}

/^ [a-zA-Z]/ {
  pos  = $2;
  size = $6 + $9;
  name = $10;

  gsub(/[\(\)]/, "", name);

  if (name == "EBML::Void") {
    name = "EBMLVoid";
  } else if (name == "SeekHead") {
    name = "SeekHeader";
  }

  print idx ": " name " size " size " at " pos;
  idx = idx + 1;
}
