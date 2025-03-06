#!/usr/bin/env python3
"""Summarizes gas differences from semantic test diff.

The script collects all gas differences present in git diff of semantic tests
and summarizes them in the form of a table that's ready to post in a GitHub comment.
Only changes that are already committed are taken into account.
Changes that are only staged or not staged or committed at all are ignored.

Useful for reviewing the gas impact of a specific PR / branch.
Instead of tediously going through each individual change, it's recommended to review the table.

Dependencies: parsec, tabulate
"""

import subprocess
import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from pathlib import Path
from enum import Enum
from parsec import generate, ParseError, regex, string, optional
from tabulate import tabulate
from tqdm import tqdm


class Kind(Enum):
    Ir = 1
    IrOptimized = 2
    Legacy = 3
    LegacyOptimized = 4

class Diff(Enum):
    Minus = 1
    Plus = 2

SEMANTIC_TEST_DIR = Path("test/libsolidity/semanticTests/")

minus = string("-").result(Diff.Minus)
plus = string("+").result(Diff.Plus)

space = string(" ")
comment = string("//")
colon = string(":")

gas_ir = string("gas ir").result(Kind.Ir)
gas_ir_optimized = string("gas irOptimized").result(Kind.IrOptimized)
gas_legacy_optimized = string("gas legacyOptimized").result(Kind.LegacyOptimized)
gas_legacy = string("gas legacy").result(Kind.Legacy)
code_suffix = string("code")

def number() -> int:
    """Parse number."""
    return regex(r"([0-9]*)").parsecmap(int)

@generate
def diff_string() -> (Kind, Diff, int):
    """Usage: diff_string.parse(string)

    Example string:

    -// gas irOptimized: 138070

    """
    diff_kind = yield minus | plus
    yield comment
    yield space
    codegen_kind = yield gas_ir_optimized ^ gas_ir ^ gas_legacy_optimized ^ gas_legacy
    yield optional(space)
    yield optional(code_suffix)
    yield colon
    yield space
    val = yield number()
    return (diff_kind, codegen_kind, val)

def collect_statistics(lines, code) -> (int, int, int, int, int, int):
    """Returns

    (old_ir_optimized, old_legacy_optimized, old_legacy, new_ir_optimized,
    new_legacy_optimized, new_legacy)

    All the values in the same file (in the diff) are summed up.

    """
    if not lines:
        raise RuntimeError("Empty list")

    out = [
        parsed
        for line in lines
        if line.startswith('+// gas ') or line.startswith('-// gas ')
        if (code and " code: " in line) or (not code and " code: " not in line)
        if (parsed := diff_string.parse(line)) is not None
    ]
    diff_kinds = [Diff.Minus, Diff.Plus]
    codegen_kinds = [Kind.IrOptimized, Kind.LegacyOptimized, Kind.Legacy]
    return tuple(
        sum(
            val
            for (diff_kind, codegen_kind, val) in out
            if diff_kind == _diff_kind and codegen_kind == _codegen_kind
        )
        for _diff_kind in diff_kinds
        for _codegen_kind in codegen_kinds
    )

def semantictest_statistics(base_branch: str):
    """Prints the tabulated statistics that can be pasted in github."""
    def parse_git_diff(fname, code):
        args = ["git", "diff", "--cached", "--unified=0", base_branch, "--", fname]
        # print("Executing", " ".join(args))
        diff_output = subprocess.check_output(
            args,
            universal_newlines=True
        ).splitlines()
        if len(diff_output) == 0:
            return None
        return collect_statistics(diff_output, code)

    def percent(old, new):
        return (int(new) - int(old)) / int(old) * 100 if int(old) != 0 else None

    def percent_or_zero(old, new):
        result = percent(old, new)
        return result if result is not None else 0

    def format_percent(percentage):
        if percentage is None:
            return ''
        return f'{percentage:.3f}%'

    def stat(old, new):
        return format_percent(percent(old, new))

    if not SEMANTIC_TEST_DIR.is_dir():
        sys.exit(f"Semantic tests not found. '{SEMANTIC_TEST_DIR.absolute()}' is missing or not a directory.")

    table = []
    test_files = list(SEMANTIC_TEST_DIR.rglob("*.sol"))
    for path in tqdm(test_files):
        fname = path.as_posix()
        parsed_deploy = parse_git_diff(fname, True)
        parsed_runtime = parse_git_diff(fname, False)
        if not parsed_deploy and not parsed_runtime:
            continue
        parsed_changes = []
        for parsed in [parsed_runtime, parsed_deploy]:
            assert len(parsed) == 6
            ir_optimized = stat(parsed[0], parsed[3])
            legacy_optimized = stat(parsed[1], parsed[4])
            legacy = stat(parsed[2], parsed[5])
            fname = f"`{fname.split('/', 3)[-1]}`"
            average = ((
                percent_or_zero(parsed[0], parsed[3]) +
                percent_or_zero(parsed[1], parsed[4]) +
                percent_or_zero(parsed[2], parsed[5])
            ) / 3)
            parsed_changes += [average, fname, ir_optimized, legacy_optimized, legacy]
        table += [parsed_changes]
    import numpy as np
    table_data = np.array(table)
    sort_indices = np.argsort(table_data[:, 0].astype(float))[::-1]
    sorted_table = table_data[sort_indices][:, np.array([1,7,2,8,3,9])]
    # sorted_table = [row[0][1:] for row in sorted(table, reverse=True)]

    if table:
        print("<details><summary>Click for a table of gas differences</summary>\n")
        table_header = ["File name", "IR optimized", "IR optimized deploy", "Legacy optimized", "Legacy optimized deploy", "Legacy", "Legacy deploy"]
        print(tabulate(sorted_table, headers=table_header, tablefmt="github"))
        print("</details>")
    else:
        print("No differences found.")

def main():
    parser = ArgumentParser(description=__doc__, formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument(
        '--base',
        dest='base_branch',
        default='origin/develop',
        help='The base branch to diff against. default: origin/develop',
    )
    options = parser.parse_args()

    try:
        semantictest_statistics(options.base_branch)
    except subprocess.CalledProcessError as exception:
        sys.exit(f"Error in the git diff:\n{exception.output}")
    except ParseError as exception:
        sys.exit(
            f"ParseError: {exception}\n\n"
            f"{exception.text}\n"
            f"{' ' * exception.index}^\n"
        )

if __name__ == "__main__":
    main()
