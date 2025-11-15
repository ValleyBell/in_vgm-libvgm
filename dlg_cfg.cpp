/*3456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456
0000000001111111111222222222233333333334444444444555555555566666666667777777777888888888899999*/
#define _USE_MATH_DEFINES
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include <stdtype.h>

#include <emu/SoundDevs.h>
#include <emu/EmuCores.h>
#include <emu/EmuStructs.h>
#include <emu/SoundEmu.h>	// for SndEmu_GetDevName()

#include "resource.h"
#include "playcfg.hpp"
#include "FileInfoStorage.hpp"
#include "in_vgm.h"
extern PluginConfig pluginCfg;

#ifndef M_LN2
#define M_LN2	0.693147180559945309417
#endif

#define HIDE_VGM7Z

#define TAB_ICON_WIDTH	16
#define TAB_ICON_TRANSP	0xFF00FF


// Dialogue box tabsheet handler
#define NUM_CFG_TABS	5
#define CfgPlayback		hWndCfgTab[0]
#define CfgTags			hWndCfgTab[1]
#define Cfg7z			hWndCfgTab[2]
#define CfgMuting		hWndCfgTab[3]
#define CfgOptPan		hWndCfgTab[4]

#define SLIDER_GETPOS(hWnd, dlgID)	(UINT16) \
										SendDlgItemMessage(hWnd, dlgID, TBM_GETPOS, 0, 0)
#define SLIDER_SETPOS(hWnd, dlgID, Pos)	SendDlgItemMessage(hWnd, dlgID, TBM_SETPOS, TRUE, Pos)
#define CTRL_DISABLE(hWnd, dlgID)		EnableWindow(GetDlgItem(hWnd, dlgID), FALSE)
#define CTRL_ENABLE(hWnd, dlgID)		EnableWindow(GetDlgItem(hWnd, dlgID), TRUE)
#define CTRL_HIDE(hWnd, dlgID)			ShowWindow(GetDlgItem(hWnd, dlgID), SW_HIDE)
#define CTRL_SHOW(hWnd, dlgID)			ShowWindow(GetDlgItem(hWnd, dlgID), SW_SHOW)
#define CTRL_SET_ENABLE(hWnd, dlgID, Enable)	EnableWindow(GetDlgItem(hWnd, dlgID), Enable)
#define CTRL_IS_ENABLED(hWnd, dlgID, Enable)	IsWindowEnabled(GetDlgItem(hWnd, dlgID))
#define SETCHECKBOX(hWnd, dlgID, Check)	SendDlgItemMessage(hWnd, dlgID, BM_SETCHECK, Check, 0)
#define CREATE_CHILD(dlgID, DlgProc)	\
								CreateDialog(hPluginInst, (LPCTSTR)dlgID, hWndMain, DlgProc)
#define COMBO_ADDSTR(hWnd, dlgID, String)	\
								SendDlgItemMessageA(hWnd, dlgID, CB_ADDSTRING, 0, (LPARAM)String)
#define COMBO_GETPOS(hWnd, dlgID)		SendDlgItemMessage(hWnd, dlgID, CB_GETCURSEL, 0, 0)
#define COMBO_SETPOS(hWnd, dlgID, Pos)	SendDlgItemMessage(hWnd, dlgID, CB_SETCURSEL, Pos, 0)
#define CHECK2BOOL(hWnd, dlgID)			(IsDlgButtonChecked(hWnd, dlgID) == BST_CHECKED)


typedef HRESULT (__stdcall *ETDT_FUNC)(HWND hwnd, DWORD dwFlags);	// EnableThemeDialogTexture
typedef BOOL (__stdcall *ITDTE_FUNC)(HWND hwnd);	// IsThemeDialogTextureEnabled


// Function Prototypes from in_vgm.c
const char* GetIniFilePath(void);
FileInfoStorage* GetMainPlayerFIS(void);
FileInfoStorage* GetMainPlayerFISScan(void);
void RefreshPlaybackOptions(void);
void RefreshMuting(void);
void RefreshPanning(void);
void UpdatePlayback(void);


// Function Prototypes from dlg_fileinfo.c
void QueueInfoReload(void);


// Function Prototypes
int ConfigDlg_Show(HINSTANCE hDllInst, HWND hWndParent);
static void InitConfigDialog(HWND hWndMain);
static inline void AddTab(HWND tabCtrlWnd, int ImgIndex, char* TabTitle);
static void EnableWinXPVisualStyles(HWND hWndMain);
static void Slider_Setup(HWND hWndDlg, int dlgID, int minVal, int maxVal, int largeStep, int tickFreq);
static BOOL SetDlgItemFloat(HWND hDlg, int nIDDlgItem, double value, int precision);
static int LoadConfigDialogInfo(HWND hWndDlg);

static BOOL CALLBACK ConfigDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK CfgDlgPlaybackProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK CfgDlgTagsProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK CfgDlgMutingProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK CfgDlgOptPanProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK CfgDlgChildProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);

static inline bool IsSongPlaying(void);
static bool IsChipAvailable(const ChipOptions& cOpts);
static UINT32 GetChipCore(const ChipOptions& cOpts);
static void ShowMutingCheckBoxes(UINT8 ChipID, UINT8 ChipSet);
static void SetMutingData(UINT32 CheckBox, bool MuteOn);
static inline UINT16 PanPos2Slider(INT16 panPos);
static inline INT16 PanSlider2Pos(UINT16 sliderValue);
static void ShowOptPanBoxes(UINT8 ChipID, UINT8 ChipSet);
static void SetPanningData(UINT32 sliderID, UINT16 value, bool noRefresh);
void Dialogue_TrackChange(void);


static HINSTANCE hPluginInst;

static HWND hWndCfgTab[NUM_CFG_TABS];
static bool LoopTimeMode;
static UINT8 muteChipID = 0x00;
static UINT8 muteChipSet = 0;
static ChipOptions* muteCOpts;

int ConfigDlg_Show(HINSTANCE hDllInst, HWND hWndParent)
{
	hPluginInst = hDllInst;
	return DialogBox(hDllInst, (LPTSTR)DlgConfigMain, hWndParent, ConfigDialogProc);
}

