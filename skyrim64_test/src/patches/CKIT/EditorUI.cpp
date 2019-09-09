#include "../../common.h"
#include <tbb/concurrent_vector.h>
#include <regex>
#include <Richedit.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include "../../typeinfo/ms_rtti.h"
#include "../../typeinfo/hk_rtti.h"
#include "EditorUI.h"

#pragma comment(lib, "comctl32.lib")

HWND g_MainHwnd;
HWND g_LogHwnd;
tbb::concurrent_vector<const char *> g_LogPendingMessages;
HMENU g_ExtensionMenu;
WNDPROC OldEditorUI_WndProc;
HANDLE g_LogPipeReader;
HANDLE g_LogPipeWriter;
FILE *g_LogFile;
void ExportTest(FILE *File);

void EditorUI_Initialize()
{
	InitCommonControls();
	LoadLibraryA("MSFTEDIT.dll");

	if (!EditorUI_CreateLogWindow())
		MessageBoxA(nullptr, "Failed to create console log window", "Error", MB_ICONERROR);

	if (g_INI.GetBoolean("CreationKit", "FaceFXDebugOutput", false))
	{
		if (!EditorUI_CreateStdoutListener())
			MessageBoxA(nullptr, "Failed to create output listener for external processes", "Error", MB_ICONERROR);
	}
}

bool EditorUI_CreateLogWindow()
{
	// File output
	const std::string logPath = g_INI.Get("CreationKit_Log", "OutputFile", "none");

	if (strcmp(logPath.c_str(), "none") != 0)
	{
		if (fopen_s(&g_LogFile, logPath.c_str(), "w") != 0)
			g_LogFile = nullptr;

		AssertMsgVa(g_LogFile, "Unable to open the log file '%s' for writing. To disable, set the 'OutputFile' INI option to 'none'.", logPath.c_str());

		atexit([]()
		{
			if (g_LogFile)
				fclose(g_LogFile);
		});
	}

	// Window output
	HINSTANCE instance = (HINSTANCE)GetModuleHandle(nullptr);

	WNDCLASSEX wc;
	memset(&wc, 0, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hInstance = instance;
	wc.hIcon = LoadIcon(instance, MAKEINTRESOURCE(0x13E));
	wc.hIconSm = wc.hIcon;
	wc.lpfnWndProc = EditorUI_LogWndProc;
	wc.lpszClassName = TEXT("RTEDITLOG");
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

	if (!RegisterClassEx(&wc))
		return false;

	g_LogHwnd = CreateWindowEx(0, TEXT("RTEDITLOG"), TEXT("Log"), WS_OVERLAPPEDWINDOW, 64, 64, 1024, 480, nullptr, nullptr, instance, nullptr);

	if (!g_LogHwnd)
		return false;

	// Poll every 100ms for new lines
	SetTimer(g_LogHwnd, UI_LOG_CMD_ADDTEXT, 100, nullptr);
	ShowWindow(g_LogHwnd, SW_SHOW);
	UpdateWindow(g_LogHwnd);

	return true;
}

bool EditorUI_CreateExtensionMenu(HWND MainWindow, HMENU MainMenu)
{
	// Create extended menu options
	g_ExtensionMenu = CreateMenu();

	BOOL result = TRUE;
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_STRING, (UINT_PTR)UI_EXTMENU_SHOWLOG, "Show Log");
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_STRING, (UINT_PTR)UI_EXTMENU_CLEARLOG, "Clear Log");
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_STRING | MF_CHECKED, (UINT_PTR)UI_EXTMENU_AUTOSCROLL, "Autoscroll Log");
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_SEPARATOR, (UINT_PTR)UI_EXTMENU_SPACER, "");
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_STRING, (UINT_PTR)UI_EXTMENU_DUMPRTTI, "Dump RTTI Data");
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_STRING, (UINT_PTR)UI_EXTMENU_DUMPNIRTTI, "Dump NiRTTI Data");
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_STRING, (UINT_PTR)UI_EXTMENU_DUMPHAVOKRTTI, "Dump Havok RTTI Data");
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_STRING, (UINT_PTR)UI_EXTMENU_LOADEDESPINFO, "Dump Active Forms");
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_SEPARATOR, (UINT_PTR)UI_EXTMENU_SPACER, "");
	result = result && InsertMenu(g_ExtensionMenu, -1, MF_BYPOSITION | MF_STRING, (UINT_PTR)UI_EXTMENU_HARDCODEDFORMS, "Save Hardcoded Forms");

	MENUITEMINFO menuInfo;
	memset(&menuInfo, 0, sizeof(MENUITEMINFO));
	menuInfo.cbSize = sizeof(MENUITEMINFO);
	menuInfo.fMask = MIIM_SUBMENU | MIIM_ID | MIIM_STRING;
	menuInfo.hSubMenu = g_ExtensionMenu;
	menuInfo.wID = UI_EXTMENU_ID;
	menuInfo.dwTypeData = "Extensions";
	menuInfo.cch = (uint32_t)strlen(menuInfo.dwTypeData);
	result = result && InsertMenuItem(MainMenu, -1, TRUE, &menuInfo);

	AssertMsg(result, "Failed to create extension submenu");
	return result != FALSE;
}

