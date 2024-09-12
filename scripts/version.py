import sys

with open("src/version.hpp", "w") as f:
    f.write(f'#define FSTREE_VERSION "{sys.argv[1]}"')
