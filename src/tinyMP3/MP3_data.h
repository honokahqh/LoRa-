#ifndef MP3_DATA_H
#define MP3_DATA_H

#include <stdint.h>

extern const uint8_t data_mp3[104832];

#define MP3_Size_32kbps                 144
#define MP3_Welcome_Index               0 * MP3_Size_32kbps
#define MP3_Welcome_Frame_Num           130
#define MP3_Call_True_Index             130 * MP3_Size_32kbps
#define MP3_Call_True_Frame_Num         140    
#define MP3_Call_False_Index            270 * MP3_Size_32kbps  
#define MP3_Call_False_Frame_Num        200
#define MP3_Cancel_Index                470 * MP3_Size_32kbps
#define MP3_Cancel_Frame_Num            40
#define MP3_Register_Index              510 * MP3_Size_32kbps
#define MP3_Register_Frame_Num          50
#define MP3_Leave_Index                 560 * MP3_Size_32kbps
#define MP3_Leave_Frame_Num             50
#define MP3_Init_Index                  610 * MP3_Size_32kbps
#define MP3_Init_Frame_Num              50
#define MP3_Low_Battery_Index           660 * MP3_Size_32kbps
#define MP3_Low_Battery_Frame_Num       68
#endif // !