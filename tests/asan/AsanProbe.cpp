#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    volatile int* values = new int[8];
    for (int i = 0; i < 8; ++i) {
        values[i] = i;
    }

    int index = 7;
    if (argc > 1 && std::strcmp(argv[1], "oob") == 0) {
        index = 8;
    }

    values[index] = 42;
    std::printf("probe=%d index=%d\n", values[index], index);
    delete[] values;
    return 0;
}
