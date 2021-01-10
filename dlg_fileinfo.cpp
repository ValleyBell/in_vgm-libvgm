#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <string>
#include <vector>
#include <math.h>
#include <windows.h>

#ifdef _MSC_VER
#define snprintf	_snprintf
#endif

extern "C"
{
#include "Winamp/wa_ipc.h"
#include "Winamp/in2.h"

#include <stdtype.h>
#include <utils/StrUtils.h>

#include "resource.h"
}

#include "playcfg.hpp"
#include "FileInfoStorage.hpp"
#include "utils.hpp"
extern PluginConfig pluginCfg;


#define SETCHECKBOX(hWnd, DlgID, Check)	SendDlgItemMessage(hWnd, DlgID, BM_SETCHECK, Check, 0)


// Function Prototypes from in_vgm.c
void Config(HWND hWndParent);


// Function Prototypes
void InitFileInfoDialog(FileInfoStorage* fis);
void DeinitFileInfoDialog(void);
void FileInfoDlg_LoadFile(const char* fileName);
void FileInfoDlg_LoadFile(const wchar_t* fileName);
static void FileInfoDlg_LoadFile_Common(UINT8 loadResult, FileInfoStorage& fis);
int FileInfoDlg_Show(HINSTANCE hDllInst, HWND hWndParent);

static std::string GetChipUsageText(const FileInfoStorage::SongInfo& songInfo);
static std::string PrintTime(double seconds);
static std::string FormatVGMLength(const FileInfoStorage::SongInfo& songInfo);
void QueueInfoReload(void);

static void DisplayTagString(HWND hWndDlg, int dlgItem, const FileInfoStorage& fis,
							const std::string& tagKey, bool tagMultiLang, bool preferJap);
static BOOL CALLBACK FileInfoDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);


static const char* FileNameToLoad;
static const wchar_t* FileNameToLoadW;

static CPCONV* cpcU8_Wide;
static CPCONV* cpcU8_ACP;
static FileInfoStorage* fisFileInfo;
static bool needInfoRefresh = false;

void InitFileInfoDialog(FileInfoStorage* fis)
{
	cpcU8_Wide = NULL;
	CPConv_Init(&cpcU8_Wide, "UTF-8", "UTF-16LE");
	{
		std::string cpName(0x10, '\0');
		snprintf(&cpName[0], cpName.size(), "CP%u", GetACP());
		cpcU8_ACP = NULL;
		CPConv_Init(&cpcU8_ACP, "UTF-8", cpName.c_str());
	}
	
	fisFileInfo = fis;
	
	return;
}

void DeinitFileInfoDialog(void)
{
	CPConv_Deinit(cpcU8_Wide);	cpcU8_Wide = NULL;
	CPConv_Deinit(cpcU8_ACP);	cpcU8_ACP = NULL;
	fisFileInfo = NULL;
	
	return;
}

void FileInfoDlg_LoadFile(const char* fileName)
{
	FileNameToLoadW = NULL;
	FileNameToLoad = fileName;
	UINT8 retVal = fisFileInfo->LoadSong(FileNameToLoad, true);
	FileInfoDlg_LoadFile_Common(retVal, *fisFileInfo);
	
	return;
}

void FileInfoDlg_LoadFile(const wchar_t* fileName)
{
	FileNameToLoad = NULL;
	FileNameToLoadW = fileName;
	UINT8 retVal = fisFileInfo->LoadSong(FileNameToLoadW, true);
	FileInfoDlg_LoadFile_Common(retVal, *fisFileInfo);
	
	return;
}

static void FileInfoDlg_LoadFile_Common(UINT8 loadResult, FileInfoStorage& fis)
{
	if (loadResult & 0x80)
	{
		if (loadResult == 0xFF)
			fis._songInfo.tags["COMMENT"] = "File not found!";
		else if (loadResult == 0xFE)
			fis._songInfo.tags["COMMENT"] = "File invalid!";
		else
			fis._songInfo.tags["COMMENT"] = "Load error!";
		return;
	}
	
	if (! fis._songInfo.hasTags)
		fis._songInfo.tags["COMMENT"] = "No tags available.";
	
	needInfoRefresh = false;
	return;
}

