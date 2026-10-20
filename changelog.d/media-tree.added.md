**`adl::media_tree`**: writes the members of a media set to a directory tree
(`member_001`, `member_002`, ...), mapping the standard's backslash pathnames to
native paths, and reads one back into `MediaMember`s. It refuses names that
cannot be files on every platform, writes nothing if anything is invalid, never
writes over a member directory that has files in it, and does not follow links.
It uses `std::filesystem` only, so `adl::arinc665` stays free of file I/O and
nothing platform-specific is needed.
