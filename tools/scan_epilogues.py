
import sys

def scan_epilogues(filename):
    with open(filename, 'rb') as f:
        data = f.read()
    
    # Prefix for "xor %gs:..." is 65 48 33
    prefix = b'\x65\x48\x33'
    
    print(f"Scanning {filename} for epilogue prefix {prefix.hex()}...")
    
    offset = 0
    count = 0
    variants = {}
    
    while True:
        offset = data.find(prefix, offset)
        if offset == -1:
            break
            
        # Get the full instruction (expecting 9 bytes)
        instruction = data[offset:offset+9]
        hex_instr = instruction.hex()
        
        if hex_instr not in variants:
            variants[hex_instr] = 0
        variants[hex_instr] += 1
        
        count += 1
        offset += 1
        
    print(f"Found {count} occurrences of prefix {prefix.hex()}")
    for instr, cnt in variants.items():
        print(f"Variant {instr}: {cnt}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <library>", file=sys.stderr)
        sys.exit(2)
    scan_epilogues(sys.argv[1])
