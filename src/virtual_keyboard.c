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
    "n",
};

static const char *vkey_layout_display_names[VKEY_LAYOUT_COUNT] = {
    "ABC",
    "QWERTY",
    "DVORAK",
    "Azerty",
    "Qwertz",
    "N",
};

static const char *vkey_layout_rows[VKEY_LAYOUT_COUNT][VKEY_LAYOUT_ROWS] = {
    {
        "  01abcdefghijklm:; ",
        "  23nopqrstuvwxyz., ",
        "  45!@#$%^&*()_+=-  ",
        "  67{}[]<>/\\'\"`~|   ",
        "  89          ?     ",
    },
    {
        "  01qwertyuiop[]\\   ",
        "  23asdfghjkl;'-    ",
        "  45zxcvbnm,./      ",
        "  67!@#$%^&*()_+=   ",
        "  89{}<>|`~:\"       ",
    },
    {
        "  01',.pyfgcrl/=\\   ",
        "  23aoeuidhtns-     ",
        "  45;qjkxbmwvz      ",
        "  67!@#$%^&*()_+=   ",
        "  89{}<>|`~:\"       ",
    },
    {
        "  01azertyuiop^$    ",
        "  23qsdfghjklm;*    ",
        "  45wxcvbn,;:!      ",
        "  67!@#$%&()_+=-    ",
        "  89{}<>|`~\"'/\\     ",
    },
    {
        "  01qwertzuiop[]\\   ",
        "  23asdfghjkl;'-    ",
        "  45yxcvbnm,./      ",
        "  67!@#$%^&*()_+=   ",
        "  89{}<>|`~:\"       ",
    },
    {
        "  010123456789-+    ",
        "  23abcdefghijklm   ",
        "  45nopqrstuvwxyz   ",
        "  67.,;:/\\'\"!?_     ",
        "  89()[]{}<>`       ",
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
        return VKEY_LAYOUT_N;
    return -1;
}

char getVirtualKeyboardLayoutChar(int layout, int index, int caps)
{
    char c;
    int row;
    int col;

    if (index < 0 || index >= VKEY_LAYOUT_SIZE)
        return ' ';
    row = index / VKEY_LAYOUT_COLS;
    col = index % VKEY_LAYOUT_COLS;
    c = vkey_layout_rows[normalizeVirtualKeyboardLayout(layout)][row][col];
    if (caps && c >= 'a' && c <= 'z')
        c = (char)(c - ('a' - 'A'));
    return c;
}
//--------------------------------------------------------------
//End of file: virtual_keyboard.c
//--------------------------------------------------------------