static void InitConfigDialog(HWND hWndMain)
{
	HWND TabCtrlWnd;
	RECT TabDispRect;
	RECT TabRect;
	HIMAGELIST hImgList;
	unsigned int CurTab;
	
	TabCtrlWnd = GetDlgItem(hWndMain, TabCollection);
	InitCommonControls();
	
	// Load images for tabs
	hImgList = ImageList_LoadImage(hPluginInst, (LPCSTR)TabIcons, TAB_ICON_WIDTH, 0,
									TAB_ICON_TRANSP, IMAGE_BITMAP, LR_CREATEDIBSECTION);
	TabCtrl_SetImageList(TabCtrlWnd, hImgList);
	
	// Add tabs
	AddTab(TabCtrlWnd, 0, "Playback");
	AddTab(TabCtrlWnd, 1, "Tags");
#ifndef HIDE_VGM7Z
	AddTab(TabCtrlWnd, 2, "VGM7z");
#endif
	AddTab(TabCtrlWnd, 3, "Muting");
	AddTab(TabCtrlWnd, 5, "Other Opts");
	
	// Get display rect
	GetWindowRect(TabCtrlWnd, &TabDispRect);
	GetWindowRect(hWndMain, &TabRect);
	OffsetRect(&TabDispRect,
		-TabRect.left - GetSystemMetrics(SM_CXDLGFRAME),
		-TabRect.top - GetSystemMetrics(SM_CYDLGFRAME) - GetSystemMetrics(SM_CYCAPTION));
	TabCtrl_AdjustRect(TabCtrlWnd, FALSE, &TabDispRect);
	
	// Create child windows
	CfgPlayback	= CREATE_CHILD(DlgCfgPlayback,	CfgDlgPlaybackProc);
	CfgTags		= CREATE_CHILD(DlgCfgTags,		CfgDlgTagsProc);
	Cfg7z		= CREATE_CHILD(DlgCfgVgm7z,		CfgDlgChildProc);
	CfgMuting	= CREATE_CHILD(DlgCfgMuting,	CfgDlgMutingProc);
	CfgOptPan	= CREATE_CHILD(DlgCfgOptPan,	CfgDlgOptPanProc);
	
	EnableWinXPVisualStyles(hWndMain);
	
	// position tabs
	TabDispRect.right -= TabDispRect.left;	// .right gets Tab Width
	TabDispRect.bottom -= TabDispRect.top;	// .bottom gets Tab Height
	for (CurTab = 0; CurTab < NUM_CFG_TABS; CurTab ++)
	{
		SetWindowPos(hWndCfgTab[CurTab], HWND_TOP, TabDispRect.left, TabDispRect.top,
					TabDispRect.right, TabDispRect.bottom, SWP_NOZORDER);
	}
	
	// show first tab, hide the others
	ShowWindow(hWndCfgTab[0], SW_SHOW);
	for (CurTab = 1; CurTab < NUM_CFG_TABS; CurTab ++)
		ShowWindow(hWndCfgTab[CurTab], SW_HIDE);
	
	return;
}

static inline void AddTab(HWND tabCtrlWnd, int ImgIndex, char* TabTitle)
{
	TC_ITEM newTab;
	int tabIndex;
	
	tabIndex = TabCtrl_GetItemCount(tabCtrlWnd);
	newTab.mask = TCIF_TEXT;
	newTab.mask |= (ImgIndex >= 0) ? TCIF_IMAGE : 0;
	newTab.pszText = TabTitle;
	newTab.iImage = ImgIndex;
	TabCtrl_InsertItem(tabCtrlWnd, tabIndex, &newTab);
	
	return;
}

static void EnableWinXPVisualStyles(HWND hWndMain)
{
	HMODULE uxtDLL = LoadLibrary("uxtheme.dll");
	if (uxtDLL == NULL)
		return;
	
	ETDT_FUNC EnThemeDlgTex = (ETDT_FUNC)GetProcAddress(uxtDLL, "EnableThemeDialogTexture");
	ITDTE_FUNC ThemeDlgTexIsEn = (ITDTE_FUNC)GetProcAddress(uxtDLL, "IsThemeDialogTextureEnabled");
	if (ThemeDlgTexIsEn == NULL || EnThemeDlgTex == NULL)
	{
		FreeLibrary(uxtDLL);
		return;
	}
	
	if (ThemeDlgTexIsEn(hWndMain))
	{
		unsigned int curTab;
#ifndef ETDT_ENABLETAB
#define ETDT_ENABLETAB	6
#endif
		
		// draw pages with theme texture
		for (curTab = 0; curTab < NUM_CFG_TABS; curTab ++)
			EnThemeDlgTex(hWndCfgTab[curTab], ETDT_ENABLETAB);
	}
	
	FreeLibrary(uxtDLL);
	return;
}


static void Slider_Setup(HWND hWndDlg, int dlgID, int minVal, int maxVal, int largeStep, int tickFreq)
{
	LONG retL;
	
	retL = SendDlgItemMessage(hWndDlg, dlgID, TBM_SETRANGE, 0, MAKELONG(minVal, maxVal));
	// Note to TBM_SETTICFREQ:
	//  Needs Automatic Ticks enabled, draw a tick mark every x ticks.
	retL = SendDlgItemMessage(hWndDlg, dlgID, TBM_SETTICFREQ, tickFreq, 0);
	retL = SendDlgItemMessage(hWndDlg, dlgID, TBM_SETLINESIZE, 0, 1);
	retL = SendDlgItemMessage(hWndDlg, dlgID, TBM_SETPAGESIZE, 0, largeStep);
	
	return;
}

static BOOL SetDlgItemFloat(HWND hDlg, int nIDDlgItem, double value, int precision)
{
	char TempStr[0x10];
	
	sprintf(TempStr, "%.*f", precision, value);
	return SetDlgItemTextA(hDlg, nIDDlgItem, TempStr);
}