bool EditorUI_CreateStdoutListener()
{
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = nullptr;

	if (!CreatePipe(&g_LogPipeReader, &g_LogPipeWriter, &saAttr, 0))
		return false;

	// Ensure the read handle to the pipe for STDOUT is not inherited
	if (!SetHandleInformation(g_LogPipeReader, HANDLE_FLAG_INHERIT, 0))
		return false;

	std::thread pipeReader([]()
	{
		char logBuffer[16384];
		memset(logBuffer, 0, sizeof(logBuffer));

		while (true)
		{
			char buffer[4096];
			memset(buffer, 0, sizeof(buffer));

			DWORD bytesRead;
			bool succeeded = ReadFile(g_LogPipeReader, buffer, ARRAYSIZE(buffer) - 1, &bytesRead, nullptr) != 0;

			// Bail if there's nothing left or the process exited
			if (!succeeded || bytesRead <= 0)
				break;

			strcat_s(logBuffer, buffer);

			// Flush on every newline and skip empty/whitespace strings
			while (char *end = strchr(logBuffer, '\n'))
			{
				*end = '\0';
				size_t len = (size_t)(end - logBuffer);

				while (strchr(logBuffer, '\r'))
					*strchr(logBuffer, '\r') = ' ';

				if (len > 0 && (len > 1 || logBuffer[0] != ' '))
					EditorUI_Warning(6, "%s", logBuffer);

				strcpy_s(logBuffer, end + 1);
			}
		}

		CloseHandle(g_LogPipeReader);
		CloseHandle(g_LogPipeWriter);
	});

	pipeReader.detach();
	return true;
}

HANDLE EditorUI_GetStdoutListenerPipe()
{
	return g_LogPipeWriter;
}

HWND EditorUI_GetMainWindow()
{
	return g_MainHwnd;
}