int FileInfoDlg_Show(HINSTANCE hDllInst, HWND hWndParent)
{
	return DialogBoxA(hDllInst, (LPTSTR)DlgFileInfo, hWndParent, FileInfoDialogProc);
}


static std::string GetChipUsageText(const FileInfoStorage::SongInfo& songInfo)
{
	const std::vector<std::string>& chipList = songInfo.chipLstStr;
	std::string result;
	size_t curChip;
	
	for (curChip = 0; curChip < chipList.size(); curChip ++)
	{
		result += chipList[curChip];
		result += ", ";
	}
	if (result.size() >= 2)
		result = result.substr(0, result.size() - 2);
	
	return result;
}

static std::string PrintTime(double seconds)
{
	char buffer[0x20];
	UINT32 time;
	UINT16 CSecs;
	UINT16 Secs;
	UINT16 Mins;
	UINT16 Hours;
	UINT16 Days;
	
	time = (UINT32)(seconds * 100.0 + 0.5);
	CSecs = time % 100;
	time /= 100;
	Secs = time % 60;
	time /= 60;
	Mins = time % 60;
	time /= 60;
	Hours = time % 24;
	time /= 24;
	Days = time;
	
	// Maximum Output: 4 294 967 295 -> "49d + 17:02:47.30" (17 chars + \0)
	if (Days)
	{
		// just for fun
		//				 1d +  2 :  34 :  56 . 78
		sprintf(buffer, "%ud + %u:%02u:%02u.%02u", Days, Hours, Mins, Secs, CSecs);
	}
	else if (Hours)
	{
		// unlikely for single VGMs, but possible
		//				  1 :  23 :  45 . 67
		sprintf(buffer, "%u:%02u:%02u.%02u", Hours, Mins, Secs, CSecs);
	}
	else if (Mins)
	{
		//				  1 :  23 . 45
		sprintf(buffer, "%u:%02u.%02u", Mins, Secs, CSecs);
	}
	else
	{
		//				  1 . 23
		sprintf(buffer, "%u.%02u", Secs, CSecs);
	}
	
	return buffer;
}

static std::string FormatVGMLength(const FileInfoStorage::SongInfo& songInfo)
{
	std::string strTotal = PrintTime(songInfo.songLen1);
	if (! songInfo.looping)
		return strTotal + " (no loop)";
	
	double introLen = songInfo.songLen1 - songInfo.loopLen;
	if (introLen < 0.5)
	{
		// no intro (or only a very small intro for song init)
		return strTotal + " (looped)";
	}
	else
	{
		std::string strIntro = PrintTime(introLen);
		std::string strLoop = PrintTime(songInfo.loopLen);
		return strTotal + " (" + strIntro + " intro and " + strLoop + " loop)";
	}
}

void QueueInfoReload(void)
{
	needInfoRefresh = true;
	return;
}


static void DisplayTagString(HWND hWndDlg, int dlgItem, const FileInfoStorage& fis,
							const std::string& tagKey, bool tagMultiLang, bool preferJap)
{
	std::vector<std::string> langPostfixes;
	langPostfixes.push_back("");
	langPostfixes.push_back("-JPN");
	if (preferJap)
		langPostfixes[0].swap(langPostfixes[1]);
	
	if (! pluginCfg.genOpts.fidTagFallback)
		langPostfixes.resize(1);	// keep only first choice
	
	std::vector<std::string> tagKeys = CombineBaseWithList(tagKey, langPostfixes);
	
	const char* tagVal = fis.GetTag(tagKeys);
	if (tagVal == NULL)
	{
		SetDlgItemTextA(hWndDlg, dlgItem, "");
		return;
	}
	
	// The tag is stored as UTF-8, so we need to convert to either UTF-16 or ANSI.
	{
		// try Unicode version first
		size_t textWLen = 0;
		wchar_t* textWStr = NULL;
		UINT8 retVal = CPConv_StrConvert(cpcU8_Wide, &textWLen, reinterpret_cast<char**>(&textWStr), 0, tagVal);
		BOOL retB = FALSE;
		if (retVal < 0x80)
			retB = SetDlgItemTextW(hWndDlg, dlgItem, textWStr);
		free(textWStr);
		if (retB)
			return;
	}
	{
		// Unicode version failed, try ANSI version
		size_t textALen = 0;
		char* textAStr = NULL;
		UINT8 retVal = CPConv_StrConvert(cpcU8_ACP, &textALen, &textAStr, 0, tagVal);
		BOOL retB = FALSE;
		if (retVal < 0x80)
			retB = SetDlgItemTextA(hWndDlg, dlgItem, textAStr);
		free(textAStr);
		if (retB)
			return;
	}
	
	return;
}

