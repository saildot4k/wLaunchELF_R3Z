"""Exercise production SMAP startup gates and require both inherited bugs to fail."""
# Adapted from saildot4k/Launchstation's SMAP regression tests.
from pathlib import Path
import re
import subprocess
import tempfile
import os
import shlex

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'iop/udpfs_smap/src/smap.c').read_text(encoding='utf-8')
start = source.index('static int InitPHY(')
end = source.index('    if (!EnableAutoNegotiation)', start)
phy = source[start:end] + '    return 1;\n}\n'
init = source[source.index('int smap_init('):]
capability = re.search(r'    if \(([^\n]+)\)\n        return -1;', init).group(1)

HARNESS = r'''
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
struct SmapDriverData { volatile u8 *emac3_regbase; int NetDevStopFlag; };
#define SMAP_DsPHYTER_BMCR 0
#define SMAP_PHY_BMCR_RST 0x8000
#define SPD_CAPS_SMAP 1
#define SPD_R_REV_3 0
#define SPD_REG16(reg) caps
#define M_DEBUG(...) ((void)0)
static int EnableVerboseOutput;
static int reads, waits, writes, busy_reads;
static u16 caps;
static void _smap_write_phy(volatile u8 *base, int reg, u16 value) {
    (void)base; (void)reg;
    if(value != SMAP_PHY_BMCR_RST) writes += 1000;
    writes++;
}
static u16 _smap_read_phy(volatile u8 *base, int reg) {
    (void)base; (void)reg;
    return reads++ < busy_reads ? SMAP_PHY_BMCR_RST : 0;
}
static void DelayThread(int us) { if(us != 1000) waits += 1000; waits++; }
@PHY@
static int probe_smap(void) { if(@CAPABILITY@) return -1; return 0; }
static int check_phy(int busy, int stopped, int result, int nreads, int nwaits) {
    struct SmapDriverData state = {0, stopped};
    reads=waits=writes=0;busy_reads=busy;
    int actual=InitPHY(&state);
    if(actual!=result || reads!=nreads || waits!=nwaits || writes!=1) {
        fprintf(stderr,"PHY busy=%d stop=%d: result=%d reads=%d waits=%d writes=%d\n",
                busy,stopped,actual,reads,waits,writes);
        return 1;
    }
    return 0;
}
int main(void) {
    int failures=0;
    failures += check_phy(0,0,1,1,0);
    failures += check_phy(1,0,1,2,1);
    failures += check_phy(5,0,1,6,5);
    failures += check_phy(9,0,1,10,9);
    failures += check_phy(100,0,-1,10,9);
    failures += check_phy(100,1,0,1,0);
    for(unsigned int value=0; value<=UINT16_MAX; value++) {
        caps=(u16)value;
        if(probe_smap() != ((value & SPD_CAPS_SMAP) ? 0 : -1)) {
            fprintf(stderr,"Wrong capability decision for 0x%04x\n",value);
            failures++;
            break;
        }
    }
    return failures ? 1 : 0;
}
'''


def exercise(directory, name, reset_code, capability_code, expected):
    cfile = directory / (name + '.c')
    binary = directory / (name + '.exe')
    cfile.write_text(HARNESS.replace('@PHY@', reset_code)
                     .replace('@CAPABILITY@', capability_code), encoding='utf-8')
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + ['-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-variable', '-Wno-unused-but-set-variable',
                    '-Wno-parentheses', str(cfile), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
    if (result.returncode == 0) != expected:
        raise AssertionError(f'{name}: unexpected exit {result.returncode}\n{result.stderr}')
    print(f'PASS {name}: ' + ('startup gates correct' if expected else 'inherited defect reproduced'))


with tempfile.TemporaryDirectory() as temp:
    directory = Path(temp)
    exercise(directory, 'production', phy, capability, True)
    old_phy = phy.replace('for (i = 9;', 'for (i = 0;').replace('; i--) {', '; i++) {')
    assert old_phy != phy
    exercise(directory, 'old-reset-loop', old_phy, capability, False)
    exercise(directory, 'old-capability-check', phy,
             '!SPD_REG16(SPD_R_REV_3) & SPD_CAPS_SMAP', False)
