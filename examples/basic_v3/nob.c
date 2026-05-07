#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "./"

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "clang++", "-Wall", "-g", "-O0", "-std=c++11");
    nob_cmd_append(&cmd, "-Wno-unused-function", "-Wno-unused-but-set-variable",
                   "-Wno-unused-variable");
    nob_cmd_append(&cmd, "main.cpp", "shaders.cpp", "window.cpp");
    nob_cmd_append(&cmd, "-lglfw", "-lGLEW", "-lGL", "-lX11", "-ldl", "-lm");
    nob_cmd_append(&cmd, "-o", BUILD_FOLDER"v3_viewer");
    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}
