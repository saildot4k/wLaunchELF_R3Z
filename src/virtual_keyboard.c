//--------------------------------------------------------------
//File name:   virtual_keyboard.c
//--------------------------------------------------------------
#include "launchelf.h"

static const char *vkey_layout_config_names[VKEY_LAYOUT_COUNT] = {
    "abc",
    "qwerty",
    "dvorak",
    "azerty",
    "qwertz",
    "abnt",
    "abnt2",
};

static const char *vkey_layout_display_names[VKEY_LAYOUT_COUNT] = {
    "ABC",
    "QWERTY",
    "DVORAK",
    "AZERTY",
    "QWERTZ",
    "ABNT",
    "ABNT2",
};

enum {
    VKEY_SHIFT_NORMAL = 0,
    VKEY_SHIFT_CAPS,
    VKEY_SHIFT_COUNT,
    VKEY_SPACE_INDEX = (4 * VKEY_LAYOUT_COLS) + 4
};

static const char *vkey_layout_rows[VKEY_LAYOUT_COUNT][VKEY_SHIFT_COUNT][VKEY_LAYOUT_ROWS] = {
    {
        {
            "1234567890-=     ",
            "abcdefghijklmn[] ",
            "opqrstuvwxyz;'\\  ",
            " .,/`            ",
            "    _            ",
        },
        {
            "!@#$%^&*()_+     ",
            "ABCDEFGHIJKLMN{} ",
            "OPQRSTUVWXYZ:\"|  ",
            " <>?~            ",
            "    _            ",
        },
    },
    {
        {
            "1234567890-=     ",
            "qwertyuiop[]     ",
            "asdfghjkl;'\\     ",
            " zxcvbnm,./`     ",
            "    _            ",
        },
        {
            "!@#$%^&*()_+     ",
            "QWERTYUIOP{}     ",
            "ASDFGHJKL:\"|     ",
            " ZXCVBNM<>?~     ",
            "    _            ",
        },
    },
    {
        {
            "1234567890-=     ",
            "',.pyfgcrl/=     ",
            "aoeuidhtns-\\     ",
            " ;qjkxbmwvz`     ",
            "    _            ",
        },
        {
            "!@#$%^&*()_+     ",
            "\"<>PYFGCRL?+     ",
            "AOEUIDHTNS_|     ",
            " :QJKXBMWVZ~     ",
            "    _            ",
        },
    },
    {
        {
            "1234567890-=     ",
            "azertyuiop[]     ",
            "qsdfghjklm;'\\    ",
            " wxcvbn,./`      ",
            "    _            ",
        },
        {
            "!@#$%^&*()_+     ",
            "AZERTYUIOP{}     ",
            "QSDFGHJKLM:\"|    ",
            " WXCVBN<>?~      ",
            "    _            ",
        },
    },
    {
        {
            "1234567890-=     ",
            "qwertzuiop[]     ",
            "asdfghjkl;'\\     ",
            " yxcvbnm,./`     ",
            "    _            ",
        },
        {
            "!@#$%^&*()_+     ",
            "QWERTZUIOP{}     ",
            "ASDFGHJKL:\"|     ",
            " YXCVBNM<>?~     ",
            "    _            ",
        },
    },
    {
        {
            "'1234567890-=    ",
            "qwertyuiop'[     ",
            "asdfghjkl;~]     ",
            " \\zxcvbnm,.;     ",
            "    _            ",
        },
        {
            "\"!@#$%^&*()_+    ",
            "QWERTYUIOP`{     ",
            "ASDFGHJKL:^}     ",
            " |ZXCVBNM<>:     ",
            "    _            ",
        },
    },
    {
        {
            "'1234567890-=    ",
            "qwertyuiop'[     ",
            "asdfghjkl;~]     ",
            " \\zxcvbnm,.;/    ",
            "    _            ",
        },
        {
            "\"!@#$%^&*()_+    ",
            "QWERTYUIOP`{     ",
            "ASDFGHJKL:^}     ",
            " |ZXCVBNM<>:?    ",
            "    _            ",
        },
    },
};

int normalizeVirtualKeyboardLayout(int layout)
{
    while (layout < 0)
        layout += VKEY_LAYOUT_COUNT;
    while (layout >= VKEY_LAYOUT_COUNT)
        layout -= VKEY_LAYOUT_COUNT;
    return layout;
}

const char *getVirtualKeyboardLayoutConfigName(int layout)
{
    return vkey_layout_config_names[normalizeVirtualKeyboardLayout(layout)];
}

const char *getVirtualKeyboardLayoutDisplayName(int layout)
{
    return vkey_layout_display_names[normalizeVirtualKeyboardLayout(layout)];
}

