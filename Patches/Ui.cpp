#include "Ui.h"

#include "../ElDorito.h"
#include "../ElMacros.h"
#include "../Patch.h"
#include "../Modules/ShowGameUI.h"

extern std::shared_ptr<ShowGameUI> showUI;

namespace
{
	void __fastcall UI_MenuUpdateHook(void* a1, int unused, int menuIdToLoad);
	int UI_ShowHalo3StartMenu(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5);
	char __fastcall UI_Forge_ButtonPressHandlerHook(void* a1, int unused, uint8_t* controllerStruct);
	void LocalizedStringHook();
}

namespace Patches
{
	namespace Ui
	{
		void ApplyAll()
		{
			// English patch
			Patch(0x2333FD, { 0 }).Apply();

			// Update window title patch
			const uint8_t windowData[] = { 0x3A, 0x20, 0x45, 0x6C, 0x20, 0x44, 0x6F, 0x72, 0x69, 0x74, 0x6F, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00 };
			Pointer::Base(0x159C02F).Write(windowData, sizeof(windowData));
			Pointer::Base(0x159C06F).Write(windowData, sizeof(windowData));
			Pointer::Base(0x1EAAEE0).Write(windowData, sizeof(windowData));

			// Fix for leave game button to show H3 pause menu
			Hook(0x3B6826, true, &UI_ShowHalo3StartMenu).Apply();
			Patch::NopFill(Pointer::Base(0x3B6826 + 5), 1);

			// Fix "Network" setting in lobbies (change broken 0x100B7 menuID to 0x100B6)
			Patch(0x6C34B0, { 0xB6 }).Apply();

			// Fix gamepad option in settings (todo: find out why it doesn't detect gamepads
			// and find a way to at least enable pressing ESC when gamepad is enabled)
			Patch::NopFill(Pointer::Base(0x20D7F2), 2);

			// Fix menu update code to include missing mainmenu code
			Hook(0x6DFB73, true, &UI_MenuUpdateHook).Apply();

			// Hacky fix to stop the game crashing when you move selection on UI
			// (todo: find out what's really causing this)
			Patch::NopFill(Pointer::Base(0x569D07), 3);

			// Sorta hacky way of getting game options screen to show when you press X on lobby
			// Replaces the delay/cancel game start functionality, but that doesn't really seem to work anyway
			TODO("find real way of showing the [X] Edit Game Options text, that might enable it to work without patching")
				Patch(0x721B88, { 0x8B, 0xCE, 0xFF, 0x77, 0x10, 0xE8, 0x1E, 0x0A, 0x00, 0x00 }).Apply();
			Patch::NopFill(Pointer::Base(0x721B92), 13);

			// Hook UI vftable's forge menu button handler, so arrow keys can act as bumpers
			// added side effect: analog stick left/right can also navigate through menus
			DWORD temp;
			DWORD temp2;
			VirtualProtect(Pointer(0x169EFD8), 4, PAGE_READWRITE, &temp);
			Pointer(0x169EFD8).Write<uint32_t>((uint32_t)&UI_Forge_ButtonPressHandlerHook);
			VirtualProtect(Pointer(0x169EFD8), 4, temp, &temp2);

			// Remove Xbox Live from the network menu
			Patch::NopFill(Pointer::Base(0x723D85), 0x17);
			Pointer::Base(0x723DA1).Write<uint8_t>(0);
			Pointer::Base(0x723DB8).Write<uint8_t>(1);
			Patch::NopFill(Pointer::Base(0x723DFF), 0x3);
			Pointer::Base(0x723E07).Write<uint8_t>(0);
			Pointer::Base(0x723E1C).Write<uint8_t>(0);

			// Localized string override hook
			Hook(0x11E040, false, LocalizedStringHook).Apply();
		}
	}
}

namespace
{
	void __fastcall UI_MenuUpdateHook(void* a1, int unused, int menuIdToLoad)
	{
		bool shouldUpdate = *(DWORD*)((uint8_t*)a1 + 0x10) >= 0x1E;
		typedef void(__thiscall *UI_MenuUpdateFunc)(void* a1, int menuIdToLoad);
		UI_MenuUpdateFunc menuUpdate = (UI_MenuUpdateFunc)0xADF6E0;
		menuUpdate(a1, menuIdToLoad);

		if (shouldUpdate)
		{
			showUI->DialogStringId = menuIdToLoad;
			showUI->DialogArg1 = 0xFF;
			showUI->DialogFlags = 4;
			showUI->DialogParentStringId = 0x1000D;
			showUI->DialogShow = true;
		}
	}

	int UI_ShowHalo3StartMenu(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		showUI->DialogArg1 = 0;
		showUI->DialogFlags = 4;
		showUI->DialogParentStringId = 0x1000C;
		showUI->DialogStringId = 0x10084;
		showUI->DialogShow = true; // can't call the showUI func in the same tick/thread as scaleform ui stuff

		return 1;
	}

	std::chrono::high_resolution_clock::time_point PrevTime = std::chrono::high_resolution_clock::now();
	char __fastcall UI_Forge_ButtonPressHandlerHook(void* a1, int unused, uint8_t* controllerStruct)
	{
		uint32_t btnCode = *(uint32_t*)(controllerStruct + 0x1C);

		auto CurTime = std::chrono::high_resolution_clock::now();
		auto timeSinceLastAction = std::chrono::duration_cast<std::chrono::milliseconds>(CurTime - PrevTime);

		if (btnCode == 0x12 || btnCode == 0x13)
		{
			if (timeSinceLastAction.count() < 200) // 200ms between button presses otherwise it spams the key
				return 1;

			PrevTime = CurTime;

			if (btnCode == 0x12) // analog left / arrow key left
				*(uint32_t*)(controllerStruct + 0x1C) = 0x5;

			if (btnCode == 0x13) // analog right / arrow key right
				*(uint32_t*)(controllerStruct + 0x1C) = 0x4;
		}

		typedef char(__thiscall *UI_Forge_ButtonPressHandler)(void* a1, void* controllerStruct);
		UI_Forge_ButtonPressHandler buttonHandler = (UI_Forge_ButtonPressHandler)0xAE2180;
		return buttonHandler(a1, controllerStruct);
	}

	bool LocalizedStringHookImpl(int tagIndex, int stringId, wchar_t *outputBuffer)
	{
		const size_t MaxStringLength = 0x400;

		switch (stringId)
		{
		case 0x1010A: // start_new_campaign
		{
			// Get the version string, convert it to uppercase UTF-16, and return it
			std::string version = Utils::Version::GetVersionString();
			std::transform(version.begin(), version.end(), version.begin(), toupper);
			std::wstring unicodeVersion(version.begin(), version.end());
			swprintf(outputBuffer, MaxStringLength, L"ELDEWRITO %s", unicodeVersion.c_str());
			return true;
		}
		}
		return false;
	}

	__declspec(naked) void LocalizedStringHook()
	{
		__asm
		{
			// Run the hook implementation function and fallback to the original if it returned false
			push ebp
			mov ebp, esp
			push[ebp + 0x10]
			push[ebp + 0xC]
			push[ebp + 0x8]
			call LocalizedStringHookImpl
			add esp, 0xC
			test al, al
			jz fallback

			// Don't run the original function
			mov esp, ebp
			pop ebp
			ret

		fallback:
			// Execute replaced code and jump back to original function
			sub esp, 0x800
			mov edx, 0x51E049
			jmp edx
		}
	}
}