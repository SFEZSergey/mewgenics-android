#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc < 3) return 1;
    uint32_t target_va = (uint32_t)strtoul(argv[2], NULL, 16);
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* b = (uint8_t*)malloc(sz);
    fread(b, 1, sz, f);
    fclose(f);

    uint32_t text_start = 0x4000;
    uint32_t text_end = 0x13bd7fc;
    for (uint32_t off = text_start; off < text_end - 8; ++off) {
        for (int insn_len = 5; insn_len <= 8; ++insn_len) {
            int32_t disp = *(int32_t*)(b + off + insn_len - 4);
            uint32_t rip = (off - text_start) + insn_len;
            if (rip + disp == target_va) {
                printf("VA 0x%x (file 0x%x, len %d): ", off - text_start, off, insn_len);
                for (int i = 0; i < insn_len; ++i) printf("%02x ", b[off+i]);
                printf("\n");
            }
        }
    }
    return 0;
}
