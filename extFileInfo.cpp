#include <stdio.h>
#include <stddef.h>
#include <wchar.h>
#include <string>
#include <vector>
#include <windows.h>

#ifdef _MSC_VER
#define stricmp	_stricmp
#define wcsicmp	_wcsicmp
#define snprintf	_snprintf
#else
#define stricmp	_strcasecmp
#define wcsicmp	_wcscasecmp
#endif

extern "C"
{
#include <stdtype.h>
#include <utils/StrUtils.h>
}

#include "playcfg.hpp"
#include "FileInfoStorage.hpp"
#include "utils.hpp"

// from in_vgm.cpp
extern PluginConfig pluginCfg;
FileInfoStorage* GetMainPlayerFIS(void);
int GetPlrSongLengthMS(const FileInfoStorage::SongInfo& songInfo);


#define DLL_EXPORT	__declspec(dllexport)


void InitExtFileInfo(FileInfoStorage* scan, CPCONV* cpcU8w, CPCONV* cpcU8a);
void DeinitExtFileInfo(void);
static std::string GetYearFromDateStr(const std::string& date);
static bool GetExtendedFileInfoA(const std::string& fileName, const char* metaType, std::string& result);
static bool GetExtendedFileInfoW(const std::wstring& fileName, const char* metaType, std::string& result);
static bool GetExtendedInfoFromTags(FileInfoStorage* fis, const char* metaType, std::string& result);

extern "C"
{
DLL_EXPORT int winampGetExtendedFileInfoW(const wchar_t* wfilename, const char* metadata, wchar_t* ret, int retlen);
DLL_EXPORT int winampGetExtendedFileInfo(const char* filename, const char* metadata, char* ret, int retlen);
}


#define METATAG_AUTHOR	0x01
#define METATAG_LENGTH	0x02
#define METATAG_TITLE	0x03
#define METATAG_ALBUM	0x04
#define METATAG_COMMENT	0x05
#define METATAG_YEAR	0x06
#define METATAG_GENRE	0x07
#define METATAG_UNKNOWN	0xFF


static CPCONV* cpcU8_Wide;
static CPCONV* cpcU8_ACP;
static FileInfoStorage* fisFileInfo;

void InitExtFileInfo(FileInfoStorage* scan, CPCONV* cpcU8w, CPCONV* cpcU8a)
{
	cpcU8_Wide = cpcU8w;
	cpcU8_ACP = cpcU8a;
	fisFileInfo = scan;
	
	return;
}

void DeinitExtFileInfo(void)
{
	return;
}

static std::string GetYearFromDateStr(const std::string& date)
{
	// Note: Date formatting is almost unchanged from Maxim's in_vgm 0.35
	
	// try to detect various date formats
	// Not yyyy/mm/dd:
	//    nn/nn/yy    n/n/yy    nn/n/yy    n/nn/yy
	//    nn/nn/yyyy  n/n/yyyy  nn/n/yyyy  n/nn/yyyy
	// Should be:
	//    yyyy
	//    yyyy/mm
	//    yyyy/mm/dd
	size_t slashPos = date.rfind('/');
	if (slashPos != std::string::npos)
	{
		int year = (int)strtol(date.c_str() + slashPos + 1, NULL, 10);
		if (year > 31) // looks like a year
		{
			if (year < 100) // 2-digit, yuck
			{
				year += 1900;
				if (year < 1960)
					year += 100;	// not many sound chips around then, due to lack of transistors
			}
			char tempStr[0x10];
			snprintf(tempStr, 0x10, "%d", year);
			return tempStr;
		}
	}
	// else, try the first bit
	return date.substr(0, 4);
}

