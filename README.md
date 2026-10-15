sqlite
======

sqlite is an addon for Circle to provide support for SQLite3 databases.
It is based on the SQLite release 3.53.4 source amalgamation.

This differs from the sqlite repository, as it does not rely on circle-stdlib, or circle-newlib.
Instead, some configuration options are used to eliminate the Windows and Linux code from the sqlite3 library.
File-level access is done using a custom SQLite VFS, and the fatfs library from the addons folder.

This was mainly done so that the existing folder structure of circle home, with lib, addon and app subfolders could be used when developing.

There are some minor changes needed to the original sqlite3.c amalgamation file, which are easily identifed by searching for SQLITE_CIRCLE in the modified code. They are needed to stop the compiler from trying to include a non-existent header, and to stop a couple of macro functions from being redefined.
