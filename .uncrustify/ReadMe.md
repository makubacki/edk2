# Uncrustify POC

Uncrustify is a source code beautifier that works against C/C++ and other languages.

* Github repository: https://github.com/uncrustify/uncrustify

This proof-of-concept explores the use of Uncrustify for edk2 source code. The goal being to establish a configuration
for the tool that closely adheres to the
[EDK II C Coding Standards Specification](https://edk2-docs.gitbook.io/edk-ii-c-coding-standards-specification/). By
doing so, the tool can be used by developers to help reduce some of the tedious work involved in understanding and
applying the rules in the specification to their code. For general background on rationale behind the current set
of style guidelines and arguments for uniformity of style across the codebase, refer to the coding standards
specification.

The results from this tool should be considered a "best effort". Part of the initial POC work involves identifying
shortcomings and areas of ambiguity and deciding upon the best course of action. For example, Uncrustify is highly
customizable (as of version 0.72.0, 742 configuration options exist), eventually a scenario will have an explicit rule
defined in the Uncrustify configuration that was not explicitly defined in the coding standards specification. A
consensus must be reached about how to treat that particular scenario. In turn, the specification might be updated to
reflect this change. In other cases, a rule in the specification might not be enforceable in Uncrustify as-is. Several
options exist:

1. Ignore the rule in Uncrustify
2. Update the specification
3. Submit an upstream change to Uncrustify
4. Maintain an edk2 fork of Uncrustify with the change

Changes required in the Uncrustify tool have already led to an edk2 fork of Uncrustify. Those changes are available
in the following branch: https://github.com/makubacki/uncrustify/tree/initial_edk2_support

Ideally, after further review and testing, the changes could be upstreamed and the fork eliminated. The Uncrustify
binary used to produce results in this branch was built from that branch.

## Background

This POC began by researching open source beautifiers capable of checking and formatting C/C++ source code
based on a set of rules in a configuration file. Three final options were considered:

1. [clang-format](https://clang.llvm.org/docs/ClangFormat.html) -
   [Not enough customization](https://clang.llvm.org/docs/ClangFormatStyleOptions.html). The tool has active
   development but the configuration file options available at the time were severely lacking what is needed
   to come close to the EDK II C Coding Standards Specification.
2. [Vera++](https://bitbucket.org/verateam/vera/wiki/Home) -
   [Not enough customization](https://bitbucket.org/verateam/vera/wiki/Rules). Development of the tool was not
   very active and the limited set of customization options made this a non-starter.
3. [Uncrustify](https://github.com/uncrustify/uncrustify) - The tool has active development and detailed customization
   options.

## Current State

A configuration file is present in this branch (`.uncrustify/edk2.cfg`) that has been tweaked to get close to edk2.
Uncrustify (fork build) was run against the edk2 source code using this configuration file to produce the commits at
the tip of this branch. Due to a new configuration file option introduced `indent_func_call_edk2_style`, this branch
will not work as-is with the mainline version of Uncrustify.

While there's many minor issues, some remaining major deficiencies are noted below.

### Missing: Naming Convention Check

See [3.1 Naming](https://edk2-docs.gitbook.io/edk-ii-c-coding-standards-specification/3_quick_reference#3-1-naming).

Many of the naming conventions can be likely be checked wit a different tool. The most common violations include:

* Unclear variable names
* Not defining non-standard abbreviations/acronyms in the header file
* Not using Pascal case
* Capitalizing acronyms (e.g. MyPCIAddress)
* Not separating distinct words with an underscore in macro names (e.g. EACH_WORD_ISNOT_SEPARATE)
* Usage of Hungarian notation
* Global variables not prefixed with (g) and module variables not prefixed with (m)

The following issues in the main version of Uncrustify are what led to the fork. **They are resolved in the fork** but
information concerning the issues are kept here for reference.

## Current State: Issues Addressed in Uncrustify Fork

### Missing: Function Call Format

See [5.2.2.4](https://edk2-docs.gitbook.io/edk-ii-c-coding-standards-specification/5_source_files/52_spacing#5-2-2-4-subsequent-lines-of-multi-line-function-calls-should-line-up-two-spaces-from-the-beginning-of-the-function-name).

Uncrustify feature request:
[Feature Request: Multi-line function call argument indentation from function name · Issue #3077](https://github.com/uncrustify/uncrustify/issues/3077)

* The problem is that Uncrustify cannot indent function call arguments relative to the start of a function name.
* Uncrustify can indent function call arguments relative to the block indentation level or the open parenthesis level
  and/or align multi-line arguments to the first argument.
* Uncrustify can keep the first argument on the same line as the opening parenthesis or move it to the next line for
  a multi-line argument list.

**Note:** If the fork implementation is known to not introduce regressions, a cleaned up version can be used to satisfy
the above feature request.

### Missing: Special Handling for DEBUG ()

Due to the way arguments to the `DEBUG ()` macro are substituted internally, the actual argument list is surrounded by
a pair of two opening and closing parenthesis.

This is uniquely different than the convention for other function / function macros which would normally put the
parenthesis on the next line to form a new indentation level.

* Uncrustify cannot make an exception for the `DEBUG ()` macro as-is.

## How to Run Uncrustify

These instructions are written for Windows 10. These activities could be further automated into a high-level script
but that has not been done yet.

1. Generate a list of all .c and .h files recursively

   * It is recommended to run this in cleanly cloned edk2 repo without submodules to prevent submodule files
     (such as Brotli files in MdeModulePkg) from getting included in the file list. This will significantly increase
     the amount of time Uncrustify takes to run.

   * Sample Powershell command to recursively write all .c and .h files in a given package to a text file:

     ```powershell
     Get-ChildItem -Path .\MdePkg\* -Include *.c, *.h -Recurse -Force | %{$_.fullname} | Out-File
     -FilePath .\MdePkgFiles.txt -Encoding utf8
     ```

2. Run Uncrustify using the generated text file as input

   ```shell
   uncrustify.exe -c .\.uncrustify\edk2.cfg -F MdePkgFiles.txt --replace --no-backup --if-changed
   ```

   > *Note:* When testing a configuration change, it is sometimes useful to run Uncrustify against a particular file
     and check the debug output to understand what rule was applied and why it was applied. The command shows an
     example of how to run the configuration file `edk2.cfg` against the source file `VariableSmm.c` where the file
     is forced to be treated as C, the debug output is written to `uncrustify_debug.txt` and the log severity level
     is set to "all".

   ```shell
   uncrustify.exe -c .\cfg\edk2.cfg -f .\MdeModulePkg\Universal\Variable\RuntimeDxe\VariableSmm.c -o output.c -l C -p uncrustify_debug.txt -L A 2>verbose_debug.txt
   ```

Uncrustify will update the source files in-place (with the commands given). This allows you to diff the results with
git. From here, you can iteratively tweak the configuration file and check the results until your satisfied with the
outcome.

## Next Steps

I see enough potential that I would like to share the current state with others in the hope they
will be willing to further advance the work and share any improvements so we can seriously consider using Uncrustify
in the edk2 development process.

The tool itself could be used in several ways to aid developers. For example, Uncrustify could be run as a PR hook to
post an "Uncrustified" branch of the PR submitted so the author can quickly accept changes into their PR (assuming they
did not first run the tool offline). Having such an easy mechanism to format large amounts of code could more strictly
enforce consistency in edk2 while providing downstream consumers a low overhead path to style alignment with edk2.

How to integrate the tool into the edk2 development flow is something we will need to address if the project considers
adoption.
