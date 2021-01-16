// Winamp VGM input plug-in
// ------------------------
// Written by Valley Bell, 2020
//
// Made using the Example .RAW plug-in by Justin Frankel and
// small parts (mainly dialogues) of the old in_vgm 0.35 by Maxim.

// #defines used by the Visual C++ project file:
//	UNICODE_INPUT_PLUGIN (Unicode build only)

#include <vector>
#include <string>
#include <windows.h>
#include <stdio.h>
#include <locale.h>	// for setlocale

#ifdef _MSC_VER
#define snprintf	_snprintf
#endif

extern "C"
{
#include "Winamp/in2.h"
#include "Winamp/wa_ipc.h"

#include <zlib.h>	// for info in About message

#include <stdtype.h>

#include "ini_func.h"
}	// end extern "C"

#include <emu/SoundEmu.h>	// for SndEmu_GetDevName()
#include <utils/DataLoader.h>
#include <utils/FileLoader.h>
#include <player/playerbase.hpp>
#include <player/vgmplayer.hpp>
#include <player/s98player.hpp>
#include <player/droplayer.hpp>
#include <player/playera.hpp>

#include "utils.hpp"
#include "FileInfoStorage.hpp"
#include "TagFormatter.hpp"
#include "playcfg.hpp"
#include "in_vgm.h"


#ifndef WM_WA_MPEG_EOF
// post this to the main window at end of file (after playback as stopped)
#define WM_WA_MPEG_EOF	WM_USER+2
#endif

#ifdef UNICODE_INPUT_PLUGIN
typedef std::wstring	in2string;
#else
typedef std::string 	in2string;
#endif

#define DLL_EXPORT	__declspec(dllexport)


#define NUM_CHN			2
#define BIT_PER_SEC		16
#define SMPL_BYTES		(NUM_CHN * BIT_PER_SEC / 8)

#define RENDER_SAMPLES	576	// visualizations look best with 576 samples
#define BLOCK_SIZE		(RENDER_SAMPLES * SMPL_BYTES)


// Function Prototypes from dlg_cfg.c
int ConfigDlg_Show(HINSTANCE hDllInst, HWND hWndParent);
void Dialogue_TrackChange(void);

// Function Protorypes from dlg_fileinfo.c
void InitFileInfoDialog(FileInfoStorage* fis);
void DeinitFileInfoDialog(void);
void FileInfoDlg_LoadFile(const char* fileName);
void FileInfoDlg_LoadFile(const wchar_t* fileName);
int FileInfoDlg_Show(HINSTANCE hDllInst, HWND hWndParent);
//void QueueInfoReload(void);

// Function Protorypes from extFileInfo.cpp
void InitExtFileInfo(FileInfoStorage* scan, CPCONV* cpcU8w, CPCONV* cpcU8a);
void DeinitExtFileInfo(void);



// Function Prototypes
//extern "C" BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
void Config(HWND hWndParent);	// Note: also used by dlg_fileinfo.cpp
static void About(HWND hWndParent);
static void GenerateFileExtList(void);
static void Init(void);
static void Deinit(void);

static int IsOurFile(const in_char* fn);

static inline UINT32 MulDivRound(UINT64 Number, UINT64 Numerator, UINT64 Denominator);
static inline UINT32 MSec2Samples(UINT32 val, const PlayerA& player);
static void PreparePlayback(void);
static int Play(const in_char* FileName);
static void Pause(void);
static void Unpause(void);
static int IsPaused(void);
static void Stop(void);

int GetPlrSongLengthMS(const PlayerA& plr);
int GetPlrSongLengthMS(const FileInfoStorage::SongInfo& songInfo);
int GetLength(void);

static int GetOutputTime(void);
static void SetOutputTime(int time_in_ms);
static void SetVolume(int volume);
static void SetPan(int pan);
const char* GetIniFilePath(void);
FileInfoStorage* GetMainPlayerFIS(void);
void RefreshPlaybackOptions(void);
void RefreshMuting(void);
void RefreshPanning(void);
void UpdatePlayback(void);

static int InfoDialog(const in_char* fileName, HWND hWnd);
static void GetFileInfo(const in_char* fileName, in_char* title, int* length_in_ms);
static void EQ_Set(int on, char data[10], int preamp);

static DWORD WINAPI DecodeThread(LPVOID b);
static UINT8 FilePlayCallback(PlayerBase* player, void* userParam, UINT8 evtType, void* evtParam);
static DATA_LOADER* PlayerFileReqCallback(void* userParam, PlayerBase* player, const char* fileName);


