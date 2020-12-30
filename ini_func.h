#ifndef __INI_FUNC_H__
#define __INI_FUNC_H__

#include <stdtype.h>

void Ini_SetFilePath(const char* filePath);

void ReadIni_Integer(const char* Section, const char* Key, UINT32* Value);
void ReadIni_SIntSht(const char* Section, const char* Key, INT16* Value);
void ReadIni_IntByte(const char* Section, const char* Key, UINT8* Value);
void ReadIni_Boolean(const char* Section, const char* Key, bool* Value);
void ReadIni_String(const char* Section, const char* Key, char* String, UINT32 StrSize);
void ReadIni_Float(const char* Section, const char* Key, float* Value);

void WriteIni_Integer(const char* Section, const char* Key, UINT32 Value);
void WriteIni_SInteger(const char* Section, const char* Key, INT32 Value);
void WriteIni_XInteger(const char* Section, const char* Key, UINT32 Value);
void WriteIni_Boolean(const char* Section, const char* Key, bool Value);
void WriteIni_String(const char* Section, const char* Key, const char* String);
void WriteIni_Float(const char* Section, const char* Key, float Value);

#endif	// __INI_FUNC_H__
