"""Exercise the production ELF hold gesture and both controller readers on a host."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    pos = source.index("{", start)
    depth = 0
    while pos < len(source):
        if source.startswith("//", pos):
            pos = source.index("\n", pos)
            continue
        if source.startswith("/*", pos):
            pos = source.index("*/", pos) + 2
            continue
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
        pos += 1
    raise AssertionError(signature)


HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t u8;
#define MAX_PATH 1025
#define PAD_CROSS 0x4000
#define PAD_CIRCLE 0x2000
#define PAD_STATE_STABLE 6
#define PAD_STATE_FINDCTP1 2
#define DS34BT_STATE_RUNNING 8
#define DS34USB_STATE_RUNNING 8
#define POLLING_TIME 10
static int swapKeys;
static struct { int reboot_iop_elf_load; } config, *setting = &config;
static u64 now, origin, release_after;
static u32 gesture_buttons;
static int polls;
static u64 Timer(void) { return now; }
static void DelayThread(int us) { assert(us == 10000); now += us / 1000; }
static u32 readPadButtonsHeld(void)
{
    assert(++polls < 200);
    return now - origin <= release_after ? gesture_buttons : 0;
}
static void gesture(u32 buttons, u64 duration)
{
    origin = now;
    release_after = duration;
    gesture_buttons = buttons;
    polls = 0;
}

/*PAD_READERS*/

struct padButtonStatus { uint16_t btns; u8 unused[30]; };
typedef struct { int option, attr, init_count, max_count; } ee_sema_t;
static int connected[4], readable[4], pad_reads[4];
static u32 hardware[4], paddata_t[4];
static struct padButtonStatus buttons_t[4];
static int ret[4], isRunning;
static const int semRunning = 1, semPoll = 2, semFinish = 3;
static volatile u32 held_buttons;
static int published;
static int padGetState(int port, int slot) { return connected[port]; }
static int padRead(int port, int slot, struct padButtonStatus *buttons)
{
    pad_reads[port]++;
    buttons->btns = 0xffff ^ hardware[port];
    return readable[port];
}
static int ds34bt_get_status(int port) { return connected[2]; }
static int ds34usb_get_status(int port) { return connected[3]; }
static int data(int port, u8 *dst)
{
    uint16_t buttons = 0xffff ^ hardware[port];
    memcpy(dst, &buttons, sizeof buttons);
    return readable[port];
}
static int ds34bt_get_data(int port, u8 *dst) { return data(2, dst); }
static int ds34usb_get_data(int port, u8 *dst) { return data(3, dst); }
static void ds34bt_set_rumble(int port, int a, int b) {}
static void ds34usb_set_rumble(int port, int a, int b) {}
static void WaitSema(int sem) {}
static void SignalSema(int sem) { if (sem == semPoll) published++; }
static int CreateSema(ee_sema_t *sem) { return 4; }
static void DeleteSema(int sem) { isRunning = 0; }
static void alrmHandler(void) {}
static void SetAlarm(int cycles, void (*handler)(void), int *sem) {}
static void ExitDeleteThread(void) {}

/*PRODUCTION*/

static void check_readers(void)
{
    /* A disconnected or failed port cannot contribute its previous button state. */
    for (int port = 0; port < 4; port++) {
        memset(connected, 0, sizeof connected);
        memset(readable, 0, sizeof readable);
        memset(hardware, 0, sizeof hardware);
        for (int i = 0; i < 4; i++) {
            paddata_t[i] = PAD_CROSS | PAD_CIRCLE;
            ret[i] = 1;
        }
        hardware[port] = PAD_CIRCLE;
        connected[port] = port < 2 ? PAD_STATE_STABLE : DS34BT_STATE_RUNNING;
        readable[port] = 1;
        if (port < 2)
            assert(readNativePadButtonsHeld() == PAD_CIRCLE);
        isRunning = 1;
        published = 0;
        padPollingThread(NULL);
        assert(published == 1);
        assert(readDs34PadButtonsHeld() == PAD_CIRCLE);
        readable[port] = 0;
        if (port < 2)
            assert(readNativePadButtonsHeld() == 0);
        isRunning = 1;
        padPollingThread(NULL);
        assert(readDs34PadButtonsHeld() == 0);
        readable[port] = 1;
        connected[port] = 0;
        if (port < 2)
            assert(readNativePadButtonsHeld() == 0);
        isRunning = 1;
        padPollingThread(NULL);
        assert(readDs34PadButtonsHeld() == 0);
    }
}

int main(void)
{
    const u64 durations[] = {0, 10, 100, 990, 999, 1000, 1001, 5000};
    for (int mapping = 0; mapping < 2; mapping++) {
        swapKeys = mapping;
        u32 confirm = mapping ? PAD_CROSS : PAD_CIRCLE;
        u32 other = mapping ? PAD_CIRCLE : PAD_CROSS;
        for (int saved = 0; saved < 2; saved++) {
            config.reboot_iop_elf_load = saved;
            for (unsigned i = 0; i < sizeof durations / sizeof durations[0]; i++) {
                /* Start near timer wrap to exercise elapsed-time arithmetic too. */
                now = UINT64_MAX - 500;
                gesture(confirm, durations[i]);
                assert(chooseElfLaunchIopReset(saved) == (durations[i] >= 1000 ? !saved : saved));
                assert(now - origin <= 1000);
                assert(config.reboot_iop_elf_load == saved);
            }
            gesture(0, 5000); /* Automatic/keyboard launch: no physical confirm. */
            assert(chooseElfLaunchIopReset(saved) == saved && now == origin);
            gesture(other, 5000); /* Only the configured Confirm button counts. */
            assert(chooseElfLaunchIopReset(saved) == saved && now == origin);

            gesture(confirm, 5000);
            prepareElfLaunchIopReset("udpfs:/APP.ELF");
            now += 20000; /* Device/header work can outlast the original hold. */
            assert(takePreparedElfLaunchIopReset("udpfs:/APP.ELF") == !saved);
            assert(takePreparedElfLaunchIopReset("udpfs:/APP.ELF") == -1);
            assert(config.reboot_iop_elf_load == saved);

            gesture(confirm, 50);
            prepareElfLaunchIopReset("mass:/APP.ELF");
            assert(takePreparedElfLaunchIopReset("mass:/APP.ELF") == saved);
            gesture(confirm, 5000);
            prepareElfLaunchIopReset("mass:/APP.ELF");
            assert(takePreparedElfLaunchIopReset("mass:/OTHER.ELF") == -1);
            assert(takePreparedElfLaunchIopReset("mass:/APP.ELF") == -1);
            gesture(confirm, 5000);
            prepareElfLaunchIopReset("mass:/APP.ELF");
            prepareElfLaunchIopReset(NULL); /* Failed selection or a new browser. */
            assert(takePreparedElfLaunchIopReset("mass:/APP.ELF") == -1);
        }
    }
    check_readers();
    puts("ELF launch hold: timing, both mappings, both reset choices, one-use capture, and all controller ports passed");
}
'''

actions = (ROOT / "src/main_actions.c").read_text()
native = (ROOT / "src/pad.c").read_text()
ds34 = (ROOT / "src/pad_ds34.c").read_text()
launch = actions[actions.index("static char prepared_elf_path"):actions.index("//CleanUpForExec")]
readers = "\n".join([
    function(native, "u32 readPadButtonsHeld(void)").replace("readPadButtonsHeld", "readNativePadButtonsHeld"),
    function(ds34, "u32 readPadButtonsHeld(void)").replace("readPadButtonsHeld", "readDs34PadButtonsHeld"),
    function(ds34, "void padPollingThread(void *args)"),
])
source = HARNESS.replace("/*PAD_READERS*/", launch).replace("/*PRODUCTION*/", readers)
with tempfile.TemporaryDirectory(prefix="wle-elf-hold-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(source)
    compiler = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(compiler + flags + ["-std=c99", "-Wall", "-Wextra", "-Werror",
                   "-Wno-unused-parameter", str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
