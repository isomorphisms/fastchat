# Icky C prerequisite receipt

Status: **historical failure, superseded on the pinned compiler fix branch**.
The FastChat application has not been implemented. The unchanged probe now passes
with the owned rebuilt ICK; see [fixed frontend receipt](fixed-frontend.md).
The baseline diagnostics below are retained as historical evidence.

## Executed probe

The requested C pass requires assignment arrows. The independent
`qualification/icky-assignment/assignment-arrow.c` probe retains ordinary C
pointer declaration, dereference/member access, named functions and descriptive
names. The owned ICK frontend rejects its `←` assignments.

`ordinary-c-control.c` differs only by conventional assignment spellings. It
passes syntax checking. It is a compiler control, not an application fallback.

| Probe | Exit | Evidence |
| --- | ---: | --- |
| Required assignment arrows | 1 | stray character U+2190; expected semicolon |
| Ordinary C control | 0 | no diagnostics |

Commands and full diagnostics are retained beside this receipt.

The available compiler was built for `aarch64-linux-gnu`, from ICK
`7afb1820cd59c0c51d19a7e37902c14f4466f442`. This run establishes a shared
C-frontend capability failure only. It produces no ARM32 object, Android
artifact, C67 acceptance or MIRO A1 runtime evidence. A1 remains the only
application target.

The inspected live ICK main is
`e3c2a40b4edafc4d9caca55d1f7c094e6aab9589`.
An executed Git diff of `ick/` and `.gitmodules` between that main and the
compiler checkout exits 0 with no differences. Its pinned GCC reference is
`6294f1d9e7536e5ffcde09d1528c918d63abfef5`.
This source-layer identity does not relabel the available compiler as an A1
compiler or qualify application headers.

Driver SHA-256:
`952d3ae0d5916e4d3030064d81cb193e1cbe1f8e706c556195f3d93d0bd42ba1`.

Owned cc1 SHA-256:
`2518c5d708f09163439576e0c69700a45ed6b3c4cc1daa609ae380536b729918`.

## Exact source findings

- The ICK materializer owns the complete overlay and prune manifest. Its current
  tree changes no C lexer, C parser or libcpp lexer/token table. The pinned GCC
  token table uses ordinary C assignment. No consumer-side notation translator
  is part of this inspected path.
- [IckY at f8aa487](https://github.com/dilapidated-shed/icky/blob/f8aa48738cecf0c833e86b7654ea4e9d15123517/README.md)
  recognizes `←`, `=`, `⌖`, `↥`, `·` but explicitly leaves their meaning and
  application/precedence rules downstream. It is not an executable C lowerer.
- [Icky Lua's actual fixtures](https://github.com/isomorphisms/lua/blob/87306483cec50f8c750a22dda1d0742246fad756/testes/symbolic-assignment.lua)
  already cover `←`, `→`, `≟`, `≠`, `≤`, `≥`, `×`, `÷`, `−`, `λ`
  and `ƒ`. The lexer and parser were inspected at that exact revision. Lua
  source presence is not a new Lua runtime acceptance claim.
- [The existing A1 application-C qualifier](https://github.com/dilapidated-shed/ick/blob/2662e57f7452f6d9492c7a4c9cb51433f105580a/qualification/android-boundary/APPLICATION-C.md)
  records executed ICK failures on Bionic nullability qualifiers and Android
  availability attributes with NDK r27c. That is prior evidence against its
  recorded compiler revision, not a new r29 or FastChat application result.

The historical assignment-arrow blocker now passes on the pinned fix revision
linked above. The complete ARM32/Bionic compile and platform link still need
qualification. Do not erase attributes, pretend to be Clang, silently compile
consumer code with another compiler, or invent a local regex translator.

## Evidence boundaries

FastChat base: `b49f0f7083d2d034ba76b910a43592d6d07bd18b`.
Semantic reference: [isomorphisms/fastchat PR #4, “Implement executable Idriç conversation core”](https://github.com/isomorphisms/fastchat/pull/4),
head `c66021957fb5dc195a995b2c0cd212863ff68915`; source and fixtures inspected;
discussion contained no comments at inspection. This experiment does not depend
on merging that reference.

Cat Food authority: `62ac940588f5ee4c5be468d48e38d74514da3757`.
Android NativeActivity/package boundary:
`isomorphisms/android-NDK@7c61ee43e75f7c2dab9288edb0e10055898b36e6`.

| Requested stage | Result |
| --- | --- |
| Full working functorial Icky C pass | BLOCKED/NOT_RUN |
| Storage and semantic runtime tests | BLOCKED/NOT_RUN |
| Icky Lua second pass | BLOCKED/NOT_RUN; working C prerequisite |
| RAM-control comparison and measurements | BLOCKED/NOT_RUN |
| A1 native library and APK | BLOCKED/NOT_RUN |
| Physical MIRO A1 execution | BLOCKED/NOT_RUN; no device channel exercised |

The draft must remain unmerged and incomplete until these stages have real
receipts. No implementation, packaged artifact or measurement is claimed.

