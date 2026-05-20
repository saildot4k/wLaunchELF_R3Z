#ifndef PSU_TYPES_H
#define PSU_TYPES_H

#include <tamtypes.h>

typedef struct
{
	u8 unused;
	u8 sec;
	u8 min;
	u8 hour;
	u8 day;
	u8 month;
	u16 year;
} ps2time;

typedef struct
{                       //Offs:  Example content
	ps2time cTime;      //0x00:  8 bytes creation timestamp (struct above)
	ps2time mTime;      //0x08:  8 bytes modification timestamp (struct above)
	u32 size;           //0x10:  file size
	u16 attr;           //0x14:  0x8427  (=normal folder, 8497 for normal file)
	u16 unknown_1_u16;  //0x16:  2 zero bytes
	u64 unknown_2_u64;  //0x18:  8 zero bytes
	u8 name[32];        //0x20:  32 name bytes, padded with zeroes
} mcT_header __attribute__((aligned(64)));

typedef struct
{                                   //Offs:  Example content
	u16 attr;                       //0x00:  0x8427  (=normal folder, 8497 for normal file)
	u16 unknown_1_u16;              //0x02:  2 zero bytes
	u32 size;                       //0x04:  header_count-1, file size, 0 for pseudo
	ps2time cTime;                  //0x08:  8 bytes creation timestamp (struct above)
	u64 EMS_used_u64;               //0x10:  8 zero bytes (but used by EMS)
	ps2time mTime;                  //0x18:  8 bytes modification timestamp (struct above)
	u64 unknown_2_u64;              //0x20:  8 bytes from mcTable
	u8 unknown_3_24_bytes[24];      //0x28:  24 zero bytes
	u8 name[32];                    //0x40:  32 name bytes, padded with zeroes
	u8 unknown_4_416_bytes[0x1A0];  //0x60:  zero byte padding to reach 0x200 size
} psu_header;                       //0x200: End of psu_header struct

#endif