// the output module (defined at the end of the file)
extern In_Module WmpMod;
static std::string wmpFileExtList;

static UINT32 decode_pos;			// current decoding position (depends on SampleRate)
static int decode_pos_ms;			// Used for correcting DSP plug-in pitch changes
static volatile int seek_needed;	// if != -1, it is the point that the decode 
									// thread should seek to, in ms.

static volatile bool killDecodeThread = 0;			// the kill switch for the decode thread
static HANDLE thread_handle = INVALID_HANDLE_VALUE;	// the handle to the decode thread
static volatile bool InStopFunc;
static volatile bool PausePlay;
static volatile bool EndPlay;

       PluginConfig pluginCfg;

static std::vector<std::string> appSearchPaths;
static std::string wa5IniDirBase;	// may be empty OR "%APPDATA%\Winamp"
static std::string waIniDir;	// either the DLL directory of "{wa5IniDirBase}\Plugins"
static std::string iniFilePath;
static CPCONV* cpcU8_Wide;
static CPCONV* cpcU8_ACP;

static in2string fileNamePlaying;	// currently playing file (used for getting info on the current file)
static PlayerA mainPlayer;
static DATA_LOADER* mainDLoad = NULL;

static FileInfoStorage* fisMain;
static FileInfoStorage* fisScan;
static FileInfoStorage* fisFileInfo;

struct SoundChipEnum
{
	UINT8 id;
	const char* name;
};
static std::vector<SoundChipEnum> soundChipList;


#if 0
extern "C" BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
#endif

void Config(HWND hWndParent)
{
	ConfigDlg_Show(WmpMod.hDllInstance, hWndParent);
	return;
}

static void About(HWND hWndParent)
{
	static const char* msgStr =
		INVGM_TITLE
#ifdef UNICODE_INPUT_PLUGIN
		" (Unicode build)"
#else
		" (ANSI build)"
#endif
		"\n"
		"by Valley Bell 2020\n"
		"\n"
		"Build date: " __DATE__ " (" INVGM_VERSION ")\n"
		"\n"
		"https://vgmrips.net/\n"
		"\n"
		"Current status:\n"
		"VGM file support up to version " VGM_VER_STR "\n"
		"Currently %u chips are emulated:\n"	// CHIP_COUNT
		"%s\n"	// ChipList
		"\n"
		"Using:\n"
		"ZLib " ZLIB_VERSION " (http://www.zlib.org/)\n"
		"libvgm (https://github.com/ValleyBell/libvgm/)\n"
		"\n"
		"Special thanks goes to Maxim for the\n"
			"initial in_vgm plugin (v0.3x)\n"
		"Thanks to all the people that worked on/contributed to libvgm.\n";
	std::string chipList;
	std::string finalMsg;
	size_t curChip;
	
	// generate Chip list
	for (curChip = 0; curChip < soundChipList.size(); curChip ++)
	{
		if (curChip && ! (curChip % 6))
		{
			// replace space with new-line every 6 chips
			chipList[chipList.size() - 1] = '\n';
		}
		
		chipList += soundChipList[curChip].name;
		chipList += ", ";
	}
	if (chipList.size() >= 2)
		chipList = chipList.substr(0, chipList.size() - 2);
	
	finalMsg.resize(strlen(msgStr) + 0x20 + chipList.length());
	snprintf(&finalMsg[0], finalMsg.size(), msgStr, (unsigned)soundChipList.size(), chipList.c_str());
	
	MessageBox(hWndParent, finalMsg.c_str(), WmpMod.description, MB_ICONINFORMATION | MB_OK);
	
	return;
}

static char* GetAppFilePath(HMODULE hModule)
{
	char* appPath;
	int retVal;
	
#ifdef USE_WMAIN
	std::vector<wchar_t> appPathW;
	
	appPathW.resize(MAX_PATH);
	retVal = GetModuleFileNameW(hModule, &appPathW[0], appPathW.size());
	if (! retVal)
		appPathW[0] = L'\0';
	
	retVal = WideCharToMultiByte(CP_UTF8, 0, &appPathW[0], -1, NULL, 0, NULL, NULL);
	if (retVal < 0)
		retVal = 1;
	appPath = (char*)malloc(retVal);
	retVal = WideCharToMultiByte(CP_UTF8, 0, &appPathW[0], -1, appPath, retVal, NULL, NULL);
	if (retVal < 0)
		appPath[0] = '\0';
	appPathW.clear();
#else
	appPath = (char*)malloc(MAX_PATH * sizeof(char));
	retVal = GetModuleFileNameA(hModule, appPath, MAX_PATH);
	if (! retVal)
		appPath[0] = '\0';
#endif
	
	return appPath;
}

