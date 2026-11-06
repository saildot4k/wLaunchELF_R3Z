/* Build with -include tests/save_test_platform.h and the production modules. */
#include "save_manager.h"
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <strings.h>

int vmcMounted[2], size_valid, time_valid, fileMode;
static char test_root[MAX_PATH];
static int writes, fail_write, renames, fail_rename, metadata;

static const char *mapped(const char *path, char full[MAX_PATH + 20])
{
	if (!strncmp(path, "mc0:", 4) || !strncmp(path, "mc1:", 4)) {
		snprintf(full, MAX_PATH + 20, "%s/mc%c/%s", test_root, path[2], path + 4 + (*(path + 4) == '/'));
		return full;
	}
	return path;
}
int genFixPath(const char *p, char *out)
{
	strcpy(out, p);
	return 99;
}
int genGetStat(const char *path, iox_stat_t *st)
{
	struct stat s;
	char full[MAX_PATH + 20];
	if (saveVmcSlot(path) >= 0)
		return saveVmcStat(path, st);
	if (stat(mapped(path, full), &s) < 0)
		return -errno;
	memset(st, 0, sizeof(*st));
	st->size = s.st_size;
	st->mode = S_ISDIR(s.st_mode) ? FIO_S_IFDIR : FIO_S_IFREG;
	return 0;
}
int genOpen(const char *path, int flags)
{
	char full[MAX_PATH + 20];
	int f = 0, fd;
	if (saveVmcSlot(path) >= 0)
		return saveVmcOpen(path, flags);
	if ((flags & 3) == FIO_O_WRONLY)
		f = O_WRONLY;
	if ((flags & 3) == FIO_O_RDWR)
		f = O_RDWR;
	if (flags & FIO_O_CREAT)
		f |= O_CREAT;
	if (flags & FIO_O_EXCL)
		f |= O_EXCL;
	if (flags & FIO_O_TRUNC)
		f |= O_TRUNC;
	fd = open(mapped(path, full), f, 0600);
	return fd < 0 ? -errno : fd;
}
int openPathForRead(const char *p, char *out, size_t n)
{
	(void)out;
	(void)n;
	return genOpen(p, FIO_O_RDONLY);
}
int genRead(int fd, void *p, int n) { return saveVmcFd(fd) ? saveVmcRead(fd, p, n) : (int)read(fd, p, n); }
int genWrite(int fd, void *p, int n)
{
	if (++writes == fail_write)
		return -EIO;
	return saveVmcFd(fd) ? saveVmcWrite(fd, p, n) : (int)write(fd, p, n);
}
int genClose(int fd) { return saveVmcFd(fd) ? saveVmcClose(fd) : close(fd); }
s64 genLseek(int fd, s64 off, int how) { return saveVmcFd(fd) ? saveVmcSeek(fd, off, how) : lseek(fd, off, how); }
int genRemove(char *p)
{
	char full[MAX_PATH + 20];
	return saveVmcSlot(p) >= 0 ? saveVmcDelete(p) : unlink(mapped(p, full));
}
int genRmdir(char *p)
{
	char full[MAX_PATH + 20];
	return rmdir(mapped(p, full));
}
int genMkdir(const char *p, int mode)
{
	char full[MAX_PATH + 20];
	(void)mode;
	return mkdir(mapped(p, full), 0700) < 0 ? -errno : 0;
}
int genCmpFileExt(const char *p, const char *ext)
{
	p = strrchr(p, '.');
	return p && !strcasecmp(p + 1, ext);
}
int getDir(const char *p, FILEINFO *f)
{
	(void)p;
	(void)f;
	return 0;
}
int fileXioRename(const char *a, const char *b)
{
	char aa[MAX_PATH + 20], bb[MAX_PATH + 20];
	if (++renames == fail_rename)
		return -EIO;
	return rename(mapped(a, aa), mapped(b, bb)) < 0 ? -errno : 0;
}
int fileXioChStat(const char *p, iox_stat_t *s, int mode)
{
	(void)p;
	(void)s;
	(void)mode;
	metadata++;
	return 0;
}
void ensureMemoryCardPortAccessible(int port) { (void)port; }
int mcGetInfo(int port, int slot, int *type, int *free_blocks, int *formatted)
{
	(void)slot;
	if (type)
		*type = port == 0 ? 2 : 1;
	if (free_blocks)
		*free_blocks = 15;
	if (formatted)
		*formatted = 1;
	return 0;
}
int mcSync(int mode, int *cmd, int *ret)
{
	(void)mode;
	(void)cmd;
	if (ret)
		*ret = 0;
	return 0;
}
int mcSetFileInfo(int p, int slot, const char *name, const sceMcTblGetDir *st, int mode)
{
	(void)p;
	(void)slot;
	(void)name;
	(void)st;
	(void)mode;
	metadata++;
	return 0;
}
int keyboard(char *s, int n)
{
	(void)s;
	(void)n;
	return -1;
}

