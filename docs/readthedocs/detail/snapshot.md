## Efficient Snapshot

In addition to the allocation APIs, Metall provides a snapshot feature that stores only the difference from the previous snapshot point instead of duplicating the entire persistent heap by leveraging [reflink](http://man7.org/linux/man-pages/man2/ioctl_ficlonerange.2.html).

With reflink, a copied file shares the same data blocks with the existing file;
data blocks are copied only when they are modified (*copy-on-write*).

As reflink is relatively new, not all filesystems support it.
Those that do include XFS, ZFS, Btrfs, and Apple File System (APFS) — we expect that more filesystems will implement this feature in the future.

In case reflink is not supported by the underlying filesystem,
Metall automatically falls back to a regular copy.