static void InitAppSearchPaths(HMODULE hModule)
{
	appSearchPaths.clear();
	
	// 1. DLL's directory ("C:\Program Files\Winamp\Plugins\")
	char* appPath = GetAppFilePath(hModule);
	const char* appTitle = GetFileTitle(appPath);
	if (appTitle != appPath)
		appSearchPaths.push_back(std::string(appPath, appTitle - appPath));
	free(appPath);
	
	// 2. Winamp configuration dir ("%APPDATA%\Winamp\Plugins)
	// Note: checking wa5IniDirBase, but adding waIniDir is intentional
	// When wa5IniDirBase is empty, then waIniDir is equal the DLL directory.
	if (! wa5IniDirBase.empty())
		appSearchPaths.push_back(waIniDir);
	
	// 3. working directory
	appSearchPaths.push_back("./");
	
	return;
}

static void DetermineWinampIniPath(HMODULE hModule)
{
	const char* result = (const char*)SendMessageA(WmpMod.hMainWindow, WM_WA_IPC, 0, IPC_GETINIDIRECTORY);
	if (result != NULL)
	{
		// Winamp 5.11+ (with user profiles)
		wa5IniDirBase = result;
		waIniDir = CombinePaths(wa5IniDirBase, "Plugins");
		StandardizeDirSeparators(waIniDir);
		
		// make sure folder exists
		CreateDirectoryA(waIniDir.c_str(), NULL);
		waIniDir += '/';
	}
	else
	{
		// old Winamp
		wa5IniDirBase = std::string();
		
		char* appPath = GetAppFilePath(hModule);
		const char* appTitle = GetFileTitle(appPath);
		waIniDir = std::string(appPath, appTitle - appPath);
		free(appPath);
	}
	
	return;
}

static const char* const FILETYPE_LIST[] =
{
	"vgm;vgz", "VGM Audio Files (*.vgm; *.vgz)",
	"s98", "T98 Sound Log Files (*.s98)",
	"dro", "DOSBox Raw OPL Files (*.dro)",
};

static void GenerateFileExtList(void)
{
	wmpFileExtList = std::string();
	if (pluginCfg.genOpts.fileTypes.vgm)
	{
		wmpFileExtList += FILETYPE_LIST[0];	wmpFileExtList += '\0';
		wmpFileExtList += FILETYPE_LIST[1];	wmpFileExtList += '\0';
	}
	if (pluginCfg.genOpts.fileTypes.s98)
	{
		wmpFileExtList += FILETYPE_LIST[2];	wmpFileExtList += '\0';
		wmpFileExtList += FILETYPE_LIST[3];	wmpFileExtList += '\0';
	}
	if (pluginCfg.genOpts.fileTypes.dro)
	{
		wmpFileExtList += FILETYPE_LIST[4];	wmpFileExtList += '\0';
		wmpFileExtList += FILETYPE_LIST[5];	wmpFileExtList += '\0';
	}
	wmpFileExtList += '\0';	// final terminator
	return;
}

