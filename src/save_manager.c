#include "save_manager.h"
#include "filer_shared.h"
#include "init.h"

static int join(char *out, const char *path, const char *name)
{
	int n = snprintf(out, MAX_PATH, "%s%s", path, name);
	return n < 0 || n >= MAX_PATH ? -ENAMETOOLONG : 0;
}

int saveReadFile(const char *path, uint8_t **data, size_t *size)
{
	int fd, ret = -EIO;
	s64 length;
	size_t off = 0;
	*data = NULL;
	*size = 0;
	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return fd;
	length = genLseek(fd, 0, SEEK_END);
	if (length <= 0 || length > SAVE_MAX_SIZE || genLseek(fd, 0, SEEK_SET) < 0)
		goto done;
	*data = malloc(length);
	if (!*data) {
		ret = -ENOMEM;
		goto done;
	}
	while (off < (size_t)length) {
		int n = genRead(fd, *data + off, length - off);
		if (n <= 0)
			goto done;
		off += n;
	}
	*size = length;
	ret = 0;
done:
	genClose(fd);
	if (ret < 0) {
		free(*data);
		*data = NULL;
	}
	return ret;
}

int saveWriteNew(const char *path, const uint8_t *data, size_t size)
{
	char fixed[MAX_PATH];
	iox_stat_t st;
	int fd, ret = 0;
	size_t off = 0;
	if (genFixPath(path, fixed) < 0)
		return -EIO;
	if (genGetStat(fixed, &st) >= 0)
		return -EEXIST;
	fd = genOpen(fixed, FIO_O_WRONLY | FIO_O_CREAT | FIO_O_EXCL);
	if (fd < 0)
		return fd;
	while (off < size) {
		int n = genWrite(fd, (void *)(data + off), size - off);
		if (n <= 0) {
			ret = -EIO;
			break;
		}
		off += n;
	}
	if (genClose(fd) < 0)
		ret = -EIO;
	if (ret < 0)
		genRemove(fixed);
	return ret;
}

int saveCardType(const char *path)
{
	int type = 0, ret, i;
	if (!strncmp(path, "vmc", 3)) {
		if ((path[3] != '0' && path[3] != '1') || path[4] != ':')
			return -1;
		return saveVmcSlot(path) >= 0 ? 1 : vmcMounted[path[3] - '0'] ? 2 :
		                                                                -1;
	}
	if (strncmp(path, "mc", 2) || (path[2] != '0' && path[2] != '1') || path[3] != ':')
		return 0;
	ensureMemoryCardPortAccessible(path[2] - '0');
	for (i = 0; i < 3; i++) {
		if (mcGetInfo(path[2] - '0', 0, &type, NULL, NULL) < 0)
			return -1;
		mcSync(0, NULL, &ret);
		if (ret == sceMcResSucceed)
			return type;
		if (ret != sceMcResChangedCard)
			return -1;
	}
	return -1;
}

int saveCardRoot(const char *path)
{
	const char *p = strchr(path, ':');
	if (!p)
		return 0;
	p++;
	if (*p == '/')
		p++;
	return !*p && saveCardType(path) > 0;
}

