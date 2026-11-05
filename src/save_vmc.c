/* PS1 card filesystem for the EE browser. Each edit is staged in memory, then
 * replaces the container using a sibling temporary file and recovery backup.
 * Keeping this above fileXio also permits signed VMP and encrypted MCX cards. */
#include "save_manager.h"
#include "filer_shared.h"

#define VMC_FD_BASE 0x60000000
#define VMC_HANDLES 4

typedef struct
{
	uint8_t *raw, *original;
	size_t original_size;
	int format;
	char path[MAX_PATH];
} ps1_vmc_t;

typedef struct
{
	int active, slot, flags, dirty, failed;
	uint32_t position, size;
	char name[21];
	uint8_t *data;
} ps1_file_t;

static ps1_vmc_t cards[2];
static ps1_file_t handles[VMC_HANDLES];

int saveVmcSlot(const char *path)
{
	int slot;
	if (!path || strncmp(path, "vmc", 3) || (path[3] != '0' && path[3] != '1') || path[4] != ':')
		return -1;
	slot = path[3] - '0';
	return cards[slot].raw ? slot : -1;
}

static const char *entryName(const char *path)
{
	const char *p = strchr(path, ':');
	if (!p)
		return NULL;
	p++;
	if (*p == '/')
		p++;
	return p;
}

void saveVmcUnmount(int slot)
{
	int i;
	if (slot < 0 || slot > 1)
		return;
	for (i = 0; i < VMC_HANDLES; i++)
		if (handles[i].active && handles[i].slot == slot) {
			free(handles[i].data);
			memset(handles + i, 0, sizeof(handles[i]));
		}
	free(cards[slot].raw);
	free(cards[slot].original);
	memset(cards + slot, 0, sizeof(cards[slot]));
}

int saveVmcMount(int slot, const char *path)
{
	ps1_vmc_t next = {0};
	int i, fd;
	s64 size;
	if (slot < 0 || slot > 1)
		return -EINVAL;
	for (i = 0; i < 2; i++)
		if (cards[i].raw && !strcmp(cards[i].path, path))
			return -EBUSY;
	/* PS2 images are handled by vmcman. Small images must pass PS1 validation. */
	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return fd;
	size = genLseek(fd, 0, SEEK_END);
	genClose(fd);
	if (size > 0x20f40)
		return 0;
	if (saveReadFile(path, &next.original, &next.original_size) < 0)
		return -EIO;
	next.raw = malloc(PS1_CARD_SIZE);
	if (!next.raw) {
		free(next.original);
		return -ENOMEM;
	}
	if (ps1CardDecode(next.original, next.original_size, next.raw, &next.format) < 0) {
		free(next.original);
		free(next.raw);
		return -EINVAL;
	}
	if (strlen(path) >= sizeof(next.path)) {
		free(next.original);
		free(next.raw);
		return -ENAMETOOLONG;
	}
	strcpy(next.path, path);
	saveVmcUnmount(slot);
	cards[slot] = next;
	return 1;
}

static int commit(int slot, uint8_t *next)
{
	ps1_vmc_t *c = cards + slot;
	uint8_t *encoded = NULL;
	size_t size;
	char temp[MAX_PATH], backup[MAX_PATH];
	iox_stat_t st;
	int ret;
	if (snprintf(temp, sizeof(temp), "%s.wle-new", c->path) >= (int)sizeof(temp) ||
	    snprintf(backup, sizeof(backup), "%s.wle-old", c->path) >= (int)sizeof(backup))
		return -ENAMETOOLONG;
	if (genGetStat(backup, &st) >= 0)
		return -EEXIST;
	ret = ps1CardEncode(next, c->format, c->original, c->original_size, &encoded, &size);
	if (ret < 0)
		return -EINVAL;
	/* Do not overwrite a container changed outside this mount. */
	{
		uint8_t *current = NULL;
		size_t length;
		ret = saveReadFile(c->path, &current, &length);
		if (ret >= 0 && (length != c->original_size || memcmp(current, c->original, length)))
			ret = -EBUSY;
		free(current);
	}
	if (ret >= 0)
		ret = saveWriteNew(temp, encoded, size);
	if (ret < 0) {
		free(encoded);
		return ret;
	}
	ret = fileXioRename(c->path, backup);
	if (ret >= 0) {
		ret = fileXioRename(temp, c->path);
		if (ret < 0)
			fileXioRename(backup, c->path);
	}
	if (ret < 0) {
		genRemove(temp);
		free(encoded);
		return ret;
	}
	genRemove(backup);
	memcpy(c->raw, next, PS1_CARD_SIZE);
	free(c->original);
	c->original = encoded;
	c->original_size = size;
	return 0;
}