static void Init(void)
{
	//AllocConsole();
	//freopen("CONOUT$", "w", stdout);
	setlocale(LC_CTYPE, "");
	
	InitAppSearchPaths(WmpMod.hDllInstance);
	DetermineWinampIniPath(WmpMod.hDllInstance);
	// The path of the INI to be loaded/written depends on the Winamp version.
	// (No fallback paths for now.)
	iniFilePath = CombinePaths(waIniDir, "in_vgm.ini");
	Ini_SetFilePath(iniFilePath.c_str());
	
	soundChipList.clear();
	for (UINT16 curDev = 0; curDev < 0x100; curDev ++)
	{
		// just stupidly iterate over all possible IDs to figure out what sound chips are supported
		const char* devName = SndEmu_GetDevName((UINT8)curDev, 0x00, NULL);
		if (devName != NULL)
		{
			SoundChipEnum sce = {(UINT8)curDev, devName};
			soundChipList.push_back(sce);
		}
	}
	
	cpcU8_Wide = NULL;
	CPConv_Init(&cpcU8_Wide, "UTF-8", "UTF-16LE");
	{
		std::string cpName(0x10, '\0');
		snprintf(&cpName[0], cpName.size(), "CP%u", GetACP());
		cpcU8_ACP = NULL;
		CPConv_Init(&cpcU8_ACP, "UTF-8", cpName.c_str());
	}
	
	LoadConfiguration(pluginCfg, iniFilePath.c_str());
	
	GenerateFileExtList();
	WmpMod.FileExtensions = &wmpFileExtList[0];
	
	mainPlayer.RegisterPlayerEngine(new VGMPlayer);
	mainPlayer.RegisterPlayerEngine(new S98Player);
	mainPlayer.RegisterPlayerEngine(new DROPlayer);
	mainPlayer.SetEventCallback(FilePlayCallback, NULL);
	mainPlayer.SetFileReqCallback(PlayerFileReqCallback, NULL);
	fisMain = new FileInfoStorage(&mainPlayer);
	
	fisScan = new FileInfoStorage(nullptr);
	PlayerA* fileScanPlayer = fisScan->GetPlayer();
	
	fisFileInfo = new FileInfoStorage(nullptr);
	
	InitFileInfoDialog(fisFileInfo);
	// Let's just assume that Winamp doesn't call GetFileInfo() and GetExtendedFileInfo() in parallel
	// and thus just share fisScan/cpU8_* between those two.
	InitExtFileInfo(fisScan, cpcU8_Wide, cpcU8_ACP);
	
	mainPlayer.SetOutputSettings(pluginCfg.genOpts.smplRate, NUM_CHN, BIT_PER_SEC, RENDER_SAMPLES);
	//fileScanPlayer->SetSampleRate(pluginCfg.genOpts.smplRate);
	//fileScanPlayer->SetFadeSamples(MSec2Samples(pluginCfg.genOpts.fadeTime, *fileScanPlayer));
	
	ApplyCfg_General(mainPlayer, pluginCfg.genOpts);
	for (size_t curChp = 0; curChp < pluginCfg.chipOpts.size(); curChp ++)
	{
		for (size_t curCSet = 0; curCSet < 2; curCSet ++)
		{
			const ChipOptions& cOpt = pluginCfg.chipOpts[curChp][curCSet];
			if (cOpt.chipType == 0xFF)
				continue;
			ApplyCfg_Chip(mainPlayer, pluginCfg.genOpts, cOpt);
		}
	}
	//printf("Everything loaded.\n");
	
	return;
}

static void Deinit(void)
{
	// one-time deinit, such as memory freeing
	SaveConfiguration(pluginCfg, iniFilePath.c_str());
	
	DeinitFileInfoDialog();
	DeinitExtFileInfo();
	
	mainPlayer.UnloadFile();
	mainPlayer.UnregisterAllPlayers();
	delete fisMain;
	delete fisScan;
	delete fisFileInfo;
	CPConv_Deinit(cpcU8_Wide);	cpcU8_Wide = NULL;
	CPConv_Deinit(cpcU8_ACP);	cpcU8_ACP = NULL;
	//FreeConsole();
	
	return;
}

static int IsOurFile(const in_char* fn)
{
	// used for detecting URL streams - currently unused.
	// return ! strncmp(fn, "http://",7); to detect HTTP streams, etc
	return 0;
} 


static inline UINT32 MulDivRound(UINT64 Number, UINT64 Numerator, UINT64 Denominator)
{
	return (UINT32)((Number * Numerator + Denominator / 2) / Denominator);
}

static inline UINT32 MSec2Samples(UINT32 val, const PlayerA& player)
{
	return (UINT32)(((UINT64)val * player.GetSampleRate() + 500) / 1000);
}

static void PreparePlayback(void)
{
	PlayerBase* player = mainPlayer.GetPlayer();
	UINT32 timeMS;
	
	ApplyCfg_General(mainPlayer, pluginCfg.genOpts, true);
	for (size_t curChp = 0; curChp < pluginCfg.chipOpts.size(); curChp ++)
	{
		for (size_t curCSet = 0; curCSet < 2; curCSet ++)
		{
			ChipOptions& cOpt = pluginCfg.chipOpts[curChp][curCSet];
			if (cOpt.chipType == 0xFF)
				continue;
			ApplyCfg_Chip(mainPlayer, pluginCfg.genOpts, cOpt);
		}
	}
	
	INT32 volume = (INT32)(0x10000 * fisMain->_songInfo.volGain * pluginCfg.genOpts.volume + 0.5);
	mainPlayer.SetMasterVolume(volume);
	
	mainPlayer.SetFadeSamples(MSec2Samples(pluginCfg.genOpts.fadeTime, mainPlayer));
	
	timeMS = (player->GetLoopTicks() == 0) ? pluginCfg.genOpts.pauseTime_jingle : pluginCfg.genOpts.pauseTime_loop;
	mainPlayer.SetEndSilenceSamples(MSec2Samples(timeMS, mainPlayer));
	
	return;
}

