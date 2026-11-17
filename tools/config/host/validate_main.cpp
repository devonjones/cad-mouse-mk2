// Host harness: reads a 556-byte config block on stdin, prints the
// TuningStatus the firmware's boot validation would assign it.
#include <stdio.h>

#include "../../../firmware/src/TuningStore.cpp"

int main() {
  TuningConfig c;
  if (fread(&c, 1, sizeof(c), stdin) != sizeof(c)) return 2;
  printf("%d\n", static_cast<int>(validate(c)));
  return 0;
}
