// Host harness for test_config_tools.py. Modes:
//   --offsets         print each TuningConfig field's offset
//   --defaults-sane   print 1 if the compiled defaults pass valuesSane()
//   --load            read a 556-byte block on stdin, run loadTuningFrom(),
//                     print "<status> <blob|defaults|other>" for tuning()
//   (no args)         read a block on stdin, print validate()'s status
#include <stdio.h>
#include <string.h>

#include "../../../firmware/src/TuningStore.cpp"

#define FIELD(f) printf(#f " %zu\n", offsetof(TuningConfig, f))

int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "";
  if (strcmp(mode, "--offsets") == 0) {
    FIELD(magic); FIELD(version); FIELD(reserved); FIELD(sign_axis);
    FIELD(decouple); FIELD(trim_pos); FIELD(trim_neg); FIELD(dead_axis);
    FIELD(curve_expo); FIELD(cross_pos); FIELD(cross_neg);
    FIELD(smooth_tau_s); FIELD(led_default_color_index); FIELD(crc32);
    return 0;
  }
  if (strcmp(mode, "--defaults-sane") == 0) {
    printf("%d\n", valuesSane(makeDefaults()) ? 1 : 0);
    return 0;
  }
  TuningConfig c;
  if (fread(&c, 1, sizeof(c), stdin) != sizeof(c)) return 2;
  if (strcmp(mode, "--load") == 0) {
    const TuningConfig defaults = makeDefaults();
    loadTuningFrom(&c);
    const char* active =
        memcmp(&tuning(), &c, sizeof(c)) == 0          ? "blob"
        : memcmp(&tuning(), &defaults, sizeof(c)) == 0 ? "defaults"
                                                       : "other";
    printf("%d %s\n", static_cast<int>(tuningStatus()), active);
    return 0;
  }
  printf("%d\n", static_cast<int>(validate(c)));
  return 0;
}