// called when winamp wants to play a file
static int Play(const in_char* fileName)
{
	int maxlatency;
	DWORD thread_id;
	UINT8 retVal;
	
	PausePlay = false;
	EndPlay = false;
	decode_pos = 0;
	decode_pos_ms = 0;
	seek_needed = -1;
	mainPlayer.SetSampleRate(pluginCfg.genOpts.smplRate);
	
	// return -1 - jump to next file in playlist
	// return +1 - stop the playlist
#ifdef UNICODE_INPUT_PLUGIN
	mainDLoad = FileLoader_InitW(fileName);
#else
	mainDLoad = FileLoader_Init(fileName);
#endif
	DataLoader_SetPreloadBytes(mainDLoad, 0x100);
	retVal = DataLoader_Load(mainDLoad);
	if (retVal)
	{
		DataLoader_CancelLoading(mainDLoad);
		DataLoader_Deinit(mainDLoad);	mainDLoad = NULL;
		return -1;	// file not found
	}
	retVal = mainPlayer.LoadFile(mainDLoad);
	if (retVal)
	{
		DataLoader_CancelLoading(mainDLoad);
		DataLoader_Deinit(mainDLoad);	mainDLoad = NULL;
		return +1;	// file invalid
	}
	fisMain->SetFileName(fileName);
	fisMain->ReadSongInfo();
	fileNamePlaying = fileName;
	
	// -1 and -1 are to specify buffer and prebuffer lengths.
	// -1 means to use the default, which all input plug-ins should really do.
	maxlatency = WmpMod.outMod->Open(pluginCfg.genOpts.smplRate, NUM_CHN, BIT_PER_SEC, -1, -1);
	if (maxlatency < 0) // error opening device
		return +1;
	
	PreparePlayback();
	
	mainPlayer.Start();
	
	{
		int kBitRate = (int)(fisMain->_songInfo.bitRate / 1000.0 + 0.5);
		int sRateKHz = (pluginCfg.genOpts.smplRate + 500) / 1000;
		WmpMod.SetInfo(kBitRate, sRateKHz, NUM_CHN, 1);
	}
	
	// initialize visualization stuff
	WmpMod.SAVSAInit(maxlatency, pluginCfg.genOpts.smplRate);
	WmpMod.VSASetInfo(pluginCfg.genOpts.smplRate, NUM_CHN);
	
	// set the output plug-ins default volume.
	// volume is 0-255, -666 is a token for current volume.
	WmpMod.outMod->SetVolume(-666); 
	
	// launch decode thread
	killDecodeThread = 0;
	thread_handle = CreateThread(NULL, 0, DecodeThread, NULL, 0x00, &thread_id);
	
	Dialogue_TrackChange();
	
	return 0; 
}

static void Pause(void)
{
	PausePlay = true;
	WmpMod.outMod->Pause(PausePlay);
	
	return;
}

static void Unpause(void)
{
	PausePlay = false;
	WmpMod.outMod->Pause(PausePlay);
	
	return;
}

static int IsPaused(void)
{
	return PausePlay;
}

static void Stop(void)
{
	if (InStopFunc)
		return;
	
	InStopFunc = true;
	// TODO: add Mutex for this block.
	//	Stupid XMPlay seems to call Stop() twice in 2 seperate threads.
	if (thread_handle != INVALID_HANDLE_VALUE)
	{
		killDecodeThread = 1;
		if (WaitForSingleObject(thread_handle, 10000) == WAIT_TIMEOUT)
		{
			MessageBox(WmpMod.hMainWindow, "Error asking thread to die!\n",
						"Error killing decode thread", 0);
			TerminateThread(thread_handle, 0);
		}
		CloseHandle(thread_handle);
		thread_handle = INVALID_HANDLE_VALUE;
	}
	
	// close output system
	WmpMod.outMod->Close();
	
	// deinitialize visualization
	WmpMod.SAVSADeInit();
	
	if (mainDLoad != NULL)
	{
		mainPlayer.Stop();
		mainPlayer.UnloadFile();
		DataLoader_Deinit(mainDLoad);	mainDLoad = NULL;
	}
	
	if (pluginCfg.genOpts.resetMuting)
	{
		for (size_t curChp = 0; curChp < pluginCfg.chipOpts.size(); curChp ++)
		{
			for (size_t curCSet = 0; curCSet < 2; curCSet ++)
			{
				ChipOptions& cOpt = pluginCfg.chipOpts[curChp][curCSet];
				if (cOpt.chipType == 0xFF)
					continue;
				cOpt.chipDisable = 0x00;
				cOpt.muteMask[0] = 0x00;
				cOpt.muteMask[1] = 0x00;
				ApplyCfg_Chip(mainPlayer, pluginCfg.genOpts, cOpt);
			}
		}
		
		// that would just make the check boxes flash during track change
		// and Winamp's main window is locked when the configuration is open
		// so I do it only when muting is reset
		Dialogue_TrackChange();
	}
	
	fileNamePlaying.clear();
	fisMain->ClearSongInfo();
	
	InStopFunc = false;
	return;
}