static int LoadConfigDialogInfo(HWND hWndDlg)
{
	const GeneralOptions& gOpts = pluginCfg.genOpts;
	char tempStr[0x18];
	
	// --- Main Dialog ---
	CheckDlgButton(hWndDlg, ImmediateUpdCheck,
					gOpts.immediateUpdate ? BST_CHECKED : BST_UNCHECKED);
	
	// --- Playback Tab ---
	COMBO_ADDSTR(CfgPlayback, ResmpModeList, "linear interpolation");
	COMBO_ADDSTR(CfgPlayback, ResmpModeList, "nearest-neighbour");
	COMBO_ADDSTR(CfgPlayback, ResmpModeList, "lin. up, NN down");
	COMBO_ADDSTR(CfgPlayback, ChipSmpModeList, "native");
	COMBO_ADDSTR(CfgPlayback, ChipSmpModeList, "highest (nat./cust.)");
	COMBO_ADDSTR(CfgPlayback, ChipSmpModeList, "custom");
	COMBO_ADDSTR(CfgPlayback, ChipSmpModeList, "highest, FM native");
	
	LoopTimeMode = false;
	SetDlgItemInt(CfgPlayback, LoopText, gOpts.maxLoops, FALSE);
	SetDlgItemInt(CfgPlayback, FadeText, gOpts.fadeTime, FALSE);
	SetDlgItemInt(CfgPlayback, PauseNlText, gOpts.pauseTime_jingle, FALSE);
	SetDlgItemInt(CfgPlayback, PauseLpText, gOpts.pauseTime_loop, FALSE);
	
	// Playback rate
	switch(gOpts.pbRate)
	{
	case 0:
		CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, RateRecRadio);
		break;
	case 60:
		CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, Rate60HzRadio);
		break;
	case 50:
		CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, Rate50HzRadio);
		break;
	default:
		CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, RateOtherRadio);
		break;
	}
	CTRL_SET_ENABLE(CfgPlayback, RateText, CHECK2BOOL(CfgPlayback, RateOtherRadio));
	CTRL_SET_ENABLE(CfgPlayback, RateLabel, CHECK2BOOL(CfgPlayback, RateOtherRadio));
	SetDlgItemInt(CfgPlayback, RateText, gOpts.pbRate, FALSE);
	
	{
		double dbVol = 6.0 * log(gOpts.volume) / M_LN2;
		int sliderPos = (int)floor(dbVol * 10 + 0.5) + 120;
		if (sliderPos < 0)
			sliderPos = 0;
		else if (sliderPos > 240)
			sliderPos = 240;
		// Slider from -12.0 db to +12.0 db, 0.1 db Steps (large Steps 2.0 db)
		Slider_Setup(CfgPlayback, VolumeSlider, 0, 240, 20, 10);
		SLIDER_SETPOS(CfgPlayback, VolumeSlider, sliderPos);
		
		sprintf(tempStr, "%.3f (%+.1f db)", gOpts.volume, dbVol);
		SetDlgItemTextA(CfgPlayback, VolumeText, tempStr);
	}
	
	SetDlgItemInt(CfgPlayback, SmplPbRateText, gOpts.smplRate, FALSE);
	COMBO_SETPOS(CfgPlayback, ResmpModeList, gOpts.resmplMode);
	SetDlgItemInt(CfgPlayback, SmplChipRateText, gOpts.chipSmplRate, FALSE);
	COMBO_SETPOS(CfgPlayback, ChipSmpModeList, gOpts.chipSmplMode);
	
	CheckDlgButton(CfgPlayback, SurroundCheck, gOpts.pseudoSurround ? BST_CHECKED : BST_UNCHECKED);
	
	/*// Filter
	switch(filter_type)
	{
	case FILTER_NONE:
		CheckRadioButton(CfgPlayback, rbFilterNone, rbFilterWeighted, rbFilterNone);
		break;
	case FILTER_LOWPASS:
		CheckRadioButton(CfgPlayback, rbFilterNone, rbFilterWeighted, rbFilterLowPass);
		break;
	case FILTER_WEIGHTED:
		CheckRadioButton(CfgPlayback, rbFilterNone, rbFilterWeighted, rbFilterWeighted);
		break;
	}*/
	
	// --- Tags Tab ---
	SetDlgItemTextA(CfgTags, TitleFormatText, gOpts.titleFormat.c_str());
	
	CheckDlgButton(CfgTags, PreferJapCheck, gOpts.preferJapTag);
	CheckDlgButton(CfgTags, FM2413Check, gOpts.appendFM2413);
	CheckDlgButton(CfgTags, TrimWhitespcCheck, gOpts.trimWhitespace);
	CheckDlgButton(CfgTags, StdSeparatorsCheck, gOpts.stdSeparators);
	CheckDlgButton(CfgTags, TagFallbackCheck, gOpts.fidTagFallback);
	CheckDlgButton(CfgTags, NoInfoCacheCheck, gOpts.noInfoCache);
	//CheckDlgButton(CfgTags, cbTagsStandardiseDates, TagsStandardiseDates);
	
	SetDlgItemInt(CfgTags, MLTypeText, gOpts.mediaLibFileType, FALSE);
	
	// --- Muting Tab and Options & Pan Tab ---
	size_t curChp;
	for (curChp = 0; curChp < pluginCfg.chipOpts.size(); curChp ++)
	{
		const ChipOptions& cOpts = pluginCfg.chipOpts[curChp][0];
		const char* devName = SndEmu_GetDevName(cOpts.chipType, 0x00, NULL);
		if (devName == NULL)
			devName = cOpts.cfgEntryName;
		COMBO_ADDSTR(CfgMuting, MutingChipList, devName);
		COMBO_ADDSTR(CfgOptPan, EmuOptChipList, devName);
	}
	COMBO_ADDSTR(CfgMuting, MutingChipNumList, "Chip #1");
	COMBO_ADDSTR(CfgOptPan, EmuOptChipNumList, "Chip #1");
	COMBO_ADDSTR(CfgMuting, MutingChipNumList, "Chip #2");
	COMBO_ADDSTR(CfgOptPan, EmuOptChipNumList, "Chip #2");
	
	COMBO_SETPOS(CfgMuting, MutingChipList, muteChipID);
	COMBO_SETPOS(CfgOptPan, EmuOptChipList, muteChipID);
	COMBO_SETPOS(CfgMuting, MutingChipNumList, muteChipSet);
	COMBO_SETPOS(CfgOptPan, EmuOptChipNumList, muteChipSet);
	
	CheckDlgButton(CfgMuting, ResetMuteCheck, gOpts.resetMuting);
	
	int panSld;
	for (panSld = PanChn1Slider; panSld <= PanChn15Slider; panSld ++)
		Slider_Setup(CfgOptPan, panSld, 0x00, 0x40, 0x08, 0x08);
	
	return 0;
}


