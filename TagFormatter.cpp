#include <string>

#include <stdtype.h>
#include "FileInfoStorage.hpp"
#include "utils.hpp"
#include "TagFormatter.hpp"


static void FixSeparators(wchar_t** TextData);

std::string FormatVGMTag(const char* format, const FileInfoStorage& fis)
{
	std::string result;
	const char* fmtPtr;
	std::vector<std::string> langPostfixes;
	
	langPostfixes.push_back("");
	langPostfixes.push_back("-JPN");
	
	for (fmtPtr = format; *fmtPtr != '\0'; fmtPtr ++)
	{
		if (*fmtPtr == '%')
		{
			fmtPtr ++;
			
			char varType = (char)toupper((unsigned char)*fmtPtr);
			std::vector<std::string> tagKeys;
			bool AppendFM = false;
			switch(varType)
			{
			case 'T':	// Track Title
				tagKeys = CombineBaseWithList("TITLE", langPostfixes);
				break;
			case 'A':	// Author
				tagKeys = CombineBaseWithList("ARTIST", langPostfixes);
				break;
			case 'G':	// Game Name
				tagKeys = CombineBaseWithList("GAME", langPostfixes);
				break;
			case 'D':	// Release Date
				tagKeys.push_back("DATE");
				break;
			case 'S':	// System Name
				tagKeys = CombineBaseWithList("SYSTEM", langPostfixes);
				break;
			case 'C':	// File Creator
				tagKeys.push_back("ENCODED_BY");
				break;
			default:
				result.push_back(*fmtPtr);
				continue;	// continue for-loop
			}
			
			if (tagKeys.size() > 1)
			{
				char isJap = (char)toupper((unsigned char)fmtPtr[1]);
				if (isJap == 'J')
				{
					fmtPtr ++;
					tagKeys[0].swap(tagKeys[1]);	// prefer Japanese tags
				}
			}
			
			const char* tagData = fis.GetTag(tagKeys);
			if (tagData != NULL)
				result += tagData;
			if (varType == 'G' && fis._songInfo.usesYM2413)
				result += " (FM)";
		}
		else
		{
			result.push_back(*fmtPtr);
		}
	}
	
	return result;
}

std::string TrimWhitespaces(const std::string& text)
{
	size_t startPos;
	size_t endPos;
	
	for (startPos = 0; startPos < text.length(); startPos++)
	{
		if (! isspace((unsigned char)text[startPos]))
			break;
	}
	for (endPos = text.length(); endPos > startPos; endPos --)
	{
		if (! isspace((unsigned char)text[endPos - 1]))
			break;
	}
	return text.substr(startPos, endPos - startPos);
}

std::string EnforceCRLF(const std::string& text)
{
	std::string result;
	size_t srcPos;
	char lastChr = '\0';
	
	result.reserve(text.size());
	for (srcPos = 0; srcPos < text.length(); srcPos++)
	{
		if (text[srcPos] == '\n' && lastChr != '\r')
			result.push_back('\r');
		lastChr = text[srcPos];
		result.push_back(lastChr);
	}
	
	return result;
}

#define GOODSEP_CHR		L','

static void FixSeparators(wchar_t** TextData)
{
	// Note: Reallocates memory for final string
	static const wchar_t BADSEPS[] =
	{	L';', L'/', L'\\', L'&', L'\xFF0C', L'\xFF0F', L'\xFF3C', 0x0000};
	
	wchar_t* TempStr;
	wchar_t* SrcStr;
	wchar_t* DstStr;
	const wchar_t* ChkStr;
	UINT32 StrSize;
	UINT32 SpcWrt;
	bool WroteSep;
	
	if (TextData == NULL || *TextData == NULL || ! wcslen(*TextData))
		return;
	
	// fix Author-Tag (a;b;c -> a, b, c)
	SrcStr = *TextData;
	StrSize = 0x00;
	WroteSep = false;
	while(*SrcStr != L'\0')
	{
		if (WroteSep && ! (*(SrcStr + 1) == L' ' || *(SrcStr + 1) == 0x3000))
			StrSize ++;	// need additional Space character
		
		ChkStr = BADSEPS;
		while(*ChkStr != 0x0000)
		{
			if (*SrcStr == *ChkStr)
			{
				// replace bad with good chars
				*SrcStr = GOODSEP_CHR;
				break;
			}
			ChkStr ++;
		}
		if (*SrcStr == GOODSEP_CHR)
			WroteSep = true;
		SrcStr ++;
		StrSize ++;
	}
	StrSize ++;	// final \0 character
	TempStr = (wchar_t*)malloc(StrSize * sizeof(wchar_t));
	
	SrcStr = *TextData;
	DstStr = TempStr;
	SpcWrt = 0;
	WroteSep = false;
	while(*SrcStr != L'\0')
	{
		if (*SrcStr == GOODSEP_CHR)
		{
			WroteSep = true;
			// trim spaces left of the seperator
			DstStr -= SpcWrt;
			SpcWrt = 0;
		}
		else
		{
			if (*SrcStr == L' ' || *SrcStr == 0x3000)
				SpcWrt ++;
			else
				SpcWrt = 0;
			if (WroteSep && ! SpcWrt)
			{
				// insert space after the seperator
				*DstStr = L' ';
				DstStr ++;
				SpcWrt ++;
			}
			WroteSep = false;
		}
		*DstStr = *SrcStr;
		SrcStr ++;
		DstStr ++;
	}
	*DstStr = L'\0';
	free(*TextData);
	*TextData = TempStr;
	
	return;
}
