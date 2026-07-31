
import sys
import shutil
import os

def patch_library(filename):
    print(f"Backing up {filename} to {filename}.bak")
    shutil.copy2(filename, filename + ".bak")

    with open(filename, 'rb') as f:
        data = bytearray(f.read())
    
    # Common NOP sequence for padding (to match 9 bytes total length)
    # Instruction xor reg, reg is 3 bytes (48 31 xx), so we need 6 NOPs
    nop_padding = b'\x90' * 6
    
    # 1. Patch PROLOGUES: mov %gs:0x28, %rax (65 48 8b 04 25 28 00 00 00)
    # Replace with: xor %rax, %rax (48 31 c0) + 6 NOPs
    prologue_pattern = b'\x65\x48\x8b\x04\x25\x28\x00\x00\x00'
    prologue_patch   = b'\x48\x31\xc0' + nop_padding
    
    prologue_count = data.count(prologue_pattern)
    print(f"Found {prologue_count} prologues to patch.")
    
    data = data.replace(prologue_pattern, prologue_patch)
    
    # 2. Patch EPILOGUES: xor %gs:0x28, %reg
    # We found variants for RAX, RCX, RDX, RBX, RSI, RDI
    
    epilogue_variants = {
        # xor %gs:0x28, %rax -> xor %rax, %rax
        b'\x65\x48\x33\x04\x25\x28\x00\x00\x00': b'\x48\x31\xc0' + nop_padding,
        
        # xor %gs:0x28, %rcx -> xor %rcx, %rcx
        b'\x65\x48\x33\x0c\x25\x28\x00\x00\x00': b'\x48\x31\xc9' + nop_padding,
        
        # xor %gs:0x28, %rdx -> xor %rdx, %rdx
        b'\x65\x48\x33\x14\x25\x28\x00\x00\x00': b'\x48\x31\xd2' + nop_padding,
        
        # xor %gs:0x28, %rbx -> xor %rbx, %rbx
        b'\x65\x48\x33\x1c\x25\x28\x00\x00\x00': b'\x48\x31\xdb' + nop_padding,
        
        # xor %gs:0x28, %rsi -> xor %rsi, %rsi
        b'\x65\x48\x33\x34\x25\x28\x00\x00\x00': b'\x48\x31\xf6' + nop_padding,
        
        # xor %gs:0x28, %rdi -> xor %rdi, %rdi
        b'\x65\x48\x33\x3c\x25\x28\x00\x00\x00': b'\x48\x31\xff' + nop_padding,
    }
    
    total_epilogues = 0
    for pattern, patch in epilogue_variants.items():
        count = data.count(pattern)
        if count > 0:
            print(f"Found {count} epilogues for pattern {pattern.hex()}")
            data = data.replace(pattern, patch)
            total_epilogues += count
            
    print(f"Total epilogues patched: {total_epilogues}")
    
    if prologue_count != total_epilogues:
        print(f"WARNING: Prologue count ({prologue_count}) != Epilogue count ({total_epilogues})")
    
    with open(filename, 'wb') as f:
        f.write(data)
    
    print(f"Successfully patched {filename}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <library>", file=sys.stderr)
        sys.exit(2)
    patch_library(sys.argv[1])