// Dialogue box callback function
static BOOL CALLBACK ConfigDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
		// do nothing if this is a child window (tab page) callback, pass to the parent
		if (GetWindowLong(hWndDlg, GWL_STYLE) & WS_CHILD)
		{
			assert(false);
			return FALSE;
		}
		
		InitConfigDialog(hWndDlg);
		
		SetWindowTextA(hWndDlg, INVGM_TITLE " configuration");
		LoadConfigDialogInfo(hWndDlg);
		
		SendMessage(hWndDlg, WM_HSCROLL, 0, 0); // trigger slider value change handlers
		SendMessage(CfgPlayback, WM_COMMAND, MAKEWPARAM(ResmpModeList, CBN_SELCHANGE), 0);
		SendMessage(CfgPlayback, WM_COMMAND, MAKEWPARAM(ChipSmpModeList, CBN_SELCHANGE), 0);
		SendMessage(CfgMuting, WM_COMMAND, MAKEWPARAM(MutingChipList, CBN_SELCHANGE), 0);
		SendMessage(CfgOptPan, WM_COMMAND, MAKEWPARAM(EmuOptChipList, CBN_SELCHANGE), 0);
		
		SetFocus(hWndDlg);
		
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			{
				UINT8 CurTab;
				for (CurTab = 0; CurTab < NUM_CFG_TABS; CurTab ++)
					SendMessage(hWndCfgTab[CurTab], WM_COMMAND, IDOK, lParam);
			}
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:	// [X] button, Alt+F4, etc
			{
				UINT8 CurTab;
				for (CurTab = 0; CurTab < NUM_CFG_TABS; CurTab ++)
					SendMessage(hWndCfgTab[CurTab], WM_COMMAND, IDCANCEL, lParam);
			}
			
			EndDialog(hWndDlg, 1);
			return TRUE;
		case ImmediateUpdCheck:
			if (HIWORD(wParam) == BN_CLICKED)
				pluginCfg.genOpts.immediateUpdate = CHECK2BOOL(hWndDlg, ImmediateUpdCheck);
			break;
		}
		break; // end WM_COMMAND handling
	case WM_NOTIFY:
		switch(LOWORD(wParam))
		{
		case TabCollection:
		{
			int CurTab = TabCtrl_GetCurSel(GetDlgItem(hWndDlg, TabCollection));
			if (CurTab == -1)
				break;
#ifdef HIDE_VGM7Z
			if (CurTab >= 2)
				CurTab ++;	// skip hidden VGM7Z tab
#endif
			
			switch(((LPNMHDR)lParam)->code)
			{
			case TCN_SELCHANGING:	// hide current window
				ShowWindow(hWndCfgTab[CurTab], SW_HIDE);
				EnableWindow(hWndCfgTab[CurTab], FALSE);
				break;
			case TCN_SELCHANGE:		// show current window
				if (hWndCfgTab[CurTab] == CfgMuting)
				{
					COMBO_SETPOS(CfgMuting, MutingChipList, muteChipID);
					COMBO_SETPOS(CfgMuting, MutingChipNumList, muteChipSet);
					ShowMutingCheckBoxes(muteChipID, muteChipSet);
				}
				else if (hWndCfgTab[CurTab] == CfgOptPan)
				{
					COMBO_SETPOS(CfgOptPan, EmuOptChipList, muteChipID);
					COMBO_SETPOS(CfgOptPan, EmuOptChipNumList, muteChipSet);
					ShowOptPanBoxes(muteChipID, muteChipSet);
				}
				EnableWindow(hWndCfgTab[CurTab], TRUE);
				ShowWindow(hWndCfgTab[CurTab], SW_SHOW);
				break;
			}
			break;
		}
		}
		return TRUE;
	}
	
	return FALSE;	// message not processed
}

static BOOL CALLBACK CfgDlgPlaybackProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	GeneralOptions& gOpts = pluginCfg.genOpts;
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			gOpts.maxLoops = GetDlgItemInt(CfgPlayback, LoopText, NULL, FALSE);
			gOpts.fadeTime = GetDlgItemInt(CfgPlayback, FadeText, NULL, FALSE);
			gOpts.pauseTime_jingle = GetDlgItemInt(CfgPlayback, PauseNlText, NULL, FALSE);
			gOpts.pauseTime_loop = GetDlgItemInt(CfgPlayback, PauseLpText, NULL, FALSE);
			
			if (IsDlgButtonChecked(CfgPlayback, RateRecRadio))
				gOpts.pbRate = 0;
			else if (IsDlgButtonChecked(CfgPlayback, Rate60HzRadio))
				gOpts.pbRate = 60;
			else if (IsDlgButtonChecked(CfgPlayback, Rate50HzRadio))
				gOpts.pbRate = 50;
			else if (IsDlgButtonChecked(CfgPlayback, RateOtherRadio))
				gOpts.pbRate = GetDlgItemInt(CfgPlayback, RateText, NULL, FALSE);
			
			gOpts.smplRate = GetDlgItemInt(CfgPlayback, SmplPbRateText, NULL, FALSE);
			gOpts.resmplMode = (UINT8)COMBO_GETPOS(CfgPlayback, ResmpModeList);
			gOpts.chipSmplRate = GetDlgItemInt(CfgPlayback, SmplChipRateText, NULL, FALSE);
			gOpts.chipSmplMode = (UINT8)COMBO_GETPOS(CfgPlayback, ChipSmpModeList);
			
			ApplyCfg_General(*GetMainPlayerFISScan()->GetPlayer(), gOpts, false);
			ApplyCfg_General(*GetMainPlayerFIS()->GetPlayer(), gOpts, true);
			RefreshPlaybackOptions();
			QueueInfoReload();	// if the Playback Rate was changed, a refresh is needed
			
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		case RateRecRadio:
		case Rate60HzRadio:
		case Rate50HzRadio:
		case RateOtherRadio:
			// Windows already does the CheckRadioButton by itself
			//CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, LOWORD(wParam));
			if (LOWORD(wParam) == RateOtherRadio)
			{
				CTRL_ENABLE(CfgPlayback, RateText);
				CTRL_ENABLE(CfgPlayback, RateLabel);
				//SetFocus(GetDlgItem(CfgPlayback, RateText));
			}
			else
			{
				CTRL_DISABLE(CfgPlayback, RateText);
				CTRL_DISABLE(CfgPlayback, RateLabel);
			}
			break;
		case ResmpModeList:
			break;
		case ChipSmpModeList:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				BOOL CtrlEnable = (COMBO_GETPOS(CfgPlayback, ChipSmpModeList) > 0);
				CTRL_SET_ENABLE(CfgPlayback, SmplChipRateLabel, CtrlEnable);
				CTRL_SET_ENABLE(CfgPlayback, SmplChipRateText, CtrlEnable);
				CTRL_SET_ENABLE(CfgPlayback, SmplChipRateHzLabel, CtrlEnable);
			}
			break;
		case LoopText:
			if (HIWORD(wParam) == EN_CHANGE)
			{
				// for correct grammar: "Play loop x time(s)"
				BOOL Translated;
				bool NewLTMode = (GetDlgItemInt(CfgPlayback, LoopText, &Translated, FALSE) == 1);
				if (Translated && NewLTMode != LoopTimeMode)
				{
					char TimeStr[0x08];
					
					LoopTimeMode = NewLTMode;
					strcpy(TimeStr, "times");
					if (LoopTimeMode)
						TimeStr[4] = '\0';
					SetDlgItemTextA(CfgPlayback, LoopTimesLabel, TimeStr);
				}
			}
			break;
		case SurroundCheck:
			gOpts.pseudoSurround = CHECK2BOOL(CfgPlayback, SurroundCheck);
			
			UpdatePlayback();
			break;
		}
		break;
	case WM_HSCROLL:
		if (GetDlgCtrlID((HWND)lParam) == VolumeSlider)
		{
			double dbVol;
			int sliderPos;
			char tempStr[0x18];
			
			if (LOWORD(wParam) == TB_THUMBPOSITION || LOWORD(wParam) == TB_THUMBTRACK)
				sliderPos = HIWORD(wParam);
			else
				sliderPos = SLIDER_GETPOS(CfgPlayback, VolumeSlider);
			dbVol = (sliderPos - 120) / 10.0f;
			gOpts.volume = pow(2.0, dbVol / 6.0);
			
			sprintf(tempStr, "%.3f (%+.1f db)", gOpts.volume, dbVol);
			SetDlgItemTextA(CfgPlayback, VolumeText, tempStr);
			
			RefreshPlaybackOptions();
			if (LOWORD(wParam) != TB_THUMBTRACK)	// prevent too many skips
				UpdatePlayback();
		}
		break;
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}