LRESULT CALLBACK EditorUI_WndProc(HWND Hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (Message == WM_CREATE)
	{
		const CREATESTRUCT *createInfo = (CREATESTRUCT *)lParam;

		if (!_stricmp(createInfo->lpszName, "Creation Kit") && !_stricmp(createInfo->lpszClass, "Creation Kit"))
		{
			// Initialize the original window before adding anything
			LRESULT status = CallWindowProc(OldEditorUI_WndProc, Hwnd, Message, wParam, lParam);
			g_MainHwnd = Hwnd;

			// Increase status bar spacing
			int spacing[4] =
			{
				200,	// 150
				300,	// 225
				700,	// 500
				-1,		// -1
			};

			SendMessageA(GetDlgItem(Hwnd, UI_EDITOR_STATUSBAR), SB_SETPARTS, ARRAYSIZE(spacing), (LPARAM)& spacing);

			// Grass is always enabled by default, make the UI buttons match
			CheckMenuItem(GetMenu(Hwnd), UI_EDITOR_TOGGLEGRASS, MF_CHECKED);
			SendMessageA(GetDlgItem(Hwnd, UI_EDITOR_TOOLBAR), TB_CHECKBUTTON, UI_EDITOR_TOGGLEGRASS_BUTTON, TRUE);

			// Same for fog
			CheckMenuItem(GetMenu(Hwnd), UI_EDITOR_TOGGLEFOG, *(bool *)(g_ModuleBase + 0x4F05728) ? MF_CHECKED : MF_UNCHECKED);

			// Create custom menu controls
			EditorUI_CreateExtensionMenu(Hwnd, createInfo->hMenu);

			return status;
		}
	}
	else if (Message == WM_COMMAND)
	{
		const uint32_t param = LOWORD(wParam);

		switch (param)
		{
		case UI_EDITOR_TOGGLEFOG:
		{
			// Call the CTRL+F5 hotkey function directly
			((void(__fastcall *)())(g_ModuleBase + 0x1319740))();
		}
		return 0;

		case UI_EXTMENU_SHOWLOG:
		{
			ShowWindow(g_LogHwnd, SW_SHOW);
			SetForegroundWindow(g_LogHwnd);
		}
		return 0;

		case UI_EXTMENU_CLEARLOG:
		{
			PostMessageA(g_LogHwnd, UI_LOG_CMD_CLEARTEXT, 0, 0);
		}
		return 0;

		case UI_EXTMENU_AUTOSCROLL:
		{
			MENUITEMINFO info;
			info.cbSize = sizeof(MENUITEMINFO);
			info.fMask = MIIM_STATE;
			GetMenuItemInfo(g_ExtensionMenu, param, FALSE, &info);

			bool check = !((info.fState & MFS_CHECKED) == MFS_CHECKED);

			if (!check)
				info.fState &= ~MFS_CHECKED;
			else
				info.fState |= MFS_CHECKED;

			PostMessageA(g_LogHwnd, UI_LOG_CMD_AUTOSCROLL, (WPARAM)check, 0);
			SetMenuItemInfo(g_ExtensionMenu, param, FALSE, &info);
		}
		return 0;

		case UI_EXTMENU_DUMPRTTI:
		case UI_EXTMENU_DUMPNIRTTI:
		case UI_EXTMENU_DUMPHAVOKRTTI:
		case UI_EXTMENU_LOADEDESPINFO:
		{
			char filePath[MAX_PATH];
			memset(filePath, 0, sizeof(filePath));

			OPENFILENAME ofnData;
			memset(&ofnData, 0, sizeof(OPENFILENAME));
			ofnData.lStructSize = sizeof(OPENFILENAME);
			ofnData.lpstrFilter = "Text Files (*.txt)\0*.txt\0\0";
			ofnData.lpstrFile = filePath;
			ofnData.nMaxFile = ARRAYSIZE(filePath);
			ofnData.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
			ofnData.lpstrDefExt = "txt";

			if (FILE *f; GetSaveFileName(&ofnData) && fopen_s(&f, filePath, "w") == 0)
			{
				if (param == UI_EXTMENU_DUMPRTTI)
					MSRTTI::Dump(f);
				else if (param == UI_EXTMENU_DUMPNIRTTI)
					ExportTest(f);
				else if (param == UI_EXTMENU_DUMPHAVOKRTTI)
				{
					// Convert path to directory
					*strrchr(filePath, '\\') = '\0';
					HKRTTI::DumpReflectionData(filePath);
				}
				else if (param == UI_EXTMENU_LOADEDESPINFO)
				{
					struct VersionControlListItem
					{
						const char *EditorId;
						uint32_t FileOffset;
						char Type[4];
						uint32_t FileLength;
						char GroupType[4];
						uint32_t FormId;
						uint32_t VersionControlId;
						char _pad0[0x8];
					};
					static_assert_offset(VersionControlListItem, EditorId, 0x0);
					static_assert_offset(VersionControlListItem, FileOffset, 0x8);
					static_assert_offset(VersionControlListItem, Type, 0xC);
					static_assert_offset(VersionControlListItem, FileLength, 0x10);
					static_assert_offset(VersionControlListItem, GroupType, 0x14);
					static_assert_offset(VersionControlListItem, FormId, 0x18);
					static_assert_offset(VersionControlListItem, VersionControlId, 0x1C);
					static_assert(sizeof(VersionControlListItem) == 0x28);

					static std::vector<VersionControlListItem> formList;

					// Invoke the dialog, building form list
					void(*callback)(void *, int, VersionControlListItem *) = [](void *, int, VersionControlListItem *Data)
					{
						formList.push_back(*Data);
						formList.back().EditorId = _strdup(Data->EditorId);
					};

					XUtil::DetourCall(g_ModuleBase + 0x13E32B0, callback);
					CallWindowProcA((WNDPROC)(g_ModuleBase + 0x13E6270), Hwnd, WM_COMMAND, 1185, 0);

					// Sort by: form id, then name, then file offset
					std::sort(formList.begin(), formList.end(),
						[](const VersionControlListItem& A, const VersionControlListItem& B) -> bool
					{
						if (A.FormId == B.FormId)
						{
							if (int ret = _stricmp(A.EditorId, B.EditorId); ret != 0)
								return ret < 0;

							return A.FileOffset > B.FileOffset;
						}

						return A.FormId > B.FormId;
					});

					// Dump it to the log
					fprintf(f, "Type, Editor Id, Form Id, File Offset, File Length, Version Control Id\n");

					for (auto& item : formList)
					{
						fprintf(f, "%c%c%c%c,\"%s\",%08X,%u,%u,-%08X-\n",
							item.Type[0], item.Type[1], item.Type[2], item.Type[3],
							item.EditorId,
							item.FormId,
							item.FileOffset,
							item.FileLength,
							item.VersionControlId);

						free((void *)item.EditorId);
					}

					formList.clear();
				}

				fclose(f);
			}
		}
		return 0;

		case UI_EXTMENU_HARDCODEDFORMS:
		{
			auto getFormById = (__int64(__fastcall *)(uint32_t))(g_ModuleBase + 0x16B8780);

			for (uint32_t i = 0; i < 2048; i++)
			{
				__int64 form = getFormById(i);

				if (form)
				{
					(*(void(__fastcall **)(__int64, __int64))(*(__int64 *)form + 360))(form, 1);
					EditorUI_Log("SetFormModified(%08X)", i);
				}
			}

			// Fake the click on "Save"
			PostMessageA(Hwnd, WM_COMMAND, 40127, 0);
		}
		return 0;
		}
	}
	else if (Message == WM_SETTEXT && Hwnd == g_MainHwnd)
	{
		// Continue normal execution but with a custom string
		char customTitle[1024];
		sprintf_s(customTitle, "%s [CK64Fixes Rev. %s]", (const char *)lParam, g_GitVersion);

		return CallWindowProc(OldEditorUI_WndProc, Hwnd, Message, wParam, (LPARAM)customTitle);
	}

	return CallWindowProc(OldEditorUI_WndProc, Hwnd, Message, wParam, lParam);
}

