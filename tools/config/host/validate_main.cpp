// Host harness for test_config_tools.py. With --offsets, prints each
// TuningConfig field's offset; otherwise reads a 556-byte config block on
// stdin and prints the TuningStatus the firmware's boot validation assigns.
#include <stdio.h>
#include <string.h>

#include "../../../firmware/src/TuningStore.cpp"

#define FIELD(f) printf(#f " %zu\n", offsetof(TuningConfig, f))

int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "--offsets") == 0) {
    FIELD(magic); FIELD(version); FIELD(reserved); FIELD(sign_axis);
    FIELD(decouple); FIELD(trim_pos); FIELD(trim_neg); FIELD(dead_axis);
    FIELD(curve_expo); FIELD(cross_pos); FIELD(cross_neg);
    FIELD(smooth_tau_s); FIELD(led_default_color_index); FIELD(crc32);
    return 0;
  }
  TuningConfig c;
  if (fread(&c, 1, sizeof(c), stdin) != sizeof(c)) return 2;
  printf("%d\n", static_cast<int>(validate(c)));
  return 0;
}