static BOOL CALLBACK CfgDlgTagsProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	GeneralOptions& gOpts = pluginCfg.genOpts;
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			{
				char tempStr[0x80];
				UINT charCnt = GetDlgItemTextA(CfgTags, TitleFormatText, tempStr, 0x7F);	tempStr[0x7F] = '\0';
				gOpts.titleFormat.assign(tempStr, tempStr + charCnt);
			}
			
			//TagsStandardiseDates = CHECK2BOOL(CfgTags, cbTagsStandardiseDates);
			
			SetDlgItemInt(CfgTags, MLTypeText, gOpts.mediaLibFileType, FALSE);
			
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		case PreferJapCheck:
			gOpts.preferJapTag = CHECK2BOOL(CfgTags, PreferJapCheck);
			break;
		case FM2413Check:
			gOpts.appendFM2413 = CHECK2BOOL(CfgTags, FM2413Check);
			break;
		case TrimWhitespcCheck:
			gOpts.trimWhitespace = CHECK2BOOL(CfgTags, TrimWhitespcCheck);
			QueueInfoReload();
			break;
		case StdSeparatorsCheck:
			gOpts.stdSeparators = CHECK2BOOL(CfgTags, StdSeparatorsCheck);
			QueueInfoReload();
			break;
		case TagFallbackCheck:
			gOpts.fidTagFallback = CHECK2BOOL(CfgTags, TagFallbackCheck);
			break;
		case NoInfoCacheCheck:
			gOpts.noInfoCache = CHECK2BOOL(CfgTags, NoInfoCacheCheck);
			break;
		}
		break;
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}

static BOOL CALLBACK CfgDlgMutingProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		case MutingChipList:
		case MutingChipNumList:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				ShowMutingCheckBoxes((UINT8)COMBO_GETPOS(CfgMuting, MutingChipList),
									(UINT8)COMBO_GETPOS(CfgMuting, MutingChipNumList));
			}
			break;
		case ResetMuteCheck:
			if (HIWORD(wParam) == BN_CLICKED)
				pluginCfg.genOpts.resetMuting = CHECK2BOOL(CfgMuting, ResetMuteCheck);
			break;
		case MuteChipCheck:
			muteCOpts->chipDisable = CHECK2BOOL(CfgMuting, MuteChipCheck) ? 0xFF : 0x00;
			
			RefreshMuting();
			UpdatePlayback();
			break;
		}
		// I'm NOT going to have 24 case-lines!
		if (LOWORD(wParam) >= MuteChn1Check && LOWORD(wParam) <= MuteChn24Check)
		{
			if (HIWORD(wParam) == BN_CLICKED)
			{
				SetMutingData(LOWORD(wParam) - MuteChn1Check,
								(bool)CHECK2BOOL(CfgMuting, LOWORD(wParam)));
			}
		}
		break;
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}

static BOOL CALLBACK CfgDlgOptPanProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		case OpenINIButton:
		{
			const char* iniPath = GetIniFilePath();
			int retVal = (int)ShellExecuteA(hWndDlg, "open", iniPath, NULL, NULL, SW_SHOW);
			return (retVal >= 32) ? TRUE : FALSE;
		}
		case EmuCoreRadio1:
		case EmuCoreRadio2:
		case EmuCoreRadio3:
		{
			ChipOptions* cOpts = &pluginCfg.chipOpts[muteChipID][0];
			bool isSub;
			
			cOpts[0].emuType = LOWORD(wParam) - EmuCoreRadio1;
			UINT32 fcc = EmuTypeNum2CoreFCC(cOpts[0].chipType, cOpts[0].emuType, &isSub);
			if (isSub)
				cOpts[0].emuCoreSub = fcc;
			else
				cOpts[0].emuCore = fcc;
			
			// currently EmuCore affects both chip instances
			cOpts[1].emuType = cOpts[0].emuType;
			cOpts[1].emuCore = cOpts[0].emuCore;
			cOpts[1].emuCoreSub = cOpts[0].emuCoreSub;
			// enable/disable panning sliders, depending on the core's features
			ShowOptPanBoxes(muteChipID, muteChipSet);
			break;
		}
		case EmuOptChipList:
		case EmuOptChipNumList:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				ShowOptPanBoxes((UINT8)COMBO_GETPOS(CfgOptPan, EmuOptChipList),
								(UINT8)COMBO_GETPOS(CfgOptPan, EmuOptChipNumList));
			}
			break;
		}
		break;
	case WM_HSCROLL:
	{
		int sliderID = GetDlgCtrlID((HWND)lParam);
		if (sliderID >= PanChn1Slider && sliderID <= PanChn15Slider)
		{
			int sliderPos;
			bool dragging = (LOWORD(wParam) == TB_THUMBTRACK);
			if (dragging || LOWORD(wParam) == TB_THUMBPOSITION)
				sliderPos = HIWORD(wParam);
			else
				sliderPos = SLIDER_GETPOS(CfgOptPan, sliderID);
			SetPanningData(sliderID - PanChn1Slider, sliderPos, dragging);
		}
		break;
	}
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}

static BOOL CALLBACK CfgDlgChildProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	// Generic Procedure for unused/unfinished tabs
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}


static inline bool IsSongPlaying(void)
{
	PlayerA* plr = GetMainPlayerFIS()->GetPlayer();
	return !!(plr->GetState() & PLAYSTATE_PLAY);
}

