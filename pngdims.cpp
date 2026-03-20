// Quick tool to print PNG dimensions — build with:
//   c++ -std=c++17 -o pngdims pngdims.cpp && ./pngdims game_assets/backgrounds/2.png
#include <cstdio>
#include <cstdint>
#include <arpa/inet.h>
int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        FILE* f = fopen(argv[i], "rb");
        if (!f) { printf("%s: can't open\n", argv[i]); continue; }
        uint8_t sig[8]; fread(sig, 1, 8, f);
        uint32_t len; fread(&len, 4, 1, f);
        uint8_t type[4]; fread(type, 1, 4, f);
        uint32_t w, h; fread(&w, 4, 1, f); fread(&h, 4, 1, f);
        w = ntohl(w); h = ntohl(h);
        printf("%s: %ux%u\n", argv[i], w, h);
        fclose(f);
    }
}