LRESULT CALLBACK EditorUI_LogWndProc(HWND Hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static HWND richEditHwnd;
	static bool autoScroll;

	switch (Message)
	{
	case WM_CREATE:
	{
		const CREATESTRUCT *info = (CREATESTRUCT *)lParam;

		// Create the rich edit control (https://docs.microsoft.com/en-us/windows/desktop/Controls/rich-edit-controls)
		uint32_t style = WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_LEFT | ES_NOHIDESEL | ES_AUTOVSCROLL | ES_READONLY;

		richEditHwnd = CreateWindowExW(0, MSFTEDIT_CLASS, L"", style, 0, 0, info->cx, info->cy, Hwnd, nullptr, info->hInstance, nullptr);
		autoScroll = true;

		if (!richEditHwnd)
			return -1;

		// Set default position
		int winW = g_INI.GetInteger("CreationKit_Log", "Width", info->cx);
		int winH = g_INI.GetInteger("CreationKit_Log", "Height", info->cy);

		MoveWindow(Hwnd, info->x, info->y, winW, winH, FALSE);

		// Set a better font
		CHARFORMAT2A format;
		memset(&format, 0, sizeof(format));

		// Convert Twips to points (1 point = 20 Twips)
		int pointSize = g_INI.GetInteger("CreationKit_Log", "FontSize", 10) * 20;

		format.cbSize = sizeof(format);
		format.dwMask = CFM_FACE | CFM_SIZE | CFM_WEIGHT;
		format.yHeight = pointSize;
		strcpy_s(format.szFaceName, g_INI.Get("CreationKit_Log", "Font", "Consolas").c_str());
		format.wWeight = (WORD)g_INI.GetInteger("CreationKit_Log", "FontWeight", FW_NORMAL);
		SendMessageA(richEditHwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);

		// Subscribe to EN_MSGFILTER and EN_SELCHANGE
		SendMessageA(richEditHwnd, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS | ENM_SELCHANGE);
	}
	return 0;

	case WM_DESTROY:
		DestroyWindow(richEditHwnd);
		return 0;

	case WM_SIZE:
	{
		int w = LOWORD(lParam);
		int h = HIWORD(lParam);
		MoveWindow(richEditHwnd, 0, 0, w, h, TRUE);
	}
	break;

	case WM_ACTIVATE:
	{
		if (wParam != WA_INACTIVE)
			SetFocus(richEditHwnd);
	}
	return 0;

	case WM_CLOSE:
		ShowWindow(Hwnd, SW_HIDE);
		return 0;

	case WM_NOTIFY:
	{
		static uint64_t lastDoubleClickTime;
		LPNMHDR notification = (LPNMHDR)lParam;

		if (notification->code == EN_MSGFILTER)
		{
			auto msgFilter = (MSGFILTER *)notification;

			if (msgFilter->msg == WM_LBUTTONDBLCLK)
				lastDoubleClickTime = GetTickCount64();
		}
		else if (notification->code == EN_SELCHANGE)
		{
			auto selChange = (SELCHANGE *)lParam;

			// Mouse double click with a valid selection -> try to parse form id
			if ((GetTickCount64() - lastDoubleClickTime > 1000) || abs(selChange->chrg.cpMax - selChange->chrg.cpMin) <= 0)
				break;

			if (selChange->chrg.cpMin == 0 && selChange->chrg.cpMax == -1)
				break;

			// Get the line number from the selected range
			LRESULT lineIndex = SendMessageA(richEditHwnd, EM_LINEFROMCHAR, selChange->chrg.cpMin, 0);

			char lineData[4096];
			*(uint16_t *)&lineData[0] = ARRAYSIZE(lineData);

			LRESULT charCount = SendMessageA(richEditHwnd, EM_GETLINE, lineIndex, (LPARAM)&lineData);

			if (charCount > 0)
			{
				lineData[charCount - 1] = '\0';

				// Capture each form id with regex "(XXXXXXXX)"
				static const std::regex formIdRegex("\\(([1234567890abcdefABCDEF]*?)\\)");
				std::smatch sm;

				for (std::string line = lineData; std::regex_search(line, sm, formIdRegex); line = sm.suffix())
				{
					// Parse to integer, then bring up the menu
					uint32_t id = strtoul(sm[1].str().c_str(), nullptr, 16);

					auto GetFormById = (__int64(__fastcall *)(uint32_t))(g_ModuleBase + 0x16B8780);
					__int64 form = GetFormById(id);

					if (form)
						(*(void(__fastcall **)(__int64, HWND, __int64, __int64))(*(__int64 *)form + 720i64))(form, g_MainHwnd, 0, 1);
				}
			}
		}
	}
	break;

	case WM_TIMER:
	{
		if (wParam != UI_LOG_CMD_ADDTEXT)
			break;

		if (g_LogPendingMessages.size() <= 0)
			break;

		return EditorUI_LogWndProc(Hwnd, UI_LOG_CMD_ADDTEXT, 0, 0);
	}
	return 0;

	case UI_LOG_CMD_ADDTEXT:
	{
		SendMessageA(richEditHwnd, WM_SETREDRAW, FALSE, 0);

		// Save old position if not scrolling
		POINT scrollRange;

		if (!autoScroll)
			SendMessageA(richEditHwnd, EM_GETSCROLLPOS, 0, (WPARAM)&scrollRange);

		// Get a copy of all elements and clear the global list
		tbb::concurrent_vector<const char *> messages;

		if (!wParam)
			messages.swap(g_LogPendingMessages);
		else
			messages.push_back((const char *)wParam);

		for (const char *message : messages)
		{
			// Move caret to the end, then write
			CHARRANGE range;
			range.cpMin = LONG_MAX;
			range.cpMax = LONG_MAX;

			SendMessageA(richEditHwnd, EM_EXSETSEL, 0, (LPARAM)&range);
			SendMessageA(richEditHwnd, EM_REPLACESEL, FALSE, (LPARAM)message);

			free((void *)message);
		}

		if (!autoScroll)
			SendMessageA(richEditHwnd, EM_SETSCROLLPOS, 0, (WPARAM)&scrollRange);

		SendMessageA(richEditHwnd, WM_SETREDRAW, TRUE, 0);
		RedrawWindow(richEditHwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_NOCHILDREN);
	}
	return 0;

	case UI_LOG_CMD_CLEARTEXT:
	{
		char emptyString[1];
		emptyString[0] = '\0';

		SendMessageA(richEditHwnd, WM_SETTEXT, 0, (LPARAM)&emptyString);
	}
	return 0;

	case UI_LOG_CMD_AUTOSCROLL:
		autoScroll = (bool)wParam;
		return 0;
	}

	return DefWindowProc(Hwnd, Message, wParam, lParam);
}