static bool IsChipAvailable(const ChipOptions& cOpts)
{
	if (! IsSongPlaying())
		return true;	// not playing -> "available" (= user may change settings)
	
	PlayerBase* plr = GetMainPlayerFIS()->GetPlayer()->GetPlayer();
	std::vector<PLR_DEV_INFO> diList;
	plr->GetSongDeviceInfo(diList);
	
	for (size_t curDev = 0; curDev < diList.size(); curDev ++)
	{
		const PLR_DEV_INFO& pdi = diList[curDev];
		if (pdi.type == cOpts.chipType && pdi.instance == cOpts.chipInstance)
			return true;
	}
	return false;	// The current song doesn't use the chip - disable controls.
}

static UINT32 GetChipCore(const ChipOptions& cOpts)
{
	if (! IsSongPlaying())
		return EmuTypeNum2CoreFCC(cOpts.chipType, cOpts.emuType, NULL);
	
	PlayerBase* plr = GetMainPlayerFIS()->GetPlayer()->GetPlayer();
	std::vector<PLR_DEV_INFO> diList;
	plr->GetSongDeviceInfo(diList);
	
	for (size_t curDev = 0; curDev < diList.size(); curDev ++)
	{
		const PLR_DEV_INFO& pdi = diList[curDev];
		if (pdi.type == cOpts.chipType && pdi.instance == cOpts.chipInstance)
		{
			if (pdi.type == DEVID_YM2203 || pdi.type == DEVID_YM2608 || pdi.type == DEVID_YM2610)
				return EmuTypeNum2CoreFCC(cOpts.chipType, cOpts.emuType, NULL);	// TODO: return sub-core
			else
				return pdi.core;
		}
	}
	// return magic number for "not found" (avoid 0, just in case it is used by some core)
	return 0xFFFFFFFF;
}

static void ShowMutingCheckBoxes(UINT8 ChipID, UINT8 ChipSet)
{
	UINT16 ChnCount;
	UINT16 ChnCntS[4];
	const char* SpcName[64];	// Special Channel Names
	const char* SpcRngName[4];	// Special Channel Range Names
	UINT16 CurChn;
	UINT8 SpcModes;
	bool EnableChk;
	bool Checked;
	UINT8 CurMode;
	UINT16 ChnBase;
	UINT16 FnlChn;
	char TempName[0x18];
	
	for (CurChn = 0; CurChn < 64; CurChn ++)
		SpcName[CurChn] = NULL;
	for (CurChn = 0; CurChn < 4; CurChn ++)
		SpcRngName[CurChn] = NULL;
	
	muteCOpts = &pluginCfg.chipOpts[ChipID][ChipSet];
	muteChipID = ChipID;
	muteChipSet = ChipSet;
	
	EnableChk = IsChipAvailable(*muteCOpts);
	SpcModes = 0;
	{
		// get channel count from libvgm
		const DEV_DECL* const* devPtr = sndEmu_Devices;
		while(*devPtr != NULL && (*devPtr)->deviceID != muteCOpts->chipType)
			devPtr ++;
		ChnCount = (devPtr != NULL) ? (*devPtr)->channelCount(NULL) : 0;
	}
	// handle special channel names
	switch(muteCOpts->chipType)
	{
	case DEVID_SN76496:
		SpcName[3] = "&Noise";
		break;
	case DEVID_YM2413:
	case DEVID_YM3812:
	case DEVID_YM3526:
	case DEVID_Y8950:
		SpcName[ 9] = "&Bass Drum";
		SpcName[10] = "S&nare Drum";
		SpcName[11] = "&Tom Tom";
		SpcName[12] = "C&ymbal";
		SpcName[13] = "&Hi-Hat";
		if (muteCOpts->chipType == DEVID_Y8950)
			SpcName[14] = "&Delta-T";
		break;
	case DEVID_YM2612:
		SpcName[6] = "&DAC";
		break;
	case DEVID_YM2203:
		SpcModes = 3;
		ChnCntS[0] = 3;	// 3x FM
		SpcRngName[0] = "FM Chn";
		ChnCntS[1] = 0;
		ChnCntS[2] = 3;		// 3x SSG
		SpcRngName[2] = "SSG Chn";
		ChnCount = ChnCntS[0] + ChnCntS[1] + ChnCntS[2];
		break;
	case DEVID_YM2608:
	case DEVID_YM2610:
		SpcModes = 3;
		ChnCntS[0] = 6;	// 6x FM
		SpcRngName[0] = "FM Chn";
		ChnCntS[1] = 7;	// 6x ADPCM-A + 1x DeltaT
		SpcRngName[1] = "PCM Chn";
		SpcName[14] = "&Delta-T";
		ChnCntS[2] = 3;	// 3x SSG
		SpcRngName[2] = "SSG Chn";
		ChnCount = ChnCntS[0] + ChnCntS[1] + ChnCntS[2];
		break;
	case DEVID_YMF262:
		SpcName[18] = "&Bass Drum";
		SpcName[19] = "S&nare Drum";
		SpcName[20] = "&Tom Tom";
		SpcName[21] = "C&ymbal";
		SpcName[22] = "&Hi-Hat";
		break;
	case DEVID_32X_PWM:
		EnableChk &= ! ChipSet;
		break;
	case DEVID_GB_DMG:
		SpcName[0] = "Square &1";
		SpcName[1] = "Square &2";
		SpcName[2] = "&Wave";
		SpcName[3] = "&Noise";
		break;
	case DEVID_NES_APU:
		SpcName[0] = "Square &1";
		SpcName[1] = "Square &2";
		SpcName[2] = "&Triangle";
		SpcName[3] = "&Noise";
		SpcName[4] = "&DPCM";
		SpcName[5] = "&FDS";
		break;
	case DEVID_QSOUND:
		SpcName[16] = "ADPCM 1 (&H)";
		SpcName[17] = "ADPCM 2 (&I)";
		SpcName[18] = "ADPCM 3 (&J)";
		EnableChk &= ! ChipSet;
		break;
	}
	if (ChnCount == 0)
		EnableChk = false;
	else if (ChnCount > 24)
		ChnCount = 24;	// currently only 24 checkboxes are supported
	
	SETCHECKBOX(CfgMuting, MuteChipCheck, muteCOpts->chipDisable);
	CTRL_SET_ENABLE(CfgMuting, MuteChipCheck, EnableChk);
	if (! SpcModes)
	{
		for (CurChn = 0; CurChn < ChnCount; CurChn ++)
		{
			if (SpcName[CurChn] == NULL)
			{
				if (1 + CurChn < 10)
					sprintf(TempName, "Channel &%u", 1 + CurChn);
				else
					sprintf(TempName, "Channel %u (&%c)", 1 + CurChn, 'A' + (CurChn - 9));
				SetDlgItemTextA(CfgMuting, MuteChn1Check + CurChn, TempName);
			}
			else
			{
				SetDlgItemTextA(CfgMuting, MuteChn1Check + CurChn, SpcName[CurChn]);
			}
			
			if (muteCOpts->chipType == DEVID_YMF278B)
				Checked = (muteCOpts->muteMask[1] >> CurChn) & 0x01;
			else
				Checked = (muteCOpts->muteMask[0] >> CurChn) & 0x01;
			SETCHECKBOX(CfgMuting, MuteChn1Check + CurChn, Checked);
			CTRL_SET_ENABLE(CfgMuting, MuteChn1Check + CurChn, EnableChk);
			CTRL_SHOW(CfgMuting, MuteChn1Check + CurChn);
		}
	}
	else
	{
		for (CurMode = 0; CurMode < SpcModes; CurMode ++)
		{
			ChnBase = CurMode * 8;
			if (SpcRngName[CurMode] == NULL)
				SpcRngName[CurMode] = "Channel";
			
			for (CurChn = 0, FnlChn = ChnBase; CurChn < ChnCntS[CurMode]; CurChn ++, FnlChn ++)
			{
				if (SpcName[FnlChn] == NULL)
				{
					if (1 + CurChn < 10)
						sprintf(TempName, "%s &%u", SpcRngName[CurMode], 1 + CurChn);
					else
						sprintf(TempName, "%s %u (&%c)", SpcRngName[CurMode], 1 + CurChn,
								'A' + (CurChn - 9));
					SetDlgItemTextA(CfgMuting, MuteChn1Check + FnlChn, TempName);
				}
				else
				{
					SetDlgItemTextA(CfgMuting, MuteChn1Check + FnlChn, SpcName[FnlChn]);
				}
				
				switch(CurMode)
				{
				case 0:
					Checked = (muteCOpts->muteMask[0] >> CurChn) & 0x01;
					break;
				case 1:
					Checked = (muteCOpts->muteMask[0] >> (ChnCntS[0] + CurChn)) & 0x01;
					break;
				case 2:
					Checked = (muteCOpts->muteMask[1] >> CurChn) & 0x01;
					break;
				}
				SETCHECKBOX(CfgMuting, MuteChn1Check + FnlChn, Checked);
				CTRL_SET_ENABLE(CfgMuting, MuteChn1Check + FnlChn, EnableChk);
				CTRL_SHOW(CfgMuting, MuteChn1Check + FnlChn);
			}
			for (; CurChn < 8; CurChn ++, FnlChn ++)
			{
				CTRL_HIDE(CfgMuting, MuteChn1Check + FnlChn);
				CTRL_DISABLE(CfgMuting, MuteChn1Check + FnlChn);
				SetDlgItemTextA(CfgMuting, MuteChn1Check + FnlChn, "");
			}
		}
		CurChn = FnlChn;
	}
	for (; CurChn < 24; CurChn ++)
	{
		// I thought that disabling a window should prevent it from catching other
		// windows' keyboard shortcuts.
		// But NO, it seems that you have to make its text EMPTY to make it work!
		CTRL_HIDE(CfgMuting, MuteChn1Check + CurChn);
		CTRL_DISABLE(CfgMuting, MuteChn1Check + CurChn);
		SetDlgItemTextA(CfgMuting, MuteChn1Check + CurChn, "");
	}
	
	return;
}

