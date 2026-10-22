#!/usr/bin/env python3
"""
Generates test/test_gitignore.txt, the expected results of matching paths
against .gitignore patterns, as reported by git itself.

fstree matches ignore files with the semantics of .gitignore. Each case below
is a list of .gitignore lines and a list of paths, files or directories. For
each case, the script creates a repository holding those paths, writes the
lines to its .gitignore, and asks git check-ignore which paths are ignored.
The C++ tests then check that fstree agrees, without needing git.

Run it again after adding cases:

    python3 scripts/gitignore_cases.py > test/test_gitignore.txt

File format, one item per line:

    case <name>
    p "<pattern line>"
    <f|d> <0|1> "<path>"
    end

where f is a file, d a directory, and 1 means ignored. Within quotes, bytes
below 0x20, 0x7f, '"' and '%' are written as %XX.
"""

import os
import subprocess
import sys
import tempfile

# (name, .gitignore lines, paths). A path ending in "/" is a directory, any
# other a file. Parent directories are created as needed.
CASES = [
    ("literal name matches at any level",
     ["foo"],
     ["foo", "a/foo", "a/b/foo", "foobar", "barfoo", "dir/foo/", "dir/foo/x"]),
    ("leading slash anchors to the root",
     ["/foo"],
     ["foo/", "foo/x", "a/foo", "afoo"]),
    ("slash in the middle anchors to the root",
     ["a/foo"],
     ["a/foo/", "a/foo/z", "x/a/foo", "a/b/foo"]),
    ("trailing slash matches directories only",
     ["foo/"],
     ["foo", "x/foo/", "x/foo/inner", "y/foo"]),
    ("trailing slash with a slash in the middle",
     ["a/foo/"],
     ["a/foo/", "a/foo/x", "b/a/foo/", "c/a/foo"]),
    ("leading and trailing slash",
     ["/out/"],
     ["out/", "out/x", "a/out/", "out2/"]),
    ("star matches within a name",
     ["*.log"],
     ["a.log", "x/a.log", "log", ".log", "a.log.txt", "d.log/", "d.log/x"]),
    ("star does not match a slash",
     ["a/*.c"],
     ["a/x.c", "a/b/x.c", "a/.c", "b/a/x.c"]),
    ("anchored star",
     ["/*.c"],
     ["a.c", "d/a.c"]),
    ("star alone",
     ["*"],
     ["a", "d/", "d/b", ".hidden"]),
    ("question mark matches one character but not a slash",
     ["?.txt", "x/a?b"],
     ["a.txt", "ab.txt", ".txt", "x/a/b", "x/acb", "x/ab"]),
    ("character ranges",
     ["file[0-9].txt"],
     ["file1.txt", "filea.txt", "file10.txt", "file.txt"]),
    ("negated character class with !",
     ["[!a]x"],
     ["bx", "ax", "x", ".x"]),
    ("negated character class with ^",
     ["[^a]y"],
     ["by", "ay"]),
    ("closing bracket first in a class",
     ["[]]z"],
     ["]z", "z", "a]z"]),
    ("dash last in a class",
     ["[a-]q"],
     ["aq", "-q", "bq"]),
    ("named character classes",
     ["[[:digit:]]d", "[[:upper:][:punct:]]e"],
     ["1d", "ad", "Ae", "_e", "ae"]),
    ("unterminated class matches nothing",
     ["[ab"],
     ["[ab", "a", "b"]),
    ("escaped special characters",
     ["\\*", "a\\?b", "\\[x]"],
     ["*", "a", "a?b", "axb", "[x]", "x"]),
    ("escaped hash and bang",
     ["\\#hash", "\\!bang"],
     ["#hash", "!bang", "bang"]),
    ("comments and blank lines",
     ["# comment", "", "#not"],
     ["# comment", "#not", "comment"]),
    ("trailing spaces are ignored unless escaped",
     ["trail  ", "sp\\ "],
     ["trail", "trail ", "sp ", "sp"]),
    ("trailing tab is kept",
     ["tab\t"],
     ["tab", "tab\t"]),
    ("carriage return at end of line",
     ["crlf\r"],
     ["crlf", "crlf\r"]),
    ("trailing backslash matches nothing",
     ["foo\\"],
     ["foo", "foo\\"]),
    ("characters special to regular expressions are literal",
     ["a+b", "(x)", "{y}", "$z", "p|q", "dot.c", "c++", "^h"],
     ["a+b", "aab", "(x)", "x", "{y}", "y", "$z", "z", "p|q", "p", "dot.c", "dotxc", "c++", "c", "^h", "h"]),
    ("names are case sensitive",
     ["Foo"],
     ["Foo", "foo", "FOO"]),
    ("negation re-includes",
     ["*.log", "!keep.log"],
     ["a.log", "keep.log", "x/keep.log", "x/a.log"]),
    ("last matching pattern decides",
     ["!keep.log", "*.log"],
     ["keep.log", "a.log"]),
    ("negation cannot re-include below an ignored directory",
     ["build", "!build/keep.txt"],
     ["build/", "build/keep.txt", "build/x"]),
    ("re-including a directory does not re-include its files",
     ["*", "!src"],
     ["src/", "src/main.cpp", "other", "deep/src/"]),
    ("re-including directories and some files",
     ["*", "!*/", "!*.c"],
     ["a.c", "a.h", "d/", "d/a.c", "d/a.h", "d/e/b.c"]),
    ("negated directory pattern",
     ["d*", "!dir/"],
     ["dir/", "dir/x", "dfile", "d2/"]),
    ("leading double star",
     ["**/foo"],
     ["foo", "a/foo", "a/b/foo", "b/foo/x", "xfoo"]),
    ("leading double star with a slash",
     ["**/foo/bar"],
     ["foo/bar", "a/foo/bar", "a/b/foo/bar", "foo/x/bar", "bar"]),
    ("trailing double star matches everything inside",
     ["abc/**"],
     ["abc/", "abc/x", "abc/y/z", "xabc/x", "d/abc/x"]),
    ("double star in the middle matches any number of directories",
     ["a/**/b"],
     ["a/b", "a/x/b", "a/x/y/b", "ab", "a/xb", "c/a/b"]),
    ("double star not alone in a component acts as a star",
     ["a**b", "**foo", "bar**", "x/**y"],
     ["ab", "axb", "c/ab", "foo", "xfoo", "c/xfoo", "bar", "barz", "x/y", "x/ay", "x/a/y"]),
    ("double star alone matches everything",
     ["**"],
     ["a", "d/", "d/e/f"]),
    ("double star with trailing slash matches directories",
     ["**/"],
     ["a", "d/", "d/e/"]),
    ("star with trailing slash matches directories",
     ["*/"],
     ["f", "d/", "d/e/", "d/f"]),
    ("lone slash and lone bang match nothing",
     ["/", "!"],
     ["a", "d/"]),
    ("names starting with a dot",
     [".svn"],
     [".svn/", ".svn/entries", "a/.svn/", "a/.svnignore"]),
    ("typical ignore file",
     ["build/", "*.pyc", "__pycache__", ".git", "node_modules/", "*.o", "/artifacts", "**/tmp/**"],
     ["build/", "src/build/", "lib/build", "src/a.pyc", "src/__pycache__/", "src/__pycache__/x.pyc",
      "node_modules/", "web/node_modules/", "src/a.o", "artifacts/", "src/artifacts/",
      "tmp/x", "src/tmp/x", "src/tmp/", "src/main.c"]),
]