LRESULT CALLBACK EditorUI_DialogTabProc(HWND Hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (Message == WM_INITDIALOG)
	{
		// If it's the weapon sound dialog tab (id 3327), remap the "Unequip Sound" button (id 3682) to
		// a non-conflicting one (id 3683)
		char className[256];

		if (GetClassNameA(Hwnd, className, ARRAYSIZE(className)) > 0)
		{
			if (!strcmp(className, "WeaponClass"))
				SetWindowLongPtr(GetDlgItem(Hwnd, 3682), GWLP_ID, 3683);
		}

		ShowWindow(Hwnd, SW_HIDE);
		return 1;
	}

	return 0;
}

INT_PTR CALLBACK EditorUI_LipRecordDialogProc(HWND DialogHwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	// Id's for "Recording..." dialog window
	switch (Message)
	{
	case WM_APP:
		// Don't actually kill the dialog, just hide it. It gets destroyed later when the parent window closes.
		SendMessageA(GetDlgItem(DialogHwnd, 31007), PBM_SETPOS, 0, 0);
		ShowWindow(DialogHwnd, SW_HIDE);
		PostQuitMessage(0);
		return TRUE;

	case 272:
		// OnSaveSoundFile
		SendMessageA(GetDlgItem(DialogHwnd, 31007), PBM_SETRANGE, 0, 32768 * 1000);
		SendMessageA(GetDlgItem(DialogHwnd, 31007), PBM_SETSTEP, 1, 0);
		return TRUE;

	case 273:
		// Stop recording
		if (LOWORD(wParam) != 1)
			return FALSE;

		*(bool *)(g_ModuleBase + 0x3AFAE28) = false;

		if (FAILED(((HRESULT(__fastcall *)(bool))(g_ModuleBase + 0x13D5310))(false)))
			MessageBoxA(DialogHwnd, "Error with DirectSoundCapture buffer.", "DirectSound Error", MB_ICONERROR);

		return EditorUI_LipRecordDialogProc(DialogHwnd, WM_APP, 0, 0);

	case 1046:
		// Start recording
		ShowWindow(DialogHwnd, SW_SHOW);
		*(bool *)(g_ModuleBase + 0x3AFAE28) = true;

		if (FAILED(((HRESULT(__fastcall *)(bool))(g_ModuleBase + 0x13D5310))(true)))
		{
			MessageBoxA(DialogHwnd, "Error with DirectSoundCapture buffer.", "DirectSound Error", MB_ICONERROR);
			return EditorUI_LipRecordDialogProc(DialogHwnd, WM_APP, 0, 0);
		}
		return TRUE;
	}

	return FALSE;
}