static void encodeFile(const char *path, save_archive_t *s)
{
	uint8_t *data;
	size_t size;
	assert(saveEncode(s, SAVE_PSV, &data, &size) == 0);
	assert(saveWriteNew(path, data, size) == 0);
	free(data);
}
static void assertFile(const char *path, const uint8_t *expected, size_t size)
{
	uint8_t *data;
	size_t n;
	assert(saveReadFile(path, &data, &n) == 0);
	assert(n == size);
	assert(!memcmp(data, expected, size));
	free(data);
}
static void removeTree(const char *path)
{
	DIR *dir = opendir(path);
	struct dirent *e;
	if (dir) {
		while ((e = readdir(dir))) {
			char full[MAX_PATH];
			struct stat st;
			if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
				continue;
			snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
			assert(!lstat(full, &st));
			if (S_ISDIR(st.st_mode))
				removeTree(full);
			else
				assert(!unlink(full));
		}
		closedir(dir);
		assert(!rmdir(path));
	}
}
int main(void)
{
	char dir[] = "/tmp/wle-save-manager-XXXXXX", ps1[MAX_PATH], ps2[MAX_PATH], image[MAX_PATH], path[MAX_PATH];
	uint8_t *raw = malloc(PS1_CARD_SIZE), *before = malloc(PS1_CARD_SIZE), payload[8192] = {'S', 'C'};
	save_entry_t entries[2] = {{0}};
	save_archive_t a = {0};
	iox_stat_t st;
	FILEINFO files[15], file = {0};
	int fd, old_writes;
	assert(raw && before && mkdtemp(dir));
	strcpy(test_root, dir);
	snprintf(path, sizeof(path), "%s/mc0", dir);
	assert(!mkdir(path, 0700));
	snprintf(path, sizeof(path), "%s/mc1", dir);
	assert(!mkdir(path, 0700));
	snprintf(ps1, sizeof(ps1), "%s/PS1.PSV", dir);
	snprintf(ps2, sizeof(ps2), "%s/PS2.PSV", dir);
	snprintf(image, sizeof(image), "%s/CARD.VMC", dir);
	a.system = 1;
	a.count = 1;
	strcpy(a.root.name, "BASLUS-99999TEST");
	strcpy(entries[0].name, a.root.name);
	entries[0].data = payload;
	entries[0].size = sizeof(payload);
	a.files = entries;
	encodeFile(ps1, &a);
	a.system = 2;
	a.count = 2;
	a.root.mode = 0x8427;
	strcpy(a.root.name, "BASLUS-99999PS2");
	strcpy(entries[0].name, "DATA");
	strcpy(entries[1].name, "ICON");
	entries[0].mode = entries[1].mode = 0x8497;
	entries[1].data = payload;
	entries[1].size = sizeof(payload);
	encodeFile(ps2, &a);
	old_writes = writes;
	assert(saveImport(ps1, "mc0:/") == -EXDEV);
	assert(saveImport(ps2, "mc1:/") == -EXDEV);
	assert(writes == old_writes); /* Card-family checks happen before any write. */
	assert(saveImport(ps2, "mc0:/") == 0);
	assert(metadata == 3);
	assertFile("mc0:/BASLUS-99999PS2/DATA", payload, sizeof(payload));
	assert(saveImport(ps2, "mc0:/") == -EEXIST);
	assert(saveImport(ps1, "mc1:/") == 0);
	assertFile("mc1:/BASLUS-99999TEST", payload, sizeof(payload));
	/* Presets keep exact filename case/extensions and skip the keyboard prompt. */
	{
		const char *names[] = {"MC1.vmc", "MC2.vmc", "SLOT0.VMC", "SLOT1.VMC"};
		char destination[MAX_PATH];
		FILEINFO save = {0};
		strcpy(save.name, "BASLUS-99999TEST");
		save.stats.FileSizeByte = sizeof(payload);
		snprintf(destination, sizeof(destination), "%s/", dir);
		for (int i = 0; i < 4; i++) {
			uint8_t *disk;
			size_t length;
			int format;
			assert(saveBackupCard("mc1:/", &save, 1, destination, CARD_RAW, names[i]) == 0);
			snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
			assert(saveReadFile(path, &disk, &length) == 0);
			assert(length == PS1_CARD_SIZE);
			assert(ps1CardDecode(disk, length, raw, &format) == 0 && format == CARD_RAW);
			assert(ps1CardFind(raw, save.name) >= 0);
			assert(saveBackupCard("mc1:/", &save, 1, destination, CARD_RAW, names[i]) == -EEXIST);
			assertFile(path, disk, length);
			free(disk);
		}
	}
	/* Mid-import failure removes only the new partial directory. */
	strcpy(a.root.name, "FAILED");
	snprintf(path, sizeof(path), "%s/FAIL.PSV", dir);
	encodeFile(path, &a);
	fail_write = writes + 2;
	assert(saveImport(path, "mc0:/") < 0);
	fail_write = 0;
	assert(genGetStat("mc0:/FAILED", &st) < 0);
	/* Mount, import, rename and delete through the production PS1 VMC layer. */
	ps1CardFormat(raw);
	assert(saveWriteNew(image, raw, PS1_CARD_SIZE) == 0);
	assert(saveVmcMount(0, image) == 1);
	vmcMounted[0] = 1;
	assert(saveImport(ps1, "vmc0:/") == 0);
	assert(saveVmcList("vmc0:/", files, 15) == 1);
	assertFile("vmc0:/BASLUS-99999TEST", payload, sizeof(payload));
	assert(saveVmcRename("vmc0:/BASLUS-99999TEST", "RENAMED") == 0);
	assert(saveVmcFree("vmc0:/") == 14 * 8192);
	{
		uint8_t *data;
		size_t size;
		assert(!saveReadFile(image, &data, &size));
		memcpy(before, data, PS1_CARD_SIZE);
		free(data);
	}
	/* Failure publishing the staged image rolls the original filename back. */
	fail_rename = renames + 2;
	assert(saveVmcRename("vmc0:/RENAMED", "OTHER") < 0);
	fail_rename = 0;
	assertFile(image, before, PS1_CARD_SIZE);
	assertFile("vmc0:/RENAMED", payload, sizeof(payload));
	/* Cancelled and invalid writes leave an existing save intact. */
	fd = saveVmcOpen("vmc0:/RENAMED", FIO_O_WRONLY | FIO_O_TRUNC);
	assert(fd >= 0);
	assert(saveVmcWrite(fd, payload, 8192) == 8192);
	saveVmcAbort(fd);
	assert(saveVmcClose(fd) == 0);
	assertFile(image, before, PS1_CARD_SIZE);
	fd = saveVmcOpen("vmc0:/RENAMED", FIO_O_WRONLY | FIO_O_TRUNC);
	assert(fd >= 0);
	assert(saveVmcWrite(fd, "bad", 3) == 3);
	assert(saveVmcClose(fd) < 0);
	assertFile(image, before, PS1_CARD_SIZE);
	strcpy(file.name, "RENAMED");
	file.stats.FileSizeByte = 8192;
	assert(saveCheckCardCopy("vmc0:/", "mc0:/", &file, PM_NORMAL) == -EXDEV);
	assert(saveCheckCardCopy("vmc0:/", "mc1:/", &file, PM_NORMAL) == 0);
	assert(saveCheckCardCopy("vmc0:/", "mc1:/", &file, PM_PSU_RESTORE) == -EXDEV);
	assert(saveVmcDelete("vmc0:/RENAMED") == 0);
	assert(saveVmcFree("vmc0:/") == 15 * 8192);
	saveVmcUnmount(0);
	/* A container's original format survives edits, including signing/encryption. */
	for (int format = CARD_GME; format <= CARD_MCX; format++) {
		uint8_t *encoded, *disk;
		size_t length, disk_length;
		int actual;
		ps1CardFormat(raw);
		assert(!ps1CardEncode(raw, format, NULL, 0, &encoded, &length));
		snprintf(path, sizeof(path), "%s/CARD-%d.bin", dir, format);
		assert(!saveWriteNew(path, encoded, length));
		free(encoded);
		assert(saveVmcMount(0, path) == 1);
		assert(!saveImport(ps1, "vmc0:/"));
		assert(!saveReadFile(path, &disk, &disk_length));
		assert(!ps1CardDecode(disk, disk_length, raw, &actual));
		assert(actual == format && ps1CardFind(raw, "BASLUS-99999TEST") >= 0);
		free(disk);
		saveVmcUnmount(0);
	}
	free(raw);
	free(before);
	removeTree(dir);
	puts("Save manager: wrong-card rejection, import rollback and VMC transaction tests passed");
	return 0;
}
