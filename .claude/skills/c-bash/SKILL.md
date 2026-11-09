---
name: c-bash
description: make .sh files follow common C code conventions for readability and maintainability
allowed-tools: bash
---

All `.sh` files should follow `.claude/skills/bash-coding-standards.md`

You MUST work on one file at a time, don't do bulk updates since these tend to break scripts subtly.

When replacing `"${var}"` it is _not_ always safe to use `$var`, especially if the variable might contain spaces or unknown input.

It is only safe to use the shortened `$var` syntax when the variable is known to be a simple string value, usually that means its set earlier in the script.

It is also only safe to remove the `{}` if the use of this variable does not cause ambiguity, or if the text following it does not cause bash to interpret it as a different variable.

After making changes to each file one at a time you must `bash -n` the changed script and then run the script to make sure it still works.

## converting non-compliant code

When asked to convert bash to to these conventions...

Never do mass sed/perl replacements across many files at once.

Each change must be read in context first. A variable that looks like a path may hold multi-line output, quoted strings, or JSON. The fix that is correct for one variable may silently corrupt another.

Quotes are load-bearing — check usage before removing them
"$var" is NOT just style. It is required when:
- the variable may contain spaces (file names, URLs with query strings, response bodies)
- the variable may contain newlines (curl response bodies, grep output)
- the variable may contain glob characters (*, ?, [)
- the variable is on the right-hand side of [[ ]] and could contain * or ?

- Only remove quotes when you can see, from reading the script, that none of these apply.
echo "$var" must stay quoted when var is a response body
echo $var word-splits and glob-expands. echo "$var" preserves newlines. Any variable holding HTTP response bodies, JSON, or log output must keep quotes.

Work iteratively — one file at a time

1. Read the full file
1. Identify violations
1. Understand each variable's content and all its usages
1. Make targeted changes
1. Run bash -n and the test before moving to the next file

Never use automated tools (sed/perl/python) to rename variables across files

Variable names are not unique tokens — $body in one script is a response body requiring quotes; in another it is a simple path. Mass rename without reading context will introduce bugs.
The rule is: remove ${braces} only, not quotes

"${foo}" → "$foo" is usually safe (keeps quotes, removes unnecessary braces).
If any code uses "${foo}_baa" changing to  "$foo_bar" is not safe.

"${foo}" → $foo is only safe after reading the specific usage.