int saveInspect(const char *path, const FILEINFO *file)
{
	char full[MAX_PATH];
	uint8_t hdr[132];
	int fd, n, type;
	if (file->stats.AttrFile & sceMcFileAttrSubdir)
		return 0;
	type = saveCardType(path);
	if (type == 1)
		return 1;
	if (join(full, path, file->name) < 0 || (fd = openPathForRead(full, NULL, 0)) < 0)
		return 0;
	n = genRead(fd, hdr, sizeof(hdr));
	genClose(fd);
	if (n >= 64 && !memcmp(hdr, "\0VSP", 4))
		return hdr[60] == 1 ? 1 : hdr[60] == 2 ? 2 :
		                                         0;
	if (n >= 21 && (!memcmp(hdr, "Ps2PowerSave", 12) || !memcmp(hdr, "CFU\0", 4) || !memcmp(hdr + 4, "SharkPortSave\0\0\0", 16)))
		return 2;
	if (genCmpFileExt(file->name, "psu"))
		return 2;
	if (n >= 130 && ((hdr[0] == 0x51 && hdr[128] == 'S' && hdr[129] == 'C') || (hdr[54] == 'S' && hdr[55] == 'C') ||
	                 ((hdr[0] & ~0x20) == 'S' && (hdr[1] & ~0x20) == 'C')))
		return 1;
	if (n >= 128 && ((!memcmp(hdr, "MC", 2) && file->stats.FileSizeByte == PS1_CARD_SIZE) ||
	                 (!memcmp(hdr, "123-456-STD", 11) && file->stats.FileSizeByte == 0x20f40) ||
	                 (!memcmp(hdr, "VgsM", 4) && file->stats.FileSizeByte == 0x20040) ||
	                 (!memcmp(hdr, "\0PMV", 4) && file->stats.FileSizeByte == 0x20080) ||
	                 (file->stats.FileSizeByte == 0x200a0 && genCmpFileExt(file->name, "mcx"))))
		return 3;
	return 0;
}

static void copyStats(save_entry_t *e, const FILEINFO *f)
{
	strncpy(e->name, f->name, 32);
	e->name[32] = 0;
	e->size = f->stats.FileSizeByte;
	e->mode = f->stats.AttrFile;
	memcpy(e->created, &f->stats._Create, 8);
	memcpy(e->modified, &f->stats._Modify, 8);
}

int saveCollect(const char *path, const FILEINFO *file, save_archive_t *s)
{
	char full[MAX_PATH], dir[MAX_PATH];
	FILEINFO *list = NULL;
	size_t total = 0, off = 0;
	int type = saveCardType(path), i, result = -EINVAL;
	memset(s, 0, sizeof(*s));
	if (join(full, path, file->name) < 0)
		return -ENAMETOOLONG;
	if (type == 1 && !(file->stats.AttrFile & sceMcFileAttrSubdir)) {
		size_t size;
		s->files = calloc(1, sizeof(*s->files));
		if (!s->files)
			return -ENOMEM;
		if (saveReadFile(full, &s->storage, &size) < 0)
			return -EIO;
		s->system = 1;
		s->count = 1;
		copyStats(&s->root, file);
		s->files[0] = s->root;
		s->files[0].data = s->storage;
		s->files[0].size = size;
		s->files[0].mode = 0;
		return saveArchiveValidate(s);
	}
	if (type != 2 || !(file->stats.AttrFile & sceMcFileAttrSubdir)) {
		uint8_t *data;
		size_t size;
		if (saveReadFile(full, &data, &size) < 0)
			return -EIO;
		result = saveDecode(data, size, file->name, s);
		free(data);
		return result;
	}
	s->system = 2;
	copyStats(&s->root, file);
	if (snprintf(dir, sizeof(dir), "%s/", full) >= (int)sizeof(dir))
		return -ENAMETOOLONG;
	list = malloc(sizeof(*list) * MAX_ENTRY);
	if (!list)
		return -ENOMEM;
	s->count = getDir(dir, list);
	if (s->count < 1 || s->count > SAVE_MAX_FILES || s->count >= MAX_ENTRY - 2)
		goto done;
	s->files = calloc(s->count, sizeof(*s->files));
	if (!s->files) {
		result = -ENOMEM;
		goto done;
	}
	for (i = 0; i < s->count; i++) {
		if ((list[i].stats.AttrFile & sceMcFileAttrSubdir) || list[i].stats.FileSizeByte > SAVE_MAX_SIZE - total)
			goto done;
		copyStats(s->files + i, list + i);
		total += list[i].stats.FileSizeByte;
	}
	s->storage = malloc(total ? total : 1);
	if (!s->storage) {
		result = -ENOMEM;
		goto done;
	}
	for (i = 0; i < s->count; i++) {
		int fd;
		size_t pos = 0;
		if (join(full, dir, list[i].name) < 0 || (fd = openPathForRead(full, NULL, 0)) < 0)
			goto done;
		s->files[i].data = s->storage + off;
		while (pos < s->files[i].size) {
			int n = genRead(fd, s->storage + off + pos, s->files[i].size - pos);
			if (n <= 0) {
				genClose(fd);
				goto done;
			}
			pos += n;
		}
		genClose(fd);
		off += pos;
	}
	result = saveArchiveValidate(s);
done:
	free(list);
	return result;
}