int saveVmcList(const char *path, FILEINFO *files, int max)
{
	int slot = saveVmcSlot(path), i, n = 0;
	const char *name = entryName(path);
	if (slot < 0 || !name || *name)
		return 0;
	for (i = 0; i < 15 && n < max; i++) {
		const uint8_t *h = cards[slot].raw + (i + 1) * 128;
		if (h[0] != 0x51)
			continue;
		memset(files + n, 0, sizeof(*files));
		memcpy(files[n].name, h + 10, 20);
		files[n].name[20] = 0;
		files[n].stats.AttrFile = 0x9417;
		files[n].stats.FileSizeByte = (uint32_t)h[4] | ((uint32_t)h[5] << 8) | ((uint32_t)h[6] << 16) | ((uint32_t)h[7] << 24);
		memcpy(files[n].stats.EntryName, files[n].name, 20);
		n++;
	}
	size_valid = 1;
	time_valid = 0;
	return n;
}

int saveVmcStat(const char *path, iox_stat_t *st)
{
	int slot = saveVmcSlot(path), found;
	const char *name = entryName(path);
	if (slot < 0 || !name)
		return -ENOENT;
	memset(st, 0, sizeof(*st));
	if (!*name) {
		st->mode = FIO_S_IFDIR;
		return 0;
	}
	if (!saveNameValid(name, 20) || (found = ps1CardFind(cards[slot].raw, name)) < 0)
		return -ENOENT;
	st->mode = FIO_S_IFREG | FIO_S_IRUSR | FIO_S_IWUSR;
	st->private_0 = 0x9417;
	{
		const uint8_t *h = cards[slot].raw + (found + 1) * 128;
		st->size = (uint32_t)h[4] | ((uint32_t)h[5] << 8) | ((uint32_t)h[6] << 16) | ((uint32_t)h[7] << 24);
	}
	return 0;
}

int saveVmcFree(const char *path)
{
	int slot = saveVmcSlot(path);
	return slot < 0 ? 0 : ps1CardFreeBlocks(cards[slot].raw) * PS1_BLOCK_SIZE;
}

int saveVmcFd(int fd) { return fd >= VMC_FD_BASE && fd < VMC_FD_BASE + VMC_HANDLES; }

int saveVmcOpen(const char *path, int flags)
{
	int slot = saveVmcSlot(path), i, found;
	const char *name = entryName(path);
	ps1_file_t *f;
	save_archive_t s = {0};
	if (slot < 0 || !name || !saveNameValid(name, 20))
		return -EINVAL;
	found = ps1CardFind(cards[slot].raw, name);
	if (found >= 0 && (flags & FIO_O_EXCL))
		return -EEXIST;
	if (found < 0 && !(flags & FIO_O_CREAT))
		return -ENOENT;
	for (i = 0; i < VMC_HANDLES; i++)
		if (handles[i].active && handles[i].slot == slot && !strcmp(handles[i].name, name))
			return -EBUSY;
	for (i = 0; i < VMC_HANDLES; i++)
		if (!handles[i].active)
			break;
	if (i == VMC_HANDLES)
		return -EMFILE;
	f = handles + i;
	memset(f, 0, sizeof(*f));
	f->data = malloc(15 * PS1_BLOCK_SIZE);
	if (!f->data)
		return -ENOMEM;
	if (found >= 0 && !(flags & FIO_O_TRUNC)) {
		int ret = ps1CardExtract(cards[slot].raw, found, &s);
		if (ret < 0) {
			saveArchiveFree(&s);
			free(f->data);
			f->data = NULL;
			return -EIO;
		}
		f->size = s.files[0].size;
		memcpy(f->data, s.files[0].data, f->size);
		saveArchiveFree(&s);
	}
	f->active = 1;
	f->slot = slot;
	f->flags = flags;
	f->dirty = (flags & FIO_O_TRUNC) != 0 || found < 0;
	strcpy(f->name, name);
	if (flags & FIO_O_APPEND)
		f->position = f->size;
	return VMC_FD_BASE + i;
}

int saveVmcRead(int fd, void *data, int size)
{
	ps1_file_t *f;
	if (!saveVmcFd(fd))
		return -EBADF;
	f = handles + (fd - VMC_FD_BASE);
	if (!f->active || size < 0 || (f->flags & 3) == FIO_O_WRONLY)
		return -EBADF;
	if (f->position >= f->size)
		return 0;
	if ((uint32_t)size > f->size - f->position)
		size = f->size - f->position;
	memcpy(data, f->data + f->position, size);
	f->position += size;
	return size;
}

