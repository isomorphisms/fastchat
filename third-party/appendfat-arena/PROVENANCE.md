# appendFAT userspace arena provenance

Source repository: isomorphisms/sd-card-append-fat

Pinned source commit: `a4ab471bd26a1aa6a3d35260d443128072bfc947`

Pinned files:

- `lib/appendfat_arena.h` blob `8d239e5ecee752eebf859256d42b5a3925b9eeec`
- `lib/appendfat_arena.c` blob `8330901117e84d49f89ec0fc71747361c0a2c77b`

FastChat compiles this vendored snapshot through ICK. The canonical implementation
remains in sd-card-append-fat; this copy is pinned so FastChat builds are
reproducible and do not fetch mutable source during compilation.