/* Metadata Tag List:
	"track", "title", "artist", "album", "year", "comment", "genre", "length",
	"type", "family", "formatinformation", "gain", "bitrate", "vbr", "stereo", ...
*/
static bool GetExtendedFileInfoA(const std::string& fileName, const char* metaType, std::string& result)
{
	// General Meta Tags
	if (! stricmp(metaType, "type"))
	{
		// Data Type (Audio/Video/...)
		char tempStr[0x10];
		snprintf(tempStr, 0x10, "%d", pluginCfg.genOpts.mediaLibFileType);
		result = tempStr;
		return true;
	}
	else if (! stricmp(metaType, "family"))	// Winamp 5.5+ only
	{
		const char* fileExt = GetFileExtension(fileName.c_str());
		if (fileExt == NULL)
			return false;
		
		if (! stricmp(fileExt, "vgm") || ! stricmp(fileExt, "vgz"))
		{
			result = "Video Game Music File";
			return true;
		}
		return false;
	}
	else if (! stricmp(metaType, "track"))
	{
		return false;
	}
	
	FileInfoStorage* fisPlaying = GetMainPlayerFIS();
	FileInfoStorage* fis;
	if (fileName == fisPlaying->_fileNameA)
	{
		fis = fisPlaying;
	}
	else
	{
		UINT8 retVal = fisFileInfo->LoadSong(fileName, pluginCfg.genOpts.noInfoCache);
		if (retVal & 0x80)
			return false;	// loading failed
		fis = fisFileInfo;
	}
	return GetExtendedInfoFromTags(fis, metaType, result);
}

static bool GetExtendedFileInfoW(const std::wstring& fileName, const char* metaType, std::string& result)
{
	// General Meta Tags
	if (! stricmp(metaType, "type") || ! stricmp(metaType, "track"))
	{
		return GetExtendedFileInfoA("", metaType, result);
	}
	else if (! stricmp(metaType, "family"))	// Winamp 5.5+ only
	{
		const wchar_t* fileExt = GetFileExtension(fileName.c_str());
		if (fileExt == NULL)
			return false;
		fileExt ++;
		
		if (! wcsicmp(fileExt, L"vgm") || ! wcsicmp(fileExt, L"vgz"))
		{
			result = "Video Game Music File";
			return true;
		}
		return false;
	}
	
	FileInfoStorage* fisPlaying = GetMainPlayerFIS();
	FileInfoStorage* fis;
	if (fileName == fisPlaying->_fileNameW)
	{
		fis = fisPlaying;
	}
	else
	{
		UINT8 retVal = fisFileInfo->LoadSong(fileName, pluginCfg.genOpts.noInfoCache);
		if (retVal & 0x80)
			return false;	// loading failed
		fis = fisFileInfo;
	}
	return GetExtendedInfoFromTags(fis, metaType, result);
}

