#include <stdio.h>
#include <unistd.h>

int main() {
  char buffer[8192];

  buffer[8191] = 0;
  getcwd(buffer, 8191);
  puts(buffer);

  return 0;
}