static BOOL CALLBACK FileInfoDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
	{
		if (FileNameToLoadW != NULL)
			SetDlgItemTextW(hWndDlg, VGMFileText, FileNameToLoadW);
		else
			SetDlgItemTextA(hWndDlg, VGMFileText, FileNameToLoad);
		
		if (fisFileInfo->_songInfo.fileSize)
		{
			char tempStr[0x20];
			const FileInfoStorage::SongInfo& sInf = fisFileInfo->_songInfo;
			
			SetDlgItemTextA(hWndDlg, VGMVerText, sInf.verStr.c_str());
			
			snprintf(tempStr, 0x20, "%.2f", sInf.volGain);
			SetDlgItemTextA(hWndDlg, VGMGainText, tempStr);
			
			SetDlgItemTextA(hWndDlg, ChipUseText, GetChipUsageText(sInf).c_str());
			
			snprintf(tempStr, 0x20, "%d bytes (%.2f kbps)", sInf.fileSize, sInf.bitRate / 1000.0);
			SetDlgItemTextA(hWndDlg, VGMSizeText, tempStr);
			
			SetDlgItemTextA(hWndDlg, VGMLenText, FormatVGMLength(sInf).c_str());
		}
		else
		{
			SetDlgItemTextA(hWndDlg, VGMVerText, "");
			SetDlgItemTextA(hWndDlg, VGMGainText, "");
			SetDlgItemTextA(hWndDlg, ChipUseText, "");
			SetDlgItemTextA(hWndDlg, VGMSizeText, "");
			SetDlgItemTextA(hWndDlg, VGMLenText, "");
		}
		
		// display language independent tags
		bool preferJap = pluginCfg.genOpts.preferJapTag;
		DisplayTagString(hWndDlg, GameDateText, *fisFileInfo, "DATE", false, preferJap);
		DisplayTagString(hWndDlg, VGMCrtText, *fisFileInfo, "ENCODED_BY", false, preferJap);
		DisplayTagString(hWndDlg, VGMNotesText, *fisFileInfo, "COMMENT", false, preferJap);
		
		// trigger English or Japanese for other fields
		WPARAM langCheckbox = preferJap ? LangJapCheck : LangEngCheck;
		SETCHECKBOX(hWndDlg, langCheckbox, TRUE);
		PostMessage(hWndDlg, WM_COMMAND, langCheckbox, 0);
		
		return TRUE;
	}
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			//Options.JapTags = IsDlgButtonChecked(hWndDlg, LangJapCheck);
			//EndDialog(hWndDlg, 0);	// return 0 = OK
			EndDialog(hWndDlg, 1);	// returning 0 can cause bugs with the playlist
			return TRUE;
		case IDCANCEL:	// [X] button, ESC, Alt + F4
			EndDialog(hWndDlg, 1);	// return 1 = Cancel, stops further dialogues being opened
			return TRUE;
		case ConfigPluginButton:
			Config(hWndDlg);
			return TRUE;
		case LangEngCheck:
		case LangJapCheck:
		{
			bool langJap = (LOWORD(wParam) == LangJapCheck);
			DisplayTagString(hWndDlg, TrkTitleText, *fisFileInfo, "TITLE", true, langJap);
			DisplayTagString(hWndDlg, TrkAuthorText, *fisFileInfo, "ARTIST", true, langJap);
			DisplayTagString(hWndDlg, GameNameText, *fisFileInfo, "GAME", true, langJap);
			DisplayTagString(hWndDlg, GameSysText, *fisFileInfo, "SYSTEM", true, langJap);
			return TRUE;
		}
		case BrwsrInfoButton:
			//InfoInBrowser(FileInfo_Name, UseMB, TRUE);
			break;
		default:
			break;
		}
		break;
	}
	
	return FALSE;	// FALSE to signify message not processed
}
