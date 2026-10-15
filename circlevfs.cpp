//
// VFS module for circle (does not require stdlib support)
// Depends upon addon/fatfs to perform file access
// See http;//sqlite.org/vfs.html for details on the SQLite structues and
// for more information on the VFS concepts.
// 
#include "sqlite3.h"
#include <string.h>
#include <fatfs/ff.h>


#if !defined(NULL)
#define NULL ((void*)0)
#endif

#if !defined(MAXPATHNAME)
#define MAXPATHNAME (128)
#endif
extern "C"
{
// What our file objects look like
struct circleVFS_file
{
	sqlite3_file base;
	char pathname[MAXPATHNAME];
	FIL theFile;
	int flags;
};

// method declarations for circleVFS_file
static int circleVFS_file_Close (sqlite3_file*);
static int circleVFS_file_Read (sqlite3_file*, void*, int iAmt, sqlite3_int64 iOfst);
static int circleVFS_file_Write (sqlite3_file*, const void*, int iAmt, sqlite3_int64 iOfst);
static int circleVFS_file_Truncate (sqlite3_file*, sqlite3_int64 size);
static int circleVFS_file_Sync (sqlite3_file*, int flags);
static int circleVFS_file_FileSize (sqlite3_file*, sqlite3_int64 *pSize);
static int circleVFS_file_Lock (sqlite3_file*, int);
static int circleVFS_file_Unlock (sqlite3_file*, int);
static int circleVFS_file_CheckReservedLock (sqlite3_file*, int *pResOut);
static int circleVFS_file_FileControl (sqlite3_file*, int op, void *pArg);
static int circleVFS_file_SectorSize (sqlite3_file*);
static int circleVFS_file_DeviceCharacteristics (sqlite3_file*);

// 
struct sqlite3_io_methods circleVFS_io_methods = {
	1,
	circleVFS_file_Close,
	circleVFS_file_Read,
	circleVFS_file_Write,
	circleVFS_file_Truncate,
	circleVFS_file_Sync,
	circleVFS_file_FileSize,
	circleVFS_file_Lock,
	circleVFS_file_Unlock,
	circleVFS_file_CheckReservedLock,
	circleVFS_file_FileControl,
	circleVFS_file_SectorSize,
	circleVFS_file_DeviceCharacteristics,
};

// method declaration of functions for vfs object
static int circleVFS_vfs_open (sqlite3_vfs*, sqlite3_filename, sqlite3_file*, int, int*);
static int circleVFS_vfs_delete (sqlite3_vfs*, const char*, int);
static int circleVFS_vfs_access (sqlite3_vfs*, const char*, int, int *);
static int circleVFS_vfs_fullPathname (sqlite3_vfs*, const char*, int, char*);
// Dynamic loading is NOT supported by this VFS, so no DlOpen,DlError,DlSym or DlClose
/* NOT USED
static int circleVFS_vfs_Randomness (sqlite3_vfs*, int, char*);
static int circleVFS_vfs_Sleep (sqlite3_vfs*, int);
static int circleVFS_vfs_CurrentTime (sqlite3_vfs*, double*);
static int circleVFS_vfs_GetLastError (sqlite3_vfs*, int, char*);
*/

// Declare VFS object (only v1 methods are implemented)
struct sqlite3_vfs circle_VFS = {
	1,
	sizeof(struct circleVFS_file),
	MAXPATHNAME,                    /* mxPathname */
	NULL,                           /* pNext */
	"circleVFS",                        /* zName */
	NULL,                           /* pAppData */
	circleVFS_vfs_open,
	circleVFS_vfs_delete,
	circleVFS_vfs_access,
	circleVFS_vfs_fullPathname,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // circleVFS_vfs_Randomness,
	NULL, // circleVFS_vfs_Sleep,
	NULL, // circleVFS_vfs_CurrentTime,
	NULL, // circleVFS_vfs_GetLastError,
	// Methods in v2
	NULL,
	// Methods in v3
	NULL,
	NULL,
	NULL
};

//--------------------------------------------
//VFS_file methods
//--------------------------------------------

static int circleVFS_file_Close (sqlite3_file *pFile)
{
	circleVFS_file* file = (circleVFS_file*)pFile;
	f_close(&file->theFile);
	if ((file->flags & SQLITE_OPEN_DELETEONCLOSE)==SQLITE_OPEN_DELETEONCLOSE)
	{
		f_unlink (file->pathname);
	}
	return SQLITE_OK;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_Read(sqlite3_file *pFile, void *pBuf, int nBuf, sqlite3_int64 offset)
{
	circleVFS_file *file = (circleVFS_file *)pFile;

	if (f_lseek (&file->theFile, offset) != FR_OK)
	{
		return SQLITE_ERROR;
	}

	UINT nr = 0;
	// Be sure to zero fill so that short reads don't cause issues...
	memset(pBuf, 0, nBuf);
	if (f_read (&file->theFile, pBuf, (UINT)nBuf, &nr) != FR_OK)
	{
		return SQLITE_ERROR;
	}
	return nr == (UINT)nBuf
		? SQLITE_OK
		: SQLITE_IOERR_SHORT_READ;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_Write(sqlite3_file *pFile, const void *pBuf, int nBuf, sqlite3_int64 offset)
{
	circleVFS_file *file = (circleVFS_file *)pFile;

	if (f_lseek (&file->theFile, offset) != FR_OK)
	{
		return SQLITE_ERROR;
	}

	UINT nr = 0;
	if (f_write (&file->theFile, pBuf, (UINT)nBuf, &nr) != FR_OK)
	{
		return SQLITE_ERROR;
	}
	return nr == (UINT)nBuf
		? SQLITE_OK
		: SQLITE_IOERR_WRITE;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_Truncate (sqlite3_file *pFile, sqlite3_int64 size)
{
	circleVFS_file* file = (circleVFS_file*)pFile;

	sqlite3_int64 offs = f_tell(&file->theFile);
	f_lseek(&file->theFile, size);
	f_truncate(&file->theFile);
	f_lseek(&file->theFile, offs);
	return SQLITE_OK;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_Sync(sqlite3_file *pFile, int flags)
{
	circleVFS_file* file = (circleVFS_file*)pFile;
	return f_sync(&file->theFile) == FR_OK ? SQLITE_OK : SQLITE_ERROR;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_FileSize(sqlite3_file *pFile, sqlite3_int64 *pSize)
{

	circleVFS_file *file = (circleVFS_file *)pFile;
	*pSize = f_size(&file->theFile);
	return SQLITE_OK;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_Lock(sqlite3_file *pFile, int eLock)
{
	return SQLITE_OK;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_Unlock(sqlite3_file *pFile, int eLock)
{
	return SQLITE_OK;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_CheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
	*pResOut = 0;
	return SQLITE_OK;
}

// ------------------------------------------------------------------------------
static int circleVFS_file_FileControl(sqlite3_file *pFile, int op, void *pArg)
{
	return SQLITE_NOTFOUND;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_SectorSize(sqlite3_file *pFile)
{
	return 0;
}

// ------------------------------------------------------------------------------

static int circleVFS_file_DeviceCharacteristics(sqlite3_file *pFile)
{
	return 0;
}

// -----------------------------------------------------------------------------
// Methods for vfs object
static int circleVFS_vfs_open (sqlite3_vfs *vfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags)
{
	circleVFS_file *file = (circleVFS_file *)pFile;
	memset(file, 0, sizeof(circleVFS_file));
	pFile->pMethods = &circleVFS_io_methods;
	strncpy(file->pathname, zName, MAXPATHNAME);
	// Convert flags to appropriate mode:
	BYTE mode = FA_OPEN_EXISTING;
	if ((flags & SQLITE_OPEN_CREATE) == SQLITE_OPEN_CREATE)
	{
		FILINFO fno;
		if (f_stat (zName,&fno) != FR_OK)
		{
			// If file does not exist add 
			mode |= FA_CREATE_NEW;
		}
		else
		{
			if ((fno.fattrib & AM_DIR)==AM_DIR)
			{
			return SQLITE_CANTOPEN;
			}
		}
		mode |= FA_READ|FA_WRITE;
	}
	else if ((flags & SQLITE_OPEN_READWRITE) == SQLITE_OPEN_READWRITE)
	{
		mode |= FA_READ|FA_WRITE;
	}
	else if ((flags & SQLITE_OPEN_READONLY) == SQLITE_OPEN_READONLY)
	{
		mode |= FA_READ;
	}
	file->flags = flags;
	if (f_open ( &file->theFile, zName, mode) != FR_OK)
	{
		return SQLITE_CANTOPEN;
	}
	if (pOutFlags)
	{
		*pOutFlags = 0;
	}
	return SQLITE_OK;
}

static int circleVFS_vfs_delete (sqlite3_vfs* vfs, const char*pathname, int)
{
	return (f_unlink(pathname)==FR_OK) ? SQLITE_OK : SQLITE_ERROR;
}

static int circleVFS_vfs_access (sqlite3_vfs*, const char*pathname, int flags, int *pRes)
{
	FILINFO fno;
	if (f_stat (pathname, &fno) != FR_OK)
	{
		*pRes = 0;
	}
	else
	{
		if ((fno.fattrib & AM_DIR)==AM_DIR)
		{
			*pRes = 0;
		}
		else
		{
			if (flags == SQLITE_ACCESS_EXISTS || flags == SQLITE_ACCESS_READ)
			{
				*pRes = 1;
			}
			else
			{
				if (flags == SQLITE_ACCESS_READWRITE && (fno.fattrib & AM_RDO)==AM_RDO)
				{
					*pRes = 0;
				}
			}
		}
	}
	return SQLITE_OK;
}

static int circleVFS_vfs_fullPathname (sqlite3_vfs*, const char*zName, int nChars, char*zOut)
{
	strcpy(zOut, zName);
	return SQLITE_OK;
}
/********* NOT USED 
static int circleVFS_vfs_Randomness (sqlite3_vfs*, int, char*)
{
	return SQLITE_OK;
}

static int circleVFS_vfs_Sleep (sqlite3_vfs*, int)
{
	return SQLITE_OK;
}

static int circleVFS_vfs_CurrentTime (sqlite3_vfs*, double*)
{
	return SQLITE_OK;
}

static int circleVFS_vfs_GetLastError (sqlite3_vfs*, int, char*)
{
	return SQLITE_OK;
}
***************/

// ------------------------------------------------------------------------------
int sqlite3_os_init (void)
{
	int rc = sqlite3_vfs_register(&circle_VFS, 1);
	return rc;
}

int sqlite3_os_end (void)
{
	return SQLITE_OK;
}
};
