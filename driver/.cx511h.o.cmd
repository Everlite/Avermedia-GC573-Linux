savedcmd_cx511h.o := ld.lld -m elf_x86_64 -mllvm -import-instr-limit=5 --mllvm=-enable-fs-discriminator=true --mllvm=-improved-fs-discriminator=true -plugin-opt=thinlto -plugin-opt=-split-machine-functions -z noexecstack   -r -o cx511h.o @cx511h.mod  ; /usr/lib/modules/6.19.10-1-cachyos/build/tools/objtool/objtool --hacks=jump_label --hacks=noinstr --hacks=skylake --ibt --mcount --mnop --orc --retpoline --rethunk --sls --static-call --uaccess --prefix=16  --link  --module cx511h.o

cx511h.o: $(wildcard /usr/lib/modules/6.19.10-1-cachyos/build/tools/objtool/objtool)
