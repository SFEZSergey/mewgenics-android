#!/data/data/com.termux/files/usr/bin/bash
# Disassembles the .shader_text of a PSSL ELF as RDNA2 (gfx1030).
W=$(mktemp -d); trap 'rm -rf $W' EXIT
llvm-objcopy -O binary --only-section=.shader_text "$1" $W/t.bin
python3 -I -c "import sys; b=open('$W/t.bin','rb').read(); print(' '.join('0x%02x'%x for x in b))" > $W/t.txt
llvm-mc --disassemble -triple=amdgcn -mcpu=gfx1030 $W/t.txt 2>/dev/null | command grep -v "^\s*\.text"
