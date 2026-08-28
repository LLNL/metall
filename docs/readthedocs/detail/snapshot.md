# Efficient Snapshot

Metall can create point-in-time snapshots of a datastore. This is useful when
an application wants an explicit recovery point before a major update, a new
analytics phase, or an experiment that may rewrite a large part of the heap.

When possible, Metall makes snapshots efficient by using
[reflink](http://man7.org/linux/man-pages/man2/ioctl_ficlonerange.2.html)
instead of copying the entire datastore eagerly.

With reflink, the new snapshot initially shares data blocks with the source
datastore. Physical blocks are copied later only if one side is modified. This
copy-on-write behavior keeps snapshots cheap in both time and storage when the
filesystem supports it.

## Important Behavior

- A snapshot gives you an explicit recovery point; it does not make every write
  transactional.
- Snapshot efficiency depends on filesystem support for reflink.
- If reflink is unavailable, Metall automatically falls back to a regular copy,
  which is still correct but may cost more time and storage.
- Filesystems that commonly support reflink include XFS, ZFS, Btrfs, and APFS.

Snapshots fit naturally with Metall's coarse-grained persistence model: the
application decides when the current heap state is important enough to preserve
as a stable version.
