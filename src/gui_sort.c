#include "gui_sort.h"
#include "filer_shared.h"

// cmpFile returns negative when entry a should be ordered before b.
int cmpFile(FILEINFO *a, FILEINFO *b)
{
	char ca, cb;
	int i, n, ret, aElf = FALSE, bElf = FALSE, t = (file_sort == 2);
	int a_is_dir = (a->stats.AttrFile & sceMcFileAttrSubdir) != 0;
	int b_is_dir = (b->stats.AttrFile & sceMcFileAttrSubdir) != 0;

	if (file_sort == 0)
		return 0;

	if (a_is_dir == b_is_dir) {
		if (!a_is_dir) {
			if (genCmpFileExt(a->name, "ELF"))
				aElf = TRUE;
			if (genCmpFileExt(b->name, "ELF"))
				bElf = TRUE;
			if (aElf && !bElf)
				return -1;
			else if (!aElf && bElf)
				return 1;
		}
		if (file_sort == 3) {
			t = (file_show == 2);
			if (time_valid) {
				u64 time_a = *(u64 *)&a->stats._Modify;
				u64 time_b = *(u64 *)&b->stats._Modify;
				if (time_a > time_b)
					return -1;
				if (time_a < time_b)
					return 1;
			}
		}
		if (t) {
			if (a->title[0] != 0 && b->title[0] == 0)
				return -1;
			else if (a->title[0] == 0 && b->title[0] != 0)
				return 1;
			else if (a->title[0] == 0 && b->title[0] == 0)
				t = FALSE;
		}
		if (t)
			n = strlen((const char *)a->title);
		else
			n = strlen(a->name);
		for (i = 0; i <= n; i++) {
			if (t) {
				ca = a->title[i];
				cb = b->title[i];
			} else {
				ca = a->name[i];
				cb = b->name[i];
				if (ca >= 'a' && ca <= 'z')
					ca -= 0x20;
				if (cb >= 'a' && cb <= 'z')
					cb -= 0x20;
			}
			ret = ca - cb;
			if (ret != 0)
				return ret;
		}
		return 0;
	}

	if (a->stats.AttrFile & sceMcFileAttrSubdir)
		return -1;
	else
		return 1;
}

void sort(FILEINFO *a, int left, int right)
{
	FILEINFO tmp, pivot;
	int i, p;

	if (left < right) {
		pivot = a[left];
		p = left;
		for (i = left + 1; i <= right; i++) {
			if (cmpFile(&a[i], &pivot) < 0) {
				p = p + 1;
				tmp = a[p];
				a[p] = a[i];
				a[i] = tmp;
			}
		}
		a[left] = a[p];
		a[p] = pivot;
		sort(a, left, p - 1);
		sort(a, p + 1, right);
	}
}
