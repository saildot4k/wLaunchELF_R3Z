#include "gui_texteditor.h"

static int isDirectoryEntry(const FILEINFO *file)
{
	return (file->stats.AttrFile & sceMcFileAttrSubdir) != 0;
}

static int isParentEntry(const FILEINFO *file)
{
	return !strcmp(file->name, "..");
}

int canOpenInTextEditor(const char *path, const FILEINFO *file)
{
	if (isDirectoryEntry(file) || isParentEntry(file))
		return FALSE;

	/* Do not offer text editing for synthetic browser roots or executable/system assets. */
	if (!strcmp(path, setting->Misc) ||
	    genCmpFileExt(file->name, "ELF") ||
	    genCmpFileExt(file->name, "ICN") ||
	    genCmpFileExt(file->name, "SYS"))
		return FALSE;

	return TRUE;
}
