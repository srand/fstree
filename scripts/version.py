import sys
import time

version = time.strftime("%y.%m.") + sys.argv[1]

with open("src/version.hpp", "w") as f:
    f.write(f'#define FSTREE_VERSION "{version}"')