static void SetMutingData(UINT32 CheckBox, bool MuteOn)
{
	UINT8 ChnCntS[0x04];
	UINT8 SpcModes;
	UINT8 maskID;
	UINT8 chnID;
	
	SpcModes = 0;
	switch(muteCOpts->chipType)
	{
	case DEVID_YM2203:
		SpcModes = 3;
		ChnCntS[0] = 3;	// 3x FM
		ChnCntS[1] = 0;
		ChnCntS[2] = 3;	// 3x SSG
		break;
	case DEVID_YM2608:
	case DEVID_YM2610:
		SpcModes = 3;
		ChnCntS[0] = 6;	// 6x FM
		ChnCntS[1] = 7;	// 6x ADPCM-A + 1x DeltaT
		ChnCntS[2] = 3;	// 3x SSG
		break;
	}
	
	maskID = 0xFF;
	if (! SpcModes)
	{
		maskID = 0;
		if (muteCOpts->chipType == DEVID_YMF278B)
			maskID = 1;
		chnID = CheckBox;
	}
	else
	{
		switch(CheckBox / 8)
		{
		case 0:
			maskID = 0;
			chnID = CheckBox % 8;
			break;
		case 1:
			maskID = 0;
			chnID = ChnCntS[0] + (CheckBox % 8);
			break;
		case 2:
			maskID = 1;
			chnID = CheckBox % 8;
			break;
		}
	}
	if (maskID == 0xFF)
		return;
	muteCOpts->muteMask[maskID] &= ~(1 << chnID);
	muteCOpts->muteMask[maskID] |= (MuteOn << chnID);
	
	RefreshMuting();
	UpdatePlayback();
	
	return;
}

static inline UINT16 PanPos2Slider(INT16 panPos)
{
	return panPos / 0x08 + 0x20;	// scale from -0x100..+0x100 to 0x00..0x40
}

static inline INT16 PanSlider2Pos(UINT16 sliderValue)
{
	return (sliderValue - 0x20) * 0x08;
}

