#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char** argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);
  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, "clang++", "-Wall", "-g", "-O0", "-std=c++11");
  nob_cmd_append(&cmd, "main.cpp", "shaders.cpp", "window.cpp");
  nob_cmd_append(&cmd, "-lglfw", "-lGLEW", "-lGL", "-lX11", "-ldl", "-lm");
  nob_cmd_append(&cmd, "-o", "./build/v3_viewer");
  if (!nob_cmd_run(&cmd)) return 1;
  return 0;
}