int GetPlrSongLengthMS(const PlayerA& plr)
{
	const PlayerBase* player = plr.GetPlayer();
	if (player == NULL)
		return -1000;	// nothing is playing right now
	
	double songTime = plr.GetTotalTime(1);
	if (songTime < 0.0)
		return -1000;	// for infinite loops
	
	// for looping songs, add fade time
	if (player->GetLoopTicks() > 0)
		songTime += player->Sample2Second(plr.GetFadeSamples());
	
	// Note: I do *not* include the "pause after song end" here. This is intentional.
	
	return (int)(songTime * 1000 + 0.5);
}

int GetPlrSongLengthMS(const FileInfoStorage::SongInfo& songInfo)
{
	if (songInfo.fileSize == 0)
		return -1000;	// nothing is playing right now
	if (songInfo.looping && pluginCfg.genOpts.maxLoops == 0)
		return -1000;	// infinite loop
	
	double songTime = songInfo.songLenL;
	
	// for looping songs, add fade time
	if (songInfo.looping)
		songTime += pluginCfg.genOpts.fadeTime / 1000.0;	// use fade time from options
	
	// Note: I do *not* include the "pause after song end" here. This is intentional.
	
	return (int)(songTime * 1000 + 0.5);
}

int GetLength(void)	// return length of playing track
{
	return GetPlrSongLengthMS(mainPlayer);
}


// returns current output position, in ms.
// you could just use return mod.outMod->GetOutputTime(),
// but the dsp plug-ins that do tempo changing tend to make that wrong.
static int GetOutputTime(void)
{
	if (seek_needed != -1)
		return seek_needed;	// seeking in progress - return destination time
	return decode_pos_ms + (WmpMod.outMod->GetOutputTime() - WmpMod.outMod->GetWrittenTime());
}

static void SetOutputTime(int time_in_ms)	// for seeking
{
	// Winamp seems to send negative seeking values, when GetLength returns -1000.
	if (time_in_ms < 0)
		return;
	
	seek_needed = time_in_ms;
	return;
}

static void SetVolume(int volume)
{
	WmpMod.outMod->SetVolume(volume);
	return;
}

static void SetPan(int pan)
{
	WmpMod.outMod->SetPan(pan);
	return;
}

const char* GetIniFilePath(void)
{
	return iniFilePath.c_str();
}

FileInfoStorage* GetMainPlayerFIS(void)
{
	return fisMain;
}

void RefreshPlaybackOptions(void)
{
	INT32 volume = (INT32)(0x10000 * fisMain->_songInfo.volGain * pluginCfg.genOpts.volume + 0.5);
	mainPlayer.SetMasterVolume(volume);
	
	return;
}

void RefreshMuting(void)
{
	const std::vector<PlayerBase*>& plrs = mainPlayer.GetRegisteredPlayers();
	
	for (size_t curChp = 0; curChp < pluginCfg.chipOpts.size(); curChp ++)
	{
		for (size_t curCSet = 0; curCSet < 2; curCSet ++)
		{
			const ChipOptions& cOpts = pluginCfg.chipOpts[curChp][curCSet];
			PLR_MUTE_OPTS muteOpts;
			UINT32 devID = PLR_DEV_ID(cOpts.chipType, cOpts.chipInstance);
			muteOpts.disable = cOpts.chipDisable;
			muteOpts.chnMute[0] = cOpts.muteMask[0];
			muteOpts.chnMute[1] = cOpts.muteMask[1];
			
			for (size_t curPlr = 0; curPlr < plrs.size(); curPlr ++)
				plrs[curPlr]->SetDeviceMuting(devID, muteOpts);
		}
	}
	
	return;
}