LRESULT EditorUI_CSScript_PickScriptsToCompileDlgProc(void *Thisptr, UINT Message, WPARAM wParam, LPARAM lParam)
{
	thread_local bool disableListViewUpdates;

	auto updateListViewItems = [Thisptr]
	{
		if (!disableListViewUpdates)
			((void(__fastcall *)(void *))(g_ModuleBase + 0x20A9870))(Thisptr);
	};

	switch (Message)
	{
	case WM_SIZE:
		((void(__fastcall *)(void *))(g_ModuleBase + 0x20A9CF0))(Thisptr);
		break;

	case WM_NOTIFY:
	{
		LPNMHDR notification = (LPNMHDR)lParam;

		// "SysListView32" control
		if (notification->idFrom == 5401 && notification->code == LVN_ITEMCHANGED)
		{
			updateListViewItems();
			return 1;
		}
	}
	break;

	case WM_INITDIALOG:
		disableListViewUpdates = true;
		((void(__fastcall *)(void *))(g_ModuleBase + 0x20A99C0))(Thisptr);
		disableListViewUpdates = false;

		// Update it ONCE after everything is inserted
		updateListViewItems();
		break;

	case WM_COMMAND:
	{
		const uint32_t param = LOWORD(wParam);

		// "Check All", "Uncheck All", "Check All Checked-Out"
		if (param == 5474 || param == 5475 || param == 5602)
		{
			disableListViewUpdates = true;
			if (param == 5474)
				((void(__fastcall *)(void *))(g_ModuleBase + 0x20AA080))(Thisptr);
			else if (param == 5475)
				((void(__fastcall *)(void *))(g_ModuleBase + 0x20AA130))(Thisptr);
			else if (param == 5602)
				((void(__fastcall *)(void *))(g_ModuleBase + 0x20AA1E0))(Thisptr);
			disableListViewUpdates = false;

			updateListViewItems();
			return 1;
		}
		else if (param == 1)
		{
			// "Compile" button
			((void(__fastcall *)(void *))(g_ModuleBase + 0x20A9F30))(Thisptr);
		}
	}
	break;
	}

	return ((LRESULT(__fastcall *)(void *, UINT, WPARAM, LPARAM))(g_ModuleBase + 0x20ABD90))(Thisptr, Message, wParam, lParam);
}

