
import sys

def count_pattern(filename, patterns):
    with open(filename, 'rb') as f:
        data = f.read()
    
    for name, pattern in patterns.items():
        count = data.count(pattern)
        print(f"{name}: {count}")

patterns = {
    "prologue_gs_mov": b'\x65\x48\x8b\x04\x25\x28\x00\x00\x00',
    "epilogue_gs_xor": b'\x65\x48\x33\x04\x25\x28\x00\x00\x00'
}

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <library>", file=sys.stderr)
        sys.exit(2)
    count_pattern(sys.argv[1], patterns)