void RefreshPanning(void)
{
	const std::vector<PlayerBase*>& plrs = mainPlayer.GetRegisteredPlayers();
	
	for (size_t curChp = 0; curChp < pluginCfg.chipOpts.size(); curChp ++)
	{
		for (size_t curCSet = 0; curCSet < 2; curCSet ++)
		{
			const ChipOptions& cOpts = pluginCfg.chipOpts[curChp][curCSet];
			UINT32 devID = PLR_DEV_ID(cOpts.chipType, cOpts.chipInstance);
			
			for (size_t curPlr = 0; curPlr < plrs.size(); curPlr ++)
			{
				PlayerBase* pBase = plrs[curPlr];
				PLR_DEV_OPTS devOpts;
				UINT8 retVal = pBase->GetDeviceOptions(devID, devOpts);
				if (retVal)
					continue;	// this player doesn't support this chip
				
				for (size_t curInst = 0; curInst < 2; curInst ++)
				{
					devOpts.muteOpts.chnMute[curInst] = cOpts.muteMask[curInst];
					for (size_t curChn = 0; curChn < 32; curChn ++)
						devOpts.panOpts.chnPan[curInst][curChn] = cOpts.panMask[curInst][curChn];
				}
				pBase->SetDeviceOptions(devID, devOpts);
			}
		}
	}
	
	return;
}

void UpdatePlayback(void)
{
	if (WmpMod.outMod == NULL || ! WmpMod.outMod->IsPlaying())
		return;
	
	if (pluginCfg.genOpts.immediateUpdate)
	{
		// add 30 ms - else it sounds like you seek back a little
		SetOutputTime(GetOutputTime() + 30);
	}
	
	return;
}


static int InfoDialog(const in_char* fileName, HWND hWnd)
{
	FileInfoDlg_LoadFile(fileName);
	return FileInfoDlg_Show(WmpMod.hDllInstance, hWnd);
}

// This is an odd function. It is used to get the title and/or length of a track.
// If filename is either NULL or of length 0, it means you should
// return the info of LastFileName. Otherwise, return the information
// for the file in filename.
static void GetFileInfo(const in_char* fileName, in_char* title, int* length_in_ms)
{
	FileInfoStorage* fis = NULL;
	
	//printf("GetFileInfo(\"%s\")\n", fileName);
	// Note: If fileName is be NULL or empty, return info about the current file.
	if (fileName == NULL || fileName[0x00] == '\0' || fileNamePlaying == fileName)
	{
		fis = fisMain;
	}
	else
	{
		UINT8 retVal = fisScan->LoadSong(fileName, false);
		if (retVal < 0x80)
			fis = fisScan;
	}
	
	if (length_in_ms != NULL)
	{
		if (fis == NULL)
			*length_in_ms = -1000;	// File not found
		else if (fis == fisMain)
			*length_in_ms = GetPlrSongLengthMS(mainPlayer);	// return based on "playing" setting
		else
			*length_in_ms = GetPlrSongLengthMS(fis->_songInfo);	// new file: use songInfo + fading settings from options
		//printf("    Length: %u ms\n", *length_in_ms);
	}
	
	if (title != NULL) // get non-path portion of fileName
	{
		if (fis != NULL && ! fis->_songInfo.tags.empty())
		{
			std::string titleU8 = FormatVGMTag("%t (%g) - %a", *fis);
			size_t titleSize = GETFILEINFO_TITLE_LENGTH - sizeof(in_char);
#ifndef UNICODE_INPUT_PLUGIN
			CPConv_StrConvert(cpcU8_ACP, &titleSize, &title,
							titleU8.length(), titleU8.c_str());
			title[titleSize] = '\0';
#else
			CPConv_StrConvert(cpcU8_Wide, &titleSize, reinterpret_cast<char**>(&title),
							titleU8.length(), titleU8.c_str());
			title[titleSize / sizeof(wchar_t)] = L'\0';
#endif
		}
		else
		{
			const in_char* fName = (fis == fisMain) ? fileNamePlaying.c_str() : fileName;
#ifdef UNICODE_INPUT_PLUGIN
			wcsncpy(title, GetFileTitle(fName), GETFILEINFO_TITLE_LENGTH);
#else
			strncpy(title, GetFileTitle(fName), GETFILEINFO_TITLE_LENGTH);
#endif
		}
		//printf("    Title: %s|#\n", title);
	}
	
	return;
}

static void EQ_Set(int on, char data[10], int preamp)
{
	return;	// unsupported
}