int getVirtualKeyboardLayoutByConfigName(const char *name)
{
    int i;

    if (name == NULL || name[0] == '\0')
        return -1;
    for (i = 0; i < VKEY_LAYOUT_COUNT; i++) {
        if (!stricmp(name, vkey_layout_config_names[i]) || !stricmp(name, vkey_layout_display_names[i]))
            return i;
    }
    if (!stricmp(name, "0"))
        return VKEY_LAYOUT_ABC;
    if (!stricmp(name, "1"))
        return VKEY_LAYOUT_QWERTY;
    if (!stricmp(name, "2"))
        return VKEY_LAYOUT_DVORAK;
    if (!stricmp(name, "3"))
        return VKEY_LAYOUT_AZERTY;
    if (!stricmp(name, "4"))
        return VKEY_LAYOUT_QWERTZ;
    if (!stricmp(name, "5"))
        return VKEY_LAYOUT_ABNT;
    if (!stricmp(name, "6"))
        return VKEY_LAYOUT_ABNT2;
    if (!stricmp(name, "n"))
        return VKEY_LAYOUT_ABNT;
    return -1;
}

static char getVirtualKeyboardRawChar(int layout, int index, int caps)
{
    int row;
    int col;
    int shift = caps ? VKEY_SHIFT_CAPS : VKEY_SHIFT_NORMAL;

    if (index < 0 || index >= VKEY_LAYOUT_SIZE)
        return ' ';
    row = index / VKEY_LAYOUT_COLS;
    col = index % VKEY_LAYOUT_COLS;
    return vkey_layout_rows[normalizeVirtualKeyboardLayout(layout)][shift][row][col];
}

char getVirtualKeyboardLayoutDisplayChar(int layout, int index, int caps)
{
    if (index == VKEY_SPACE_INDEX)
        return ' ';
    return getVirtualKeyboardRawChar(layout, index, caps);
}

char getVirtualKeyboardLayoutChar(int layout, int index, int caps)
{
    if (index == VKEY_SPACE_INDEX)
        return ' ';
    return getVirtualKeyboardRawChar(layout, index, caps);
}

int isVirtualKeyboardLayoutKey(int layout, int index)
{
    if (index < 0 || index >= VKEY_LAYOUT_SIZE)
        return 0;
    if (index == VKEY_SPACE_INDEX)
        return 1;
    return getVirtualKeyboardRawChar(layout, index, VKEY_SHIFT_NORMAL) != ' ';
}

static void getVirtualKeyboardLayoutColumnRange(int layout, int *first_col, int *last_col)
{
    int row;
    int col;
    int index;
    int first = VKEY_LAYOUT_COLS;
    int last = 0;

    for (row = 0; row < VKEY_LAYOUT_ROWS; row++) {
        for (col = 0; col < VKEY_LAYOUT_COLS; col++) {
            index = row * VKEY_LAYOUT_COLS + col;
            if (isVirtualKeyboardLayoutKey(layout, index)) {
                if (col < first)
                    first = col;
                if (col > last)
                    last = col;
            }
        }
    }

    if (first == VKEY_LAYOUT_COLS) {
        first = 0;
        last = 0;
    }
    *first_col = first;
    *last_col = last;
}

int getVirtualKeyboardLayoutFirstColumn(int layout)
{
    int first;
    int last;

    getVirtualKeyboardLayoutColumnRange(layout, &first, &last);
    return first;
}

int getVirtualKeyboardLayoutColumnCount(int layout)
{
    int first;
    int last;

    getVirtualKeyboardLayoutColumnRange(layout, &first, &last);
    return last - first + 1;
}

static int findSelectableInRow(int layout, int row, int col)
{
    int offset;
    int left;
    int right;
    int index;

    if (row < 0 || row >= VKEY_LAYOUT_ROWS)
        return -1;
    for (offset = 0; offset < VKEY_LAYOUT_COLS; offset++) {
        left = col - offset;
        right = col + offset;
        if (left >= 0) {
            index = row * VKEY_LAYOUT_COLS + left;
            if (isVirtualKeyboardLayoutKey(layout, index))
                return index;
        }
        if (right < VKEY_LAYOUT_COLS && right != left) {
            index = row * VKEY_LAYOUT_COLS + right;
            if (isVirtualKeyboardLayoutKey(layout, index))
                return index;
        }
    }
    return -1;
}

int getVirtualKeyboardLayoutNextKey(int layout, int index, int dx, int dy)
{
    int row;
    int col;
    int next;
    int tries;

    if (!isVirtualKeyboardLayoutKey(layout, index)) {
        if (index >= 0 && index < VKEY_LAYOUT_SIZE)
            index = findSelectableInRow(layout, index / VKEY_LAYOUT_COLS, index % VKEY_LAYOUT_COLS);
        else
            index = findSelectableInRow(layout, 0, 0);
    }
    if (index < 0)
        return 0;

    row = index / VKEY_LAYOUT_COLS;
    col = index % VKEY_LAYOUT_COLS;
    if (dx != 0) {
        for (tries = 0; tries < VKEY_LAYOUT_COLS; tries++) {
            col = (col + dx + VKEY_LAYOUT_COLS) % VKEY_LAYOUT_COLS;
            next = row * VKEY_LAYOUT_COLS + col;
            if (isVirtualKeyboardLayoutKey(layout, next))
                return next;
        }
    } else if (dy != 0) {
        for (tries = 0; tries < VKEY_LAYOUT_ROWS; tries++) {
            row = (row + dy + VKEY_LAYOUT_ROWS) % VKEY_LAYOUT_ROWS;
            next = findSelectableInRow(layout, row, col);
            if (next >= 0)
                return next;
        }
    }
    return index;
}