static void ShowOptPanBoxes(UINT8 ChipID, UINT8 ChipSet)
{
	UINT8 ChnCount;
	UINT8 CurChn;
	const char* SpcName[32];	// Special Channel Names
	const char* CoreName[3];	// Names for the Emulation Cores
	bool EnableChk;
	UINT8 coreCnt;
	UINT32 curEmuCore;
	UINT8 panMaskID;
	
	for (CurChn = 0x00; CurChn < 32; CurChn ++)
		SpcName[CurChn] = NULL;
	
	CoreName[0] = NULL;
	CoreName[1] = NULL;
	CoreName[2] = NULL;
	
	muteCOpts = &pluginCfg.chipOpts[ChipID][ChipSet];
	muteChipID = ChipID;
	muteChipSet = ChipSet;
	
	EnableChk = IsChipAvailable(*muteCOpts);
	curEmuCore = GetChipCore(*muteCOpts);
	
	coreCnt = 0;
	panMaskID = 0xFF;
	switch(muteCOpts->chipType)
	{
	case DEVID_SN76496:
		CoreName[0] = "MAME";
		CoreName[1] = "Maxim";
		coreCnt = 2;
		if (curEmuCore == FCC_MAXM)
			panMaskID = 0;
		
		SpcName[3] = "&N";
		break;
	case DEVID_YM2413:
		CoreName[0] = "EMU2413";
		CoreName[1] = "MAME";
		CoreName[2] = "Nuked OPLL";
		coreCnt = 3;
		if (curEmuCore == FCC_EMU_)
			panMaskID = 0;
		
		SpcName[ 9] = "&BD";
		SpcName[10] = "S&D";
		SpcName[11] = "&TT";
		SpcName[12] = "C&Y";
		SpcName[13] = "&HH";
		break;
	case DEVID_YM2612:
		CoreName[0] = "MAME";
		CoreName[1] = "Nuked OPN2";
		CoreName[2] = "Gens";
		coreCnt = 3;
		break;
	case DEVID_YM2151:
		CoreName[0] = "MAME";
		CoreName[1] = "Nuked OPM";
		coreCnt = 2;
		break;
	case DEVID_YM2203:
	case DEVID_YM2608:
	case DEVID_YM2610:
		CoreName[0] = "EMU2149";
		CoreName[1] = "MAME";
		coreCnt = 2;
		if (curEmuCore == FCC_EMU_)
			panMaskID = 1;
		
		SpcName[0] = "S&1";
		SpcName[1] = "S&2";
		SpcName[2] = "S&3";
		break;
	case DEVID_YM3812:
	case DEVID_YMF262:
		CoreName[0] = "AdLibEmu";
		CoreName[1] = "MAME";
		CoreName[2] = "Nuked OPL3";
		coreCnt = 3;
		break;
	case DEVID_YMF278B:
		CoreName[0] = "openMSX";
		break;
	case DEVID_32X_PWM:
		//CoreName[0] = "Gens";	// This is so much stripped down that it barely is Gens anymore.
		break;
	case DEVID_AY8910:
		CoreName[0] = "EMU2149";
		CoreName[1] = "MAME";
		coreCnt = 2;
		if (curEmuCore == FCC_EMU_)
			panMaskID = 0;
		break;
	case DEVID_NES_APU:
		CoreName[0] = "NSFPlay";
		CoreName[1] = "MAME";
		coreCnt = 2;
		break;
	case DEVID_C6280:
		CoreName[0] = "Ootake";
		CoreName[1] = "MAME";
		coreCnt = 2;
		break;
	case DEVID_QSOUND:
		CoreName[0] = "superctr";
		CoreName[1] = "MAME";
		coreCnt = 2;
		break;
	case DEVID_SAA1099:
		CoreName[0] = "Valley Bell";
		CoreName[1] = "MAME";
		coreCnt = 2;
		break;
	case DEVID_C352:
		CoreName[0] = "superctr";
		break;
	case DEVID_MIKEY:
		CoreName[0] = "laoo";
		break;
	default:
		EnableChk = false;
		break;
	}
	if (panMaskID != 0xFF && ! muteCOpts->chnCnt[panMaskID])
		panMaskID = 0xFF;
	
	ChnCount = (panMaskID != 0xFF) ? muteCOpts->chnCnt[panMaskID] : 0;
	if (ChnCount > 15)
		ChnCount = 15;	// there are only 15 sliders
	
	for (int curCore = 0; curCore < 3; curCore ++)
	{
		bool doEnable = EnableChk && (coreCnt > 1) && (curCore < coreCnt);
		CTRL_SET_ENABLE(CfgOptPan, EmuCoreRadio1 + curCore, doEnable);
		
		const char* coreName = CoreName[curCore];
		if (coreName == NULL)
			coreName = (! curCore) ? "default" : "alternative";
		SetDlgItemTextA(CfgOptPan, EmuCoreRadio1 + curCore, coreName);
	}
	
	CheckRadioButton(CfgOptPan, EmuCoreRadio1, EmuCoreRadio3, EmuCoreRadio1 + muteCOpts->emuType);
	
	INT16 PanPos;
	char TempName[0x20];
	
	for (CurChn = 0; CurChn < ChnCount; CurChn ++)
	{
		bool doEnable = EnableChk && (panMaskID != 0xFF);
		if (SpcName[CurChn] == NULL)
		{
			if (1 + CurChn < 10)
				sprintf(TempName, "&%u", 1 + CurChn);
			else
				sprintf(TempName, "&%c", 'A' + (CurChn - 9));
			SetDlgItemTextA(CfgOptPan, PanChn1Label + CurChn, TempName);
		}
		else
		{
			SetDlgItemTextA(CfgOptPan, PanChn1Label + CurChn, SpcName[CurChn]);
		}
		CTRL_SET_ENABLE(CfgOptPan, PanChn1Label + CurChn, doEnable);
		
		if (panMaskID != 0xFF)
			PanPos = PanPos2Slider(muteCOpts->panMask[panMaskID][CurChn]);
		else
			PanPos = 0x20;	// middle position
		SLIDER_SETPOS(CfgOptPan, PanChn1Slider + CurChn, PanPos);
		CTRL_SET_ENABLE(CfgOptPan, PanChn1Slider + CurChn, doEnable);
	}
	PanPos = 0x20;	// middle position
	for (; CurChn < 15; CurChn ++)
	{
		if (1 + CurChn < 10)
			sprintf(TempName, "&%u", 1 + CurChn);
		else
			sprintf(TempName, "&%c", 'A' + (CurChn - 9));
		SetDlgItemTextA(CfgOptPan, PanChn1Label + CurChn, TempName);
		
		CTRL_DISABLE(CfgOptPan, PanChn1Label + CurChn);
		CTRL_DISABLE(CfgOptPan, PanChn1Slider + CurChn);
		SLIDER_SETPOS(CfgOptPan, PanChn1Slider + CurChn, PanPos);
	}
	
	return;
}

static void SetPanningData(UINT32 sliderID, UINT16 value, bool noRefresh)
{
	UINT8 panMaskID = 0xFF;
	switch(muteCOpts->chipType)
	{
	case DEVID_SN76496:
		panMaskID = 0;
		break;
	case DEVID_YM2413:
		panMaskID = 0;
		break;
	case DEVID_YM2203:
	case DEVID_YM2608:
	case DEVID_YM2610:
		panMaskID = 1;
		break;
	case DEVID_AY8910:
		panMaskID = 0;
		break;
	}
	if (panMaskID == 0xFF)
		return;
	if (sliderID >= muteCOpts->chnCnt[panMaskID])
		return;
	
	muteCOpts->panMask[panMaskID][sliderID] = PanSlider2Pos(value);
	
	RefreshPanning();
	if (! noRefresh)
		UpdatePlayback();
	
	return;
}

void Dialogue_TrackChange(void)
{
	// redraw Muting Boxes and Panning Sliders
	ShowMutingCheckBoxes(muteChipID, muteChipSet);
	ShowOptPanBoxes(muteChipID, muteChipSet);
	
	return;
}
