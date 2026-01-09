import struct
import random

def generate_fvecs(filename, num_vectors, dim):
    with open(filename, 'wb') as f:
        for _ in range(num_vectors):
            # Write dimension
            f.write(struct.pack('I', dim))
            # Write floats
            data = [random.random() for _ in range(dim)]
            f.write(struct.pack(f'{dim}f', *data))

if __name__ == '__main__':
    generate_fvecs('sample.fvecs', 100, 128)
    print("Created sample.fvecs with 100 vectors of dimension 128")