void EditorUI_LogVa(const char *Format, va_list Va)
{
	char buffer[2048];
	int len = _vsnprintf_s(buffer, _TRUNCATE, Format, Va);

	if (len <= 0)
		return;

	if (len >= 2 && buffer[len - 1] != '\n')
		strcat_s(buffer, "\n");

	if (g_LogFile)
	{
		fputs(buffer, g_LogFile);
		fflush(g_LogFile);
	}

	if (g_LogPendingMessages.size() < 50000)
		g_LogPendingMessages.push_back(_strdup(buffer));
}

void EditorUI_Log(const char *Format, ...)
{
	va_list va;

	va_start(va, Format);
	EditorUI_LogVa(Format, va);
	va_end(va);
}

void EditorUI_Warning(int Type, const char *Format, ...)
{
	static const char *typeList[30] =
	{
		"DEFAULT",
		"COMBAT",
		"ANIMATION",
		"AI",
		"SCRIPTS",
		"SAVELOAD",
		"DIALOGUE",
		"QUESTS",
		"PACKAGES",
		"EDITOR",
		"MODELS",
		"TEXTURES",
		"PLUGINS",
		"MASTERFILE",
		"FORMS",
		"MAGIC",
		"SHADERS",
		"RENDERING",
		"PATHFINDING",
		"MENUS",
		"AUDIO",
		"CELLS",
		"HAVOK",
		"FACEGEN",
		"WATER",
		"INGAME",
		"MEMORY",
		"PERFORMANCE",
		"JOBS",
		"SYSTEM"
	};

	char buffer[2048];
	va_list va;

	va_start(va, Format);
	_vsnprintf_s(buffer, _TRUNCATE, Format, va);
	va_end(va);

	EditorUI_Log("%s: %s", typeList[Type], buffer);
}