static bool GetExtendedInfoFromTags(FileInfoStorage* fis, const char* metaType, std::string& result)
{
	int tagIdx;
	
	result = "";	// default to a blank string
	if (! stricmp(metaType, "artist"))
		tagIdx = METATAG_AUTHOR;
	else if (! stricmp(metaType, "length"))
		tagIdx = METATAG_LENGTH;
	else if (! stricmp(metaType, "title"))
		tagIdx = METATAG_TITLE;
	else if (! stricmp(metaType, "album"))
		tagIdx = METATAG_ALBUM;	// return Game Name
	else if (! stricmp(metaType, "comment"))
		tagIdx = METATAG_COMMENT;
	else if (! stricmp(metaType, "year") || ! stricmp(metaType, "date"))
		tagIdx = METATAG_YEAR;
	else if (! stricmp(metaType, "genre"))
		tagIdx = METATAG_GENRE;	// return System (?)
	else
	{
		tagIdx = METATAG_UNKNOWN;
#ifdef _DEBUG
		{
			char debugStr[0x80];
			sprintf(debugStr, "GetExtendedFileInfo: Unknown metadata type: \"%s\"\n", metaType);
			MessageBox(NULL, debugStr, "in_vgm Warning", MB_ICONEXCLAMATION);
		}
#endif
	}
	
	switch(tagIdx)
	{
	case METATAG_LENGTH:
	{
		char tempStr[0x10];
		snprintf(tempStr, 0x10, "%d", GetPlrSongLengthMS(fis->_songInfo));
		result = tempStr;	// length in milliseconds
		return true;
	}
	//case METATAG_GENRE:
	//	result = "Game";
	//	return true;
	case METATAG_YEAR:
	{
		const char* dateTag = fis->GetTag("DATE");
		if (dateTag == NULL || dateTag[0] == '\0')
			return false;
		result = GetYearFromDateStr(dateTag);
		return true;
	}
	case METATAG_TITLE:
	case METATAG_ALBUM:
	case METATAG_GENRE:
	case METATAG_AUTHOR:
	//case METATAG_RIPPER:
	case METATAG_COMMENT:
	{
		std::vector<std::string> langPostfixes;
		langPostfixes.push_back("");
		langPostfixes.push_back("-JPN");
		if (pluginCfg.genOpts.preferJapTag)
			langPostfixes[0].swap(langPostfixes[1]);
		
		std::vector<std::string> tagKeys;
		switch(tagIdx)
		{
		case METATAG_TITLE:
			tagKeys = CombineBaseWithList("TITLE", langPostfixes);
			break;
		case METATAG_ALBUM:
			tagKeys = CombineBaseWithList("GAME", langPostfixes);
			break;
		case METATAG_GENRE:
			tagKeys = CombineBaseWithList("SYSTEM", langPostfixes);
			break;
		case METATAG_AUTHOR:
			tagKeys = CombineBaseWithList("ARTIST", langPostfixes);
			break;
		case METATAG_COMMENT:
			tagKeys.push_back("COMMENT");
			break;
		default:
			return false;
		}
		
		const char* tagData = fis->GetTag(tagKeys);
		if (tagData == NULL)
			return false;
		result = tagData;
		
		if (tagIdx == METATAG_ALBUM && pluginCfg.genOpts.appendFM2413 && fis->_songInfo.usesYM2413)
			result += " (FM)";
		return true;
	}
	default:
		return false;
	}
}


// -----------------------
// Winamp Export Functions
// -----------------------
DLL_EXPORT int winampGetExtendedFileInfoW(const wchar_t* wfilename, const char* metadata,
										 wchar_t* ret, int retlen)
{
	if (ret == NULL || ! retlen)
		return 0;
	
	// called by Winamp 5.3 and higher
	std::string retStr;
	bool retVal = GetExtendedFileInfoW(wfilename, metadata, retStr);
	if (retVal)
	{
		size_t outLen = retlen;
		printf("winampGetExtendedFileInfoW(\"%ls\", %s) -> %s\n", wfilename, metadata, retStr.c_str());
		UINT8 retVal = CPConv_StrConvert(cpcU8_Wide, &outLen, reinterpret_cast<char**>(&ret),
			retStr.length() + 1, retStr.c_str());
		if (retVal > 0x00)
			ret[outLen / sizeof(wchar_t)] = L'\0';
	}
	
	return static_cast<int>(retVal);
}

DLL_EXPORT int winampGetExtendedFileInfo(const char* filename, const char* metadata,
										char* ret, int retlen)
{
	if (ret == NULL || ! retlen)
		return 0;
	
	// called by Winamp versions until 5.24
	std::string retStr;
	bool retVal = GetExtendedFileInfoA(filename, metadata, retStr);
	if (retVal)
	{
		size_t outLen = retlen;
		printf("winampGetExtendedFileInfoA(\"%s\", %s) -> %s\n", filename, metadata, retStr.c_str());
		UINT8 retVal = CPConv_StrConvert(cpcU8_ACP, &outLen, &ret,
			retStr.length() + 1, retStr.c_str());
		if (retVal > 0x00)
			ret[outLen] = '\0';
	}
	
#if 0
	{
		char MsgStr[MAX_PATH * 2];
		sprintf(MsgStr, "file: %s\nMetadata: %s\nResult: %s", filename, metadata, ret);
		MessageBoxA(NULL, MsgStr, "GetExtFileInfoA", MB_ICONINFORMATION | MB_OK);
	}
#endif
	
	return static_cast<int>(retVal);
}