static int applyStats(const char *path, const save_entry_t *e)
{
	sceMcTblGetDir st;
	iox_stat_t xs;
	memset(&st, 0, sizeof(st));
	st.AttrFile = e->mode ? e->mode : 0x8497;
	st.FileSizeByte = e->size;
	memcpy(&st._Create, e->created, 8);
	memcpy(&st._Modify, e->modified, 8);
	memcpy(st.EntryName, e->name, strlen(e->name));
	if (!strncmp(path, "mc", 2)) {
		int ret;
		ensureMemoryCardPortAccessible(path[2] - '0');
		ret = mcSetFileInfo(path[2] - '0', 0, path + 4, &st, 0xFEED);
		if (ret < 0)
			return ret;
		mcSync(0, NULL, &ret);
		return ret;
	}
	memset(&xs, 0, sizeof(xs));
	xs.mode = (st.AttrFile & sceMcFileAttrSubdir) ? FIO_S_IFDIR : FIO_S_IFREG;
	xs.size = e->size;
	xs.private_0 = st.AttrFile;
	memcpy(xs.ctime, e->created, 8);
	memcpy(xs.mtime, e->modified, 8);
	return fileXioChStat(path, &xs, 0xFEED);
}

int saveImport(const char *source, const char *destination)
{
	uint8_t *data = NULL;
	size_t size;
	save_archive_t s = {0};
	char root[MAX_PATH], full[MAX_PATH];
	iox_stat_t st;
	int i, created = 0, type, result = -EINVAL;
	if (!saveCardRoot(destination))
		return -EINVAL;
	if (saveReadFile(source, &data, &size) < 0)
		return -EIO;
	if (saveDecode(data, size, source, &s) < 0)
		goto done;
	if (s.system == 2 && !s.root.mode)
		s.root.mode = 0x8427;
	type = saveCardType(destination);
	if (type != s.system) {
		result = -EXDEV;
		goto done;
	}
	if (type == 1 && saveVmcSlot(destination) >= 0) {
		result = saveVmcPut(destination, &s);
		goto done;
	}
	if (join(root, destination, s.root.name) < 0)
		goto done;
	if (genGetStat(root, &st) >= 0) {
		result = -EEXIST;
		goto done;
	}
	if (type == 1) {
		int blocks = 0, ret;
		mcGetInfo(destination[2] - '0', 0, NULL, &blocks, NULL);
		mcSync(0, NULL, &ret);
		if (ret < 0 || blocks < (int)(s.files[0].size / PS1_BLOCK_SIZE)) {
			result = -ENOSPC;
			goto done;
		}
		result = saveWriteNew(root, s.files[0].data, s.files[0].size);
		goto done;
	}
	/* All entries are validated before creating anything; existing saves are kept. */
	result = genMkdir(root, fileMode);
	if (result < 0)
		goto done;
	for (i = 0; i < s.count; i++) {
		if (snprintf(full, sizeof(full), "%s/%s", root, s.files[i].name) >= (int)sizeof(full)) {
			result = -ENAMETOOLONG;
			break;
		}
		if (saveCardType(destination) != 2) {
			result = -EXDEV;
			break;
		}
		result = saveWriteNew(full, s.files[i].data, s.files[i].size);
		if (result < 0)
			break;
		created++;
		result = applyStats(full, s.files + i);
		if (result < 0)
			break;
	}
	if (result >= 0)
		result = applyStats(root, &s.root);
	if (result < 0 && saveCardType(destination) == 2) {
		for (i = 0; i < created; i++) {
			if (snprintf(full, sizeof(full), "%s/%s", root, s.files[i].name) < (int)sizeof(full))
				genRemove(full);
		}
		genRmdir(root);
	}
done:
	free(data);
	saveArchiveFree(&s);
	return result;
}

