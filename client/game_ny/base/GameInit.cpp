#include "StdInc.h"
#include "GameInit.h"
#include "Text.h"
#include "Hooking.h"
#include "CrossLibraryInterfaces.h"

#include <ICoreGameInit.h>

static bool* g_preventSaveLoading;

bool GameInit::GetGameLoaded()
{
	return !(*(uint8_t*)0xF22B3C);
}

void GameInit::LoadGameFirstLaunch(bool (*callBeforeLoad)())
{
	assert(!GameInit::GetGameLoaded());

	*(DWORD*)0x10C7F80 = 6;

	if (callBeforeLoad)
	{
		static bool(*preLoadCB)();

		preLoadCB = callBeforeLoad;

		g_hooksDLL->SetHookCallback(StringHash("preGmLoad"), [] (void*)
		{
			while (!preLoadCB())
			{
				*(BYTE*)0x18A825C = 0;

				MSG msg;

				while (PeekMessage(&msg, 0, 0, 0, TRUE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		});
	}
}

struct LoadingTune
{
	void StartLoadingTune();
};

void WRAPPER LoadingTune::StartLoadingTune() { EAXJMP(0x7B9980); }

static LoadingTune& loadingTune = *(LoadingTune*)0x10F85B0;
bool& dontProcessTheGame = *(bool*)0x18A8238;

void GameInit::SetLoadScreens()
{
	// disable load screens if they're already ongoing
	hook::put<uint8_t>(0x18A823A, 0);

	((void(*)(int, int, int))0x423CE0)(0, 0, 0);
	//((void(*)())0x423E60)();

	if (GetGameLoaded())
	{
		dontProcessTheGame = true;
		*g_preventSaveLoading = true;
	}

	//*(BYTE*)0x7BD9F0 = 0xC3;
	*(BYTE*)0x18A823A = 1;

	// start at loading screen 7 (main screen 1)
	hook::put<uint32_t>(0x18A8254, 7);

	loadingTune.StartLoadingTune();

	//SetEvent(*(HANDLE*)0x10F9AAC);
}

static bool g_shouldSwitchToCustomLoad;
static rage::grcTexture* g_customLoad;

rage::grcTexture* GameInit::GetLastCustomLoadTexture()
{
	return g_customLoad;
}

void GameInit::PrepareSwitchToCustomLoad(rage::grcTexture* texture)
{
	// set up loading screen '1'
	*(DWORD*)(0x18A8F40 + 400) = INT32_MAX; // duration
	*(DWORD*)(0x18A8F48 + 400) = 0; // flag (was probably LEGAL or so)
	*(DWORD*)(0x18A8F4C + 400) = 1; // Fade : IN_OUT

	// set our local flag too
	g_shouldSwitchToCustomLoad = true;

	g_customLoad = texture;
}

bool GameInit::ShouldSwitchToCustomLoad()
{
	if (g_shouldSwitchToCustomLoad)
	{
		g_shouldSwitchToCustomLoad = false;

		return true;
	}

	return false;
}

void WRAPPER SetTextForLoadscreen(const char* text, bool a2, bool a3, int neg1) { EAXJMP(0x84F580); }
bool& stopNetwork = *(bool*)0x18A82FE;

void WRAPPER __fastcall LeaveGame(void* net) { EAXJMP(0x462370); }

static bool g_ignoreNextGracefulDeath;

void GameInit::KillNetwork(const wchar_t* reason)
{
	// no overkill please
	if (stopNetwork)
	{
		return;
	}

	g_hooksDLL->SetDisconnectSafeguard(false);

	// special case for graceful kill
	if (reason == (wchar_t*)1)
	{
		if (g_ignoreNextGracefulDeath)
		{
			g_ignoreNextGracefulDeath = false;
			return;
		}

		LeaveGame((void*)0x11D6718);
		return;
	}

	if (reason != nullptr)
	{
		TheText.SetCustom("CIT_NET_KILL_REASON", reason);
		SetTextForLoadscreen("CIT_NET_KILL_REASON", true, false, -1);

		static char smallReason[8192];
		wcstombs(smallReason, reason, _countof(smallReason));

		g_ignoreNextGracefulDeath = true;

		g_netLibrary->Disconnect(smallReason);
	}

	stopNetwork = true;
}

bool& reloadGameNextFrame = *(bool*)0x10F8074;
static int tryDisconnectFlag;

void GameInit::ReloadGame()
{
	//((void(*)())0x40ACE0)();
	//((void(*)())0x40B180)();

	reloadGameNextFrame = true;

	tryDisconnectFlag = 2;
}

static void ToggleBackGameProcess()
{
	dontProcessTheGame = false;
	g_preventSaveLoading = false;
}

static void __declspec(naked) ToggleBackGameProcessStub()
{
	__asm
	{
		// preserve ecx and call
		push ecx
		call ToggleBackGameProcess
		pop ecx

		// jump back to original
		push 788380h
		retn
	}
}

void GameInit::MurderGame()
{
	// unload streamed fonts
	((void(*)())0x7F9260)();
}

class CoreGameInit : public ICoreGameInit
{
public:
	virtual bool GetGameLoaded() override
	{
		return GameInit::GetGameLoaded();
	}

	virtual void KillNetwork(const wchar_t* errorString) override
	{
		return GameInit::KillNetwork(errorString);
	}

	virtual bool TryDisconnect()
	{
		if (tryDisconnectFlag > 0)
		{
			tryDisconnectFlag--;
			return false;
		}
		else
		{
			return true;
		}
	}

	virtual void SetPreventSavePointer(bool* preventSaveValue) override
	{
		g_preventSaveLoading = preventSaveValue;
	}
};

static InitFunction initFunction([] ()
{
	// weird jump because of weird random floats? probably for fading anyway...
	hook::nop(0x42107C, 6);

	// some function that resets to/undoes weird loading text state rather than continuing on our nice loading screens (during the 'faux game process' state)
	hook::nop(0x421426, 5);

	// hook to reset processing the game after our load caller finishes
	//hook::jump(0x420FA2, ToggleBackGameProcess);
	hook::call(0x421150, ToggleBackGameProcessStub);

	// unused byte which is set to 0 during loading
	hook::put<uint8_t>(0xF22B3C, 1);

	// LoadGameNow argument 'reload game fully, even if episodes didn't change' in one caller, to be specific the one we actually use indirectly above as the script flag uses it
	/*hook::put<uint8_t>(0x420F91, true);

	// other callers for this stuff
	hook::put<uint8_t>(0x420FD9, 1);
	hook::put<uint8_t>(0x420EAB, 1);
	hook::put<uint8_t>(0x420F33, 1);*/

	// don't load loadscreens at the start
	hook::nop(0x4028CA, 5);

	// don't wait for loadscreens at the start
	hook::put<uint8_t>(0x402B49, 0xEB);

	// always redo game object variables
	//hook::nop(0x4205C5, 2);

	// silly people doing silly things (related to loadGameNow call types)
	hook::nop(0x4210FE, 2);
	hook::nop(0x421108, 2);
	hook::nop(0x420EAE, 6);
	//hook::put<uint8_t>(0x420ECD, 0xEB);
	hook::nop(0x420EF9, 2);

	// create ICoreGameInit instance
	Instance<ICoreGameInit>::Set(new CoreGameInit());
});