static DWORD WINAPI DecodeThread(LPVOID b)
{
	char smplBuffer[BLOCK_SIZE * 2];	// has to be twice as big as the blocksize
	
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	
	while(! killDecodeThread) 
	{
		if (seek_needed != -1)	// requested seeking?
		{
			decode_pos = MulDivRound(seek_needed, pluginCfg.genOpts.smplRate, 1000);
			//decode_pos_ms = MulDivRound(decode_pos, 1000, pluginCfg.genOpts.smplRate);
			decode_pos_ms = seek_needed;
			
			mainPlayer.Seek(PLAYPOS_SAMPLE, decode_pos);
			
			// flush output device and set output to seek position
			WmpMod.outMod->Flush(decode_pos_ms);
			seek_needed = -1;
		}
		
		if (EndPlay)
		{
			// Playback finished
			WmpMod.outMod->CanWrite();	// needed for some output drivers
			
			if (! WmpMod.outMod->IsPlaying())
			{
				// we're done playing, so tell Winamp and quit the thread.
				PostMessage(WmpMod.hMainWindow, WM_WA_MPEG_EOF, 0, 0);
				break;
			}
			Sleep(10);		// give a little CPU time back to the system.
		}
		else if (WmpMod.outMod->CanWrite() >= (BLOCK_SIZE * (WmpMod.dsp_isactive() ? 2 : 1)))
			// CanWrite() returns the number of bytes you can write, so we check that
			// to the block size. the reason we multiply the block size by two if 
			// mod.dsp_isactive() is that DSP plug-ins can change it by up to a 
			// factor of two (for tempo adjustment).
		{
			UINT32 renderedBytes = mainPlayer.Render(BLOCK_SIZE, smplBuffer);
			if (renderedBytes)
			{
				UINT32 RetSamples = renderedBytes / SMPL_BYTES;
				// send data to the visualization and dsp systems
				WmpMod.SAAddPCMData(smplBuffer, NUM_CHN, BIT_PER_SEC, decode_pos_ms);
				WmpMod.VSAAddPCMData(smplBuffer, NUM_CHN, BIT_PER_SEC, decode_pos_ms);
				if (WmpMod.dsp_isactive())
					RetSamples = WmpMod.dsp_dosamples((short*)smplBuffer, RetSamples,
														BIT_PER_SEC, NUM_CHN, pluginCfg.genOpts.smplRate);
				
				WmpMod.outMod->Write(smplBuffer, RetSamples * SMPL_BYTES);
				
				decode_pos += RetSamples;
				decode_pos_ms = MulDivRound(decode_pos, 1000, pluginCfg.genOpts.smplRate);
			}
		}
		else
		{
			Sleep(20);	// Wait a little, until we can write again.
		}
	}
	
	return 0;
}

static UINT8 FilePlayCallback(PlayerBase* player, void* userParam, UINT8 evtType, void* evtParam)
{
	switch(evtType)
	{
	case PLREVT_END:
		EndPlay = true;
		break;
	}
	return 0x00;
}

static DATA_LOADER* PlayerFileReqCallback(void* userParam, PlayerBase* player, const char* fileName)
{
	std::string filePath = FindFile_Single(fileName, appSearchPaths);
	if (filePath.empty())
	{
		fprintf(stderr, "Unable to find %s!\n", fileName);
		return NULL;
	}
	//fprintf(stderr, "Player requested file - found at %s\n", filePath.c_str());

	DATA_LOADER* dLoad = FileLoader_Init(filePath.c_str());
	UINT8 retVal = DataLoader_Load(dLoad);
	if (! retVal)
		return dLoad;
	DataLoader_Deinit(dLoad);
	return NULL;
}


// module definition
static In_Module WmpMod =
{
	IN_VER,	// defined in IN2.H
	INVGM_TITLE_FULL,
	NULL,	// hMainWindow (filled in by winamp)
	NULL,	// hDllInstance (filled in by winamp)
	// Format List: "EXT\0Description\0EXT\0Description\0 ..."
	"vgm;vgz\0VGM Audio Files (*.vgm; *.vgz)\0",	// NOTE: replaced with wmpFileExtList during Init()
	1,	// is_seekable
	IN_MODULE_FLAG_USES_OUTPUT_PLUGIN,
	Config,
	About,
	Init,
	Deinit,
	GetFileInfo,
	InfoDialog,
	IsOurFile,
	Play,
	Pause,
	Unpause,
	IsPaused,
	Stop,
	
	GetLength,
	GetOutputTime,
	SetOutputTime,
	
	SetVolume,
	SetPan,
	
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,	// visualization calls filled in by winamp
	
	NULL, NULL,	// dsp calls filled in by winamp
	
	EQ_Set,
	
	NULL,	// setinfo call filled in by winamp
	
	NULL	// out_mod filled in by winamp
};

// exported symbol. Returns output module.
extern "C" DLL_EXPORT In_Module* winampGetInModule2(void)
{
	return &WmpMod;
}
