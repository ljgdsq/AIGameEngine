#include "Aster/Editor/EditorApp.h"

#include <cstdio>

int main(int argc, char** argv)
{
    Aster::EditorApp application;
    const int result = application.Run(argc, argv);
    if (result != 0)
    {
        std::fprintf(stderr, "Aster Editor exited with code %d\n", result);
    }
    return result;
}
