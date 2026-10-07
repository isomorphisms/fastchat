# appendFAT userspace arena provenance

Source repository: isomorphisms/sd-card-append-fat

Pinned source commit: `70481a5aa8cde3f15023aaa915f071cd90eb0596`

Pinned files:

- `lib/appendfat_arena.h` blob `8d239e5ecee752eebf859256d42b5a3925b9eeec`
- `lib/appendfat_arena.c` blob `fd1d0ccb25b23820828e86362af86aa43246d88d`

FastChat compiles this vendored snapshot through ICK. The canonical implementation
remains in sd-card-append-fat; this copy is pinned so FastChat builds are
reproducible and do not fetch mutable source during compilation.