void EditorUI_WarningUnknown1(const char *Format, ...)
{
	va_list va;

	va_start(va, Format);
	EditorUI_LogVa(Format, va);
	va_end(va);
}

void EditorUI_WarningUnknown2(__int64 Unused, const char *Format, ...)
{
	va_list va;

	va_start(va, Format);
	EditorUI_LogVa(Format, va);
	va_end(va);
}

void EditorUI_Assert(const char *File, int Line, const char *Message, ...)
{
	if (!Message)
		Message = "<No message>";

	char buffer[2048];
	va_list va;

	va_start(va, Message);
	_vsnprintf_s(buffer, _TRUNCATE, Message, va);
	va_end(va);

	EditorUI_Log("ASSERTION: %s (%s line %d)", buffer, File, Line);
}

BOOL EditorUI_ListViewCustomSetItemState(HWND ListViewHandle, WPARAM Index, UINT Data, UINT Mask)
{
	// Microsoft's implementation of this define is broken (ListView_SetItemState)
	LVITEMA lvi = {};
	lvi.mask = LVIF_STATE;
	lvi.state = Data;
	lvi.stateMask = Mask;

	return (BOOL)SendMessageA(ListViewHandle, LVM_SETITEMSTATE, Index, (LPARAM)&lvi);
}

void EditorUI_ListViewSelectItem(HWND ListViewHandle, int ItemIndex, bool KeepOtherSelections)
{
	if (!KeepOtherSelections)
		EditorUI_ListViewCustomSetItemState(ListViewHandle, -1, 0, LVIS_SELECTED);

	if (ItemIndex != -1)
	{
		ListView_EnsureVisible(ListViewHandle, ItemIndex, FALSE);
		EditorUI_ListViewCustomSetItemState(ListViewHandle, ItemIndex, LVIS_SELECTED, LVIS_SELECTED);
	}
}

void EditorUI_ListViewFindAndSelectItem(HWND ListViewHandle, void *Parameter, bool KeepOtherSelections)
{
	if (!KeepOtherSelections)
		EditorUI_ListViewCustomSetItemState(ListViewHandle, -1, 0, LVIS_SELECTED);

	LVFINDINFOA findInfo;
	memset(&findInfo, 0, sizeof(findInfo));

	findInfo.flags = LVFI_PARAM;
	findInfo.lParam = (LPARAM)Parameter;

	int index = ListView_FindItem(ListViewHandle, -1, &findInfo);

	if (index != -1)
		EditorUI_ListViewSelectItem(ListViewHandle, index, KeepOtherSelections);
}

void EditorUI_ListViewDeselectItem(HWND ListViewHandle, void *Parameter)
{
	LVFINDINFOA findInfo;
	memset(&findInfo, 0, sizeof(findInfo));

	findInfo.flags = LVFI_PARAM;
	findInfo.lParam = (LPARAM)Parameter;

	int index = ListView_FindItem(ListViewHandle, -1, &findInfo);

	if (index != -1)
		EditorUI_ListViewCustomSetItemState(ListViewHandle, index, 0, LVIS_SELECTED);
}