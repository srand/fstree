#!/usr/bin/env python3
"""
Compares two fstree trees and prints what differs between them.

Two writes of what should be the same content can produce different tree
digests, for instance when the files were checked out with different umasks.
This script finds the entries responsible.

    python3 scripts/compare_trees.py [-r] <tree-a> <tree-b>

As git diff-tree, it compares the entries at the top of the trees, and with
-r descends into the directories whose digests differ, which is quick even
for large trees, and lists everything below a directory only in one tree.

Both trees must be in the local cache. Use fstree pull to fetch a tree from a
remote first. The trees may be in different caches, given with --cache and
--cache-b, such as caches copied from two machines.

Each difference is printed on a line of its own:

    only in a: <path>
    only in b: <path>
    type:      <path>  <mode a> <mode b>
    content:   <path>
    target:    <path>  <target a> <target b>
    mode:      <path>  <mode a> <mode b>  [umask?]

A mode difference is marked umask? when the modes differ only in the write
bits of the group and others, as umasks such as 022 and 002 make them differ.
Without -r, a directory whose content differs is reported as content.

The exit status is 0 if the trees are equal, 1 if they differ, and 2 on error.
"""

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass


# Write permission of the group and others
UMASK_BITS = 0o022

# A line of fstree ls-tree: the digest, padded, which a symlink has none of,
# the mode, such as -rw-r--r--, and the name, followed by the target of a
# symlink.
LS_TREE_LINE = re.compile(r"^(\S*)\s+([-dl][-rwxsStT]{9}) (.*)$")


@dataclass
class Entry:
    digest: str
    mode: str
    target: str = None

    @property
    def type(self):
        return self.mode[0]

    @property
    def perms(self):
        bits = 0
        for c in self.mode[1:]:
            bits = (bits << 1) | (c != "-")
        return bits


class Trees:
    def __init__(self, fstree, cache):
        self.fstree = fstree
        self.cache = cache

    def list(self, digest):
        """Returns the entries of a tree object by name."""
        cmd = [self.fstree]
        if self.cache:
            cmd += ["--cache", self.cache]
        cmd += ["ls-tree", digest]

        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if result.returncode != 0:
            message = result.stderr.decode(errors="replace").strip()
            raise RuntimeError(re.sub(r"^error: ", "", message) or "fstree ls-tree failed: " + digest)

        entries = {}
        for line in result.stdout.decode("utf-8", errors="surrogateescape").splitlines():
            match = LS_TREE_LINE.match(line)
            if not match:
                raise RuntimeError("unexpected output from fstree ls-tree: " + line)
            digest_, mode, name = match.groups()
            target = None
            if mode.startswith("l"):
                name, _, target = name.partition(" -> ")
            entries[name] = Entry(digest_, mode, target)
        return entries


def join(parent, name):
    return parent + "/" + name if parent else name


def list_all(trees, digest, parent, kind, report):
    """Reports every entry below a directory found in one tree only."""
    for name, entry in sorted(trees.list(digest).items()):
        path = join(parent, name)
        report(kind, path)
        if entry.type == "d":
            list_all(trees, entry.digest, path, kind, report)


def compare(trees_a, trees_b, digest_a, digest_b, parent, recursive, report):
    a = trees_a.list(digest_a)
    b = trees_b.list(digest_b)

    for name in sorted(a.keys() | b.keys()):
        path = join(parent, name)
        ea, eb = a.get(name), b.get(name)

        if ea is None or eb is None:
            kind, trees, entry = ("only in a", trees_a, ea) if eb is None else ("only in b", trees_b, eb)
            report(kind, path)
            if recursive and entry.type == "d":
                list_all(trees, entry.digest, path, kind, report)
            continue

        if ea.type != eb.type:
            report("type", path, ea.mode, eb.mode)
            continue

        if ea.mode != eb.mode:
            diff = ea.perms ^ eb.perms
            note = "umask?" if diff & ~UMASK_BITS == 0 else ""
            report("mode", path, ea.mode, eb.mode, note)

        if ea.type == "l":
            if ea.target != eb.target:
                report("target", path, ea.target, eb.target)
        elif ea.digest != eb.digest:
            if ea.type == "d" and recursive:
                compare(trees_a, trees_b, ea.digest, eb.digest, path, recursive, report)
            else:
                report("content", path)


def main():
    parser = argparse.ArgumentParser(
        description="Compare two fstree trees and print what differs.")
    parser.add_argument("tree_a", help="digest of the first tree")
    parser.add_argument("tree_b", help="digest of the second tree")
    parser.add_argument("-r", "--recursive", action="store_true",
                        help="descend into directories")
    parser.add_argument("--cache", help="cache holding the trees, or the first tree "
                        "if --cache-b is given (default: fstree's default cache)")
    parser.add_argument("--cache-b", help="cache holding the second tree")
    parser.add_argument("--fstree", default="fstree", help="fstree executable (default: fstree)")
    args = parser.parse_args()

    trees_a = Trees(args.fstree, args.cache)
    trees_b = Trees(args.fstree, args.cache_b or args.cache)

    counts = {}

    def report(kind, path, *details):
        counts[kind] = counts.get(kind, 0) + 1
        line = "{:<11}{}".format(kind + ":", path)
        details = [d for d in details if d]
        if details:
            line += "  " + " ".join(details)
        print(line)

    try:
        if args.tree_a != args.tree_b:
            compare(trees_a, trees_b, args.tree_a, args.tree_b, "", args.recursive, report)
    except (OSError, RuntimeError) as e:
        sys.stdout.flush()
        print("error: " + str(e), file=sys.stderr)
        return 2

    sys.stdout.flush()

    if not counts:
        if args.tree_a != args.tree_b:
            # Equal entries make equal tree objects, unless their format differs
            print("no differences in entries, the trees differ in format or hash algorithm",
                  file=sys.stderr)
            return 1
        return 0

    print("\n" + ", ".join("{} {}".format(n, k) for k, n in sorted(counts.items())),
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
