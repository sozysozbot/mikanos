#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../syscall.h"

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(200, 100, 10, 10, "banmis");
  if (err_openwin) {
    exit(err_openwin);
  }

  SyscallWinWriteString(layer_id, 8, 24, 0xc00000, "ban missen tonir");
  SyscallWinWriteString(layer_id, 24, 40, 0x00c000, "l'es birleen");
  SyscallWinWriteString(layer_id, 40, 56, 0x0000c0, "alefis io");

  AppEvent events[1];
  while (true) {
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      printf("mi nix mels xelo voleso. %s\n", strerror(err));
      break;
    }
    if (events[0].type == AppEvent::kQuit) {
      break;
    } else if (events[0].type == AppEvent::kMouseMove ||
        events[0].type == AppEvent::kMouseButton ||
        events[0].type == AppEvent::kKeyPush) {
      // ignore
    } else {
      printf("qunenerfe volesosti. akrapt es %d\n", events[0].type);
    }
  }
  SyscallCloseWindow(layer_id);
  exit(0);
}