int saveBackup(const char *source, const FILEINFO *file, const char *destination, int format)
{
	save_archive_t s = {0};
	uint8_t *data = NULL;
	size_t size;
	char name[80], full[MAX_PATH];
	int result;
	result = saveCollect(source, file, &s);
	if (result < 0)
		goto done;
	if (format == SAVE_PSV)
		savePsvName(name, sizeof(name), s.root.name);
	else
		snprintf(name, sizeof(name), "%s%s", s.root.name, format == SAVE_MCS ? ".mcs" : format == SAVE_PSX ? ".psx" :
		                                                                                                     "");
	if (join(full, destination, name) < 0) {
		result = -ENAMETOOLONG;
		goto done;
	}
	result = saveEncode(&s, format, &data, &size);
	if (result >= 0)
		result = saveWriteNew(full, data, size);
done:
	free(data);
	saveArchiveFree(&s);
	return result;
}

int saveBackupCard(const char *source, const FILEINFO *files, int count, const char *destination, int format)
{
	uint8_t *raw = malloc(PS1_CARD_SIZE), *data = NULL, *original = NULL;
	size_t size, original_size = 0;
	char full[MAX_PATH], name[80] = "PS1_BACKUP";
	const char *ext = format == CARD_VM1 ? ".VM1" : format == CARD_GME ? ".gme" :
	                                            format == CARD_VGS     ? ".vgs" :
	                                            format == CARD_VMP     ? ".VMP" :
	                                            format == CARD_MCX     ? ".MCX" :
	                                                                     ".VMC";
	int i, result = -ENOMEM;
	if (!raw)
		return result;
	ps1CardFormat(raw);
	if (count == 1 && saveInspect(source, files) == 3) {
		if (join(full, source, files[0].name) < 0 || saveReadFile(full, &original, &original_size) < 0 || ps1CardDecode(original, original_size, raw, NULL) < 0) {
			result = -EINVAL;
			goto done;
		}
	} else
		for (i = 0; i < count; i++) {
			save_archive_t s = {0};
			result = saveCollect(source, files + i, &s);
			if (result >= 0)
				result = ps1CardPut(raw, &s);
			saveArchiveFree(&s);
			if (result < 0)
				goto done;
		}
	if (keyboard(name, 32) < 0) {
		result = 1;
		goto done;
	}
	if (!saveNameValid(name, 32)) {
		result = -EINVAL;
		goto done;
	}
	strcat(name, ext);
	if (join(full, destination, name) < 0) {
		result = -ENAMETOOLONG;
		goto done;
	}
	result = ps1CardEncode(raw, format, original, original_size, &data, &size);
	if (result >= 0)
		result = saveWriteNew(full, data, size);
done:
	free(raw);
	free(data);
	free(original);
	return result;
}

int saveCheckCardCopy(const char *source, const char *destination, const FILEINFO *file, int mode)
{
	int src = saveCardType(source), dst = saveCardType(destination), fd, n;
	char full[MAX_PATH];
	uint8_t header[2];
	if (mode == PM_PSU_BACKUP)
		return src == 1 ? -EXDEV : 0;
	if (mode == PM_PSU_RESTORE)
		return dst == 1 ? -EXDEV : 0;
	if ((src == 1 && dst == 2) || (src == 2 && dst == 1))
		return -EXDEV;
	if (dst == 2 && src == 0 && saveInspect(source, file) == 1)
		return -EXDEV;
	if (dst != 1)
		return 0;
	if (!saveCardRoot(destination) || (file->stats.AttrFile & sceMcFileAttrSubdir) ||
	    file->stats.FileSizeByte < PS1_BLOCK_SIZE || file->stats.FileSizeByte > 15 * PS1_BLOCK_SIZE || file->stats.FileSizeByte % PS1_BLOCK_SIZE)
		return -EINVAL;
	if (join(full, source, file->name) < 0 || (fd = openPathForRead(full, NULL, 0)) < 0)
		return -EIO;
	n = genRead(fd, header, 2);
	genClose(fd);
	return n == 2 && (header[0] & ~0x20) == 'S' && (header[1] & ~0x20) == 'C' ? 0 : -EINVAL;
}