int getVirtualKeyboardEditorLayoutIndex(int editor_index)
{
    int row;
    int col;

    if (editor_index < 0 || editor_index >= VKEY_EDITOR_SIZE)
        return -1;
    row = editor_index / VKEY_EDITOR_COLS;
    col = editor_index % VKEY_EDITOR_COLS;
    if (col < 2 || col >= VKEY_EDITOR_COLS - 1)
        return -1;
    return row * VKEY_LAYOUT_COLS + (col - 2);
}

int isVirtualKeyboardEditorKey(int layout, int editor_index)
{
    int layout_index;
    int col;

    if (editor_index < 0 || editor_index >= VKEY_EDITOR_SIZE)
        return 0;
    col = editor_index % VKEY_EDITOR_COLS;
    if (col < 2 || col >= VKEY_EDITOR_COLS - 1)
        return 1;
    layout_index = getVirtualKeyboardEditorLayoutIndex(editor_index);
    return isVirtualKeyboardLayoutKey(layout, layout_index);
}

char getVirtualKeyboardEditorChar(int layout, int editor_index, int caps)
{
    int layout_index = getVirtualKeyboardEditorLayoutIndex(editor_index);

    if (layout_index < 0)
        return ' ';
    return getVirtualKeyboardLayoutChar(layout, layout_index, caps);
}

char getVirtualKeyboardEditorDisplayChar(int layout, int editor_index, int caps)
{
    int layout_index = getVirtualKeyboardEditorLayoutIndex(editor_index);

    if (layout_index < 0)
        return ' ';
    return getVirtualKeyboardLayoutDisplayChar(layout, layout_index, caps);
}

static int getVirtualKeyboardEditorNextInRow(int layout, int row, int col)
{
    int offset;
    int left;
    int right;
    int index;

    if (row < 0 || row >= VKEY_LAYOUT_ROWS)
        return -1;
    for (offset = 0; offset < VKEY_EDITOR_COLS; offset++) {
        left = col - offset;
        right = col + offset;
        if (left >= 0) {
            index = row * VKEY_EDITOR_COLS + left;
            if (isVirtualKeyboardEditorKey(layout, index))
                return index;
        }
        if (right < VKEY_EDITOR_COLS && right != left) {
            index = row * VKEY_EDITOR_COLS + right;
            if (isVirtualKeyboardEditorKey(layout, index))
                return index;
        }
    }
    return -1;
}

static int getVirtualKeyboardEditorNextCharInRow(int layout, int row, int editor_col)
{
    int layout_col;
    int layout_index;

    if (row < 0 || row >= VKEY_LAYOUT_ROWS)
        return -1;
    layout_col = editor_col - 2;
    if (layout_col < 0)
        layout_col = 0;
    else if (layout_col >= VKEY_LAYOUT_COLS)
        layout_col = VKEY_LAYOUT_COLS - 1;
    layout_index = findSelectableInRow(layout, row, layout_col);
    if (layout_index < 0)
        return -1;
    return row * VKEY_EDITOR_COLS + (layout_index % VKEY_LAYOUT_COLS) + 2;
}

int getVirtualKeyboardEditorNextKey(int layout, int editor_index, int dx, int dy)
{
    int row;
    int col;
    int next;
    int tries;

    if (!isVirtualKeyboardEditorKey(layout, editor_index)) {
        if (editor_index >= 0 && editor_index < VKEY_EDITOR_SIZE)
            editor_index = getVirtualKeyboardEditorNextInRow(layout, editor_index / VKEY_EDITOR_COLS, editor_index % VKEY_EDITOR_COLS);
        else
            editor_index = getVirtualKeyboardEditorNextInRow(layout, 0, 0);
    }
    if (editor_index < 0)
        return 0;

    row = editor_index / VKEY_EDITOR_COLS;
    col = editor_index % VKEY_EDITOR_COLS;
    if (dx != 0) {
        for (tries = 0; tries < VKEY_EDITOR_COLS; tries++) {
            col = (col + dx + VKEY_EDITOR_COLS) % VKEY_EDITOR_COLS;
            next = row * VKEY_EDITOR_COLS + col;
            if (isVirtualKeyboardEditorKey(layout, next))
                return next;
        }
    } else if (dy != 0) {
        for (tries = 0; tries < VKEY_LAYOUT_ROWS; tries++) {
            row = (row + dy + VKEY_LAYOUT_ROWS) % VKEY_LAYOUT_ROWS;
            if (col >= 2 && col < VKEY_EDITOR_COLS - 1)
                next = getVirtualKeyboardEditorNextCharInRow(layout, row, col);
            else
                next = getVirtualKeyboardEditorNextInRow(layout, row, col);
            if (next >= 0)
                return next;
        }
    }
    return editor_index;
}
//--------------------------------------------------------------
//End of file: virtual_keyboard.c
//--------------------------------------------------------------
