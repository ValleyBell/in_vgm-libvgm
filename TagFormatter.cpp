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

// fix Author-Tag (a;b;c -> a, b, c)
std::string FixSeparators(const std::string& text)
{
	static const char BADSEPS[] = {';', '/', '\\', '&', '\0'};
	//static const wchar_t BADSEPS[] = {L';', L'/', L'\\', L'&', L'\xFF0C', L'\xFF0F', L'\xFF3C', L'\0'};
	#define GOODSEP_CHR		','
	
	size_t spcWrt;
	bool wroteSep;
	size_t srcPos;
	std::string result;
	
	result.reserve(text.size());
	spcWrt = 0;
	for (srcPos = 0; srcPos < text.length(); srcPos++)
	{
		char srcChr = text[srcPos];
		const char* chkChr;
		for (chkChr = BADSEPS; *chkChr != '\0'; chkChr++)
		{
			if (srcChr == *chkChr)
			{
				srcChr = *chkChr;
				break;
			}
		}
		if (srcChr == GOODSEP_CHR)
		{
			// trim spaces left of the seperator
			result.resize(result.size() - spcWrt);
			spcWrt = 0;
			result.push_back(srcChr);
			wroteSep = true;
		}
		else
		{
			if (srcChr == ' ')
				spcWrt ++;
			else
				spcWrt = 0;
			if (wroteSep && ! spcWrt)
			{
				// insert space after the seperator
				result.push_back(' ');
				spcWrt ++;
			}
			result.push_back(srcChr);
			wroteSep = false;
		}
	}
	
	return result;
}