int saveVmcWrite(int fd, const void *data, int size)
{
	ps1_file_t *f;
	if (!saveVmcFd(fd))
		return -EBADF;
	f = handles + (fd - VMC_FD_BASE);
	if (!f->active || size < 0 || (f->flags & 3) == FIO_O_RDONLY)
		return -EBADF;
	if ((uint32_t)size > 15 * PS1_BLOCK_SIZE - f->position) {
		f->failed = 1;
		return -ENOSPC;
	}
	if (f->position > f->size)
		memset(f->data + f->size, 0, f->position - f->size);
	memcpy(f->data + f->position, data, size);
	f->position += size;
	if (f->position > f->size)
		f->size = f->position;
	f->dirty = 1;
	return size;
}

s64 saveVmcSeek(int fd, s64 where, int how)
{
	ps1_file_t *f;
	if (!saveVmcFd(fd))
		return -EBADF;
	f = handles + (fd - VMC_FD_BASE);
	if (!f->active)
		return -EBADF;
	if (how == SEEK_CUR)
		where += f->position;
	else if (how == SEEK_END)
		where += f->size;
	else if (how != SEEK_SET)
		return -EINVAL;
	if (where < 0 || where > 15 * PS1_BLOCK_SIZE)
		return -EINVAL;
	f->position = where;
	return where;
}

int saveVmcPut(const char *path, const save_archive_t *s)
{
	int slot = saveVmcSlot(path), ret;
	uint8_t *next;
	if (slot < 0 || !saveCardRoot(path) || s->system != 1)
		return -EINVAL;
	if (ps1CardFind(cards[slot].raw, s->root.name) >= 0)
		return -EEXIST;
	if (ps1CardFreeBlocks(cards[slot].raw) < (int)(s->files[0].size / PS1_BLOCK_SIZE))
		return -ENOSPC;
	next = malloc(PS1_CARD_SIZE);
	if (!next)
		return -ENOMEM;
	memcpy(next, cards[slot].raw, PS1_CARD_SIZE);
	ret = ps1CardPut(next, s);
	if (ret >= 0)
		ret = commit(slot, next);
	free(next);
	return ret;
}

int saveVmcClose(int fd)
{
	ps1_file_t *f;
	int ret = 0;
	if (!saveVmcFd(fd))
		return -EBADF;
	f = handles + (fd - VMC_FD_BASE);
	if (!f->active)
		return -EBADF;
	if (f->failed)
		ret = -EIO;
	else if (f->dirty) {
		save_archive_t s = {0};
		save_entry_t entry = {0};
		uint8_t *next = malloc(PS1_CARD_SIZE);
		s.system = 1;
		s.count = 1;
		strcpy(s.root.name, f->name);
		strcpy(entry.name, f->name);
		entry.size = f->size;
		entry.data = f->data;
		s.files = &entry;
		if (!next)
			ret = -ENOMEM;
		else if (saveArchiveValidate(&s) < 0)
			ret = -EINVAL;
		else {
			memcpy(next, cards[f->slot].raw, PS1_CARD_SIZE);
			if (ps1CardFind(next, f->name) >= 0)
				ps1CardDelete(next, f->name);
			ret = ps1CardPut(next, &s);
			if (ret >= 0)
				ret = commit(f->slot, next);
		}
		free(next);
	}
	free(f->data);
	memset(f, 0, sizeof(*f));
	return ret;
}

void saveVmcAbort(int fd)
{
	if (saveVmcFd(fd))
		handles[fd - VMC_FD_BASE].dirty = 0;
}

static int edit(const char *path, const char *name)
{
	int slot = saveVmcSlot(path), ret, i;
	uint8_t *next;
	const char *old = entryName(path);
	if (slot < 0 || !old || !saveNameValid(old, 20))
		return -EINVAL;
	for (i = 0; i < VMC_HANDLES; i++)
		if (handles[i].active && handles[i].slot == slot)
			return -EBUSY;
	if (name && ps1CardFind(cards[slot].raw, name) >= 0)
		return -EEXIST;
	next = malloc(PS1_CARD_SIZE);
	if (!next)
		return -ENOMEM;
	memcpy(next, cards[slot].raw, PS1_CARD_SIZE);
	ret = name ? ps1CardRename(next, old, name) : ps1CardDelete(next, old);
	if (ret >= 0)
		ret = commit(slot, next);
	free(next);
	return ret;
}

int saveVmcDelete(const char *path) { return edit(path, NULL); }
int saveVmcRename(const char *path, const char *name) { return edit(path, name); }
