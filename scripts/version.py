import sys
import time

version = time.strftime("%y.%m.") + sys.argv[1]

with open("src/version.hpp", "w") as f:
    f.write(f'#define FSTREE_VERSION "{version}"')

# Update pyproject.toml with the new version number
with open("pyproject.toml", "r") as f:
    pyproject =  f.read()
with open("pyproject.toml", "w") as f:
    f.write(pyproject.replace('version = "0.1.0"', f'version = "{version}"'))