def encode(s):
    out = []
    for b in s.encode("utf-8", "surrogateescape"):
        if b < 0x20 or b == 0x7F or b in (ord('"'), ord("%")):
            out.append("%{:02X}".format(b))
        else:
            out.append(chr(b))
    return '"' + "".join(out) + '"'


def git(*args, cwd, env, data=None):
    return subprocess.run(
        ["git", *args], cwd=cwd, env=env, input=data, check=True, capture_output=True).stdout


def run_case(lines, paths, workdir, env):
    repo = os.path.join(workdir, "repo")
    git("init", "-q", repo, cwd=workdir, env=env)
    git("config", "core.ignorecase", "false", cwd=repo, env=env)

    with open(os.path.join(repo, ".gitignore"), "wb") as f:
        f.write("".join(line + "\n" for line in lines).encode())

    entries = []
    for path in paths:
        is_dir = path.endswith("/")
        name = path.rstrip("/")
        full = os.path.join(repo, name)
        if is_dir:
            os.makedirs(full, exist_ok=True)
        else:
            os.makedirs(os.path.dirname(full), exist_ok=True)
            open(full, "w").close()
        entries.append((name, is_dir))

    query = b"".join(name.encode() + b"\0" for name, _ in entries)
    result = subprocess.run(
        ["git", "check-ignore", "--no-index", "--stdin", "-z"],
        cwd=repo, env=env, input=query, capture_output=True)
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.decode())
    ignored = set(result.stdout.decode().split("\0"))

    return [(name, is_dir, name in ignored) for name, is_dir in entries]


def main():
    version = subprocess.run(["git", "--version"], capture_output=True, check=True).stdout.decode().strip()
    out = sys.stdout
    out.write("# Generated by scripts/gitignore_cases.py with {}. Do not edit.\n".format(version))

    for name, lines, paths in CASES:
        with tempfile.TemporaryDirectory() as workdir:
            # Keep the user's and system's git configuration out of it
            env = dict(os.environ, HOME=workdir, XDG_CONFIG_HOME=workdir, GIT_CONFIG_NOSYSTEM="1")
            results = run_case(lines, paths, workdir, env)

        out.write("case {}\n".format(name))
        for line in lines:
            out.write("p {}\n".format(encode(line)))
        for path, is_dir, ignored in results:
            out.write("{} {} {}\n".format("d" if is_dir else "f", int(ignored), encode(path)))
        out.write("end\n")


if __name__ == "__main__":
    main()
