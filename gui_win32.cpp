#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#define IDC_EDIT_TEXT      1001
#define IDC_BTN_GENERATE   1002
#define IDC_STATIC_STATUS  1003
#define IDC_COMBO_BANK     1004
#define IDC_BTN_REFRESH    1005
#define IDC_EDIT_OUTNAME   1006
#define IDC_STATIC_BANK    1007
#define IDC_STATIC_OUT     1008


static HWND  g_hEdit      = NULL;
static HWND  g_hButton    = NULL;
static HWND  g_hStatus    = NULL;
static HWND  g_hComboBank = NULL;
static HWND  g_hBtnRefresh= NULL;
static HWND  g_hEditOut   = NULL;
static HWND  g_hLblBank   = NULL;
static HWND  g_hLblOut    = NULL;
static HFONT g_hFont      = NULL;


static const wchar_t* const kExeName32 = L"ZKLEP_Win_i686.exe";
static const wchar_t* const kExeName64 = L"ZKLEP_Win_x86-64.exe";

static const wchar_t* const kSoundsDir = L"sounds";
static const wchar_t* const kOutputDir = L"output";


static bool IsSelf64Bit()
{
#if defined(_WIN64)
	return true;
#else
	return false;
#endif
}

static bool GetExeDir(wchar_t* out, DWORD cch)
{
	DWORD n;
	DWORD i;

	n = GetModuleFileNameW(NULL, out, cch);
	if (n == 0 || n >= cch)
	{
		return false;
	}

	for (i = n; i > 0; --i)
	{
		if (out[i - 1] == L'\\' || out[i - 1] == L'/')
		{
			out[i - 1] = L'\0';

			return true;
		}
	}
	out[0] = L'\0';

	return false;
}

static bool FileExists(const wchar_t* path)
{
	DWORD a = GetFileAttributesW(path);

	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool DirExists(const wchar_t* path)
{
	DWORD a = GetFileAttributesW(path);

	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool PickZklepExe(wchar_t* outPath, DWORD cch)
{
	wchar_t dir[MAX_PATH];
	const wchar_t* first;
	const wchar_t* second;
	wchar_t p[MAX_PATH];
	int r;

	if (!GetExeDir(dir, MAX_PATH))
	{
		return false;
	}

	first  = IsSelf64Bit() ? kExeName64 : kExeName32;
	second = IsSelf64Bit() ? kExeName32 : kExeName64;

	r = _snwprintf(p, MAX_PATH, L"%s\\%s", dir, first);
	if (r > 0 && (size_t)r < MAX_PATH && FileExists(p))
	{
		wcsncpy(outPath, p, (size_t)(cch - 1));
		outPath[cch - 1] = 0;

		return true;
	}

	r = _snwprintf(p, MAX_PATH, L"%s\\%s", dir, second);
	if (r > 0 && (size_t)r < MAX_PATH && FileExists(p))
	{
		wcsncpy(outPath, p, (size_t)(cch - 1));
		outPath[cch - 1] = 0;

		return true;
	}

	return false;
}

static void RefreshBankList()
{
	wchar_t dir[MAX_PATH];
	wchar_t soundsPath[MAX_PATH];
	WIN32_FIND_DATAW fd;
	HANDLE h;
	int count;
	int r;

	if (!g_hComboBank)
	{
		return;
	}

	SendMessageW(g_hComboBank, CB_RESETCONTENT, 0, 0);

	if (!GetExeDir(dir, MAX_PATH))
	{
		return;
	}

	r = _snwprintf(soundsPath, MAX_PATH, L"%s\\%s\\*", dir, kSoundsDir);
	if (r <= 0 || (size_t)r >= MAX_PATH)
	{
		return;
	}

	h = FindFirstFileW(soundsPath, &fd);
	if (h == INVALID_HANDLE_VALUE)
	{
		SendMessageW(g_hComboBank, CB_ADDSTRING, 0, (LPARAM)L"(no sounds dir)");
		SendMessageW(g_hComboBank, CB_SETCURSEL, 0, 0);

		return;
	}

	count = 0;
	do
	{
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
			wcscmp(fd.cFileName, L".") != 0 &&
			wcscmp(fd.cFileName, L"..") != 0)
		{
			SendMessageW(g_hComboBank, CB_ADDSTRING, 0, (LPARAM)fd.cFileName);
			++count;
		}
	} while (FindNextFileW(h, &fd));
	FindClose(h);

	if (count == 0)
	{
		SendMessageW(g_hComboBank, CB_ADDSTRING, 0, (LPARAM)L"(empty)");
	}
	SendMessageW(g_hComboBank, CB_SETCURSEL, 0, 0);
}

static bool GetSelectedBank(wchar_t* out, DWORD cch)
{
	int sel;
	LRESULT len;

	sel = (int)SendMessageW(g_hComboBank, CB_GETCURSEL, 0, 0);
	if (sel == CB_ERR)
	{
		return false;
	}

	len = SendMessageW(g_hComboBank, CB_GETLBTEXTLEN, sel, 0);
	if (len <= 0 || (DWORD)len + 1 > cch)
	{
		return false;
	}

	SendMessageW(g_hComboBank, CB_GETLBTEXT, sel, (LPARAM)out);
	out[len] = L'\0';

	if (out[0] == L'(')
	{
		return false;
	}

	return true;
}

static bool WriteTempTextFile(const char* utf8, wchar_t* outPath, DWORD cch)
{
	wchar_t tmpDir[MAX_PATH];
	wchar_t tmpFile[MAX_PATH];
	DWORD n;
	HANDLE h;
	DWORD written;
	DWORD len;
	BOOL ok;

	n = GetTempPathW(MAX_PATH, tmpDir);
	if (n == 0 || n >= MAX_PATH)
	{
		return false;
	}

	if (GetTempFileNameW(tmpDir, L"zkl", 0, tmpFile) == 0)
	{
		return false;
	}

	h = CreateFileW(tmpFile, GENERIC_WRITE, 0, NULL,
					CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	written = 0;
	len = (DWORD)strlen(utf8);
	ok = WriteFile(h, utf8, len, &written, NULL);
	CloseHandle(h);

	if (!ok || written != len)
	{
		return false;
	}

	wcsncpy(outPath, tmpFile, (size_t)(cch - 1));
	outPath[cch - 1] = L'\0';

	return true;
}

static char* WideToUtf8(const wchar_t* w)
{
	int need;
	char* buf;

	need = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
	if (need <= 0)
	{
		return NULL;
	}
	buf = (char*)malloc((size_t)need);
	if (!buf)
	{
		return NULL;
	}
	WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, need, NULL, NULL);

	return buf;
}

static char* DupStr(const char* s)
{
	size_t n;
	char* p;

	n = strlen(s) + 1;
	p = (char*)malloc(n);
	if (p)
	{
		memcpy(p, s, n);
	}

	return p;
}

static char* GetEditTextUtf8(HWND hEdit)
{
	int lenW;
	wchar_t* wbuf;
	char* u8;

	lenW = GetWindowTextLengthW(hEdit);
	if (lenW <= 0)
	{
		return DupStr("");
	}

	wbuf = (wchar_t*)malloc(((size_t)lenW + 1) * sizeof(wchar_t));
	if (!wbuf)
	{
		return NULL;
	}

	GetWindowTextW(hEdit, wbuf, lenW + 1);
	u8 = WideToUtf8(wbuf);
	free(wbuf);

	return u8;
}

struct ByteBuf
{
	char*  p;
	size_t len;
	size_t cap;
	ByteBuf() : p(NULL), len(0), cap(0) {}
	~ByteBuf() { free(p); }
	void Append(const char* s, size_t n)
	{
		if (len + n + 1 > cap)
		{
			size_t nc = (cap ? cap * 2 : 1024);
			char* np;
			while (nc < len + n + 1)
			{
				nc *= 2;
			}
			np = (char*)realloc(p, nc);
			if (!np)
			{
				return;
			}
			p = np; cap = nc;
		}
		memcpy(p + len, s, n);
		len += n;
		p[len] = 0;
	}
};

static bool RunZklep(const wchar_t* textFile,
					 const wchar_t* bank,
					 const wchar_t* outName,
					 DWORD* outExitCode,
					 ByteBuf* outLog)
{
	wchar_t exePath[MAX_PATH];
	wchar_t exeDir[MAX_PATH];
	wchar_t outDir[MAX_PATH];
	wchar_t cmdLine[4096];
	SECURITY_ATTRIBUTES sa;
	HANDLE hRead;
	HANDLE hWrite;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	BOOL ok;
	char tmp[4096];
	DWORD nRead;
	DWORD ec;
	int r;

	if (!PickZklepExe(exePath, MAX_PATH))
	{
		MessageBoxW(NULL,
					L"Neither GUI not found ZKLEP_Win_i686.exe\n"
					L" or ZKLEP_Win_x86-64.exe.",
					L"Error", MB_ICONERROR);

		return false;
	}

	if (!GetExeDir(exeDir, MAX_PATH))
	{
		return false;
	}

	r = _snwprintf(outDir, MAX_PATH, L"%s\\%s", exeDir, kOutputDir);
	if (r <= 0 || (size_t)r >= MAX_PATH)
	{
		return false;
	}

	CreateDirectoryW(outDir, NULL);

	r = _snwprintf(cmdLine, 4096, L"\"%s\" \"%s\" \"%s\" \"%s\"",
				   exePath, textFile, bank, outName);
	if (r <= 0 || (size_t)r >= 4096)
	{
		return false;
	}

	ZeroMemory(&sa, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	hRead = NULL;
	hWrite = NULL;
	if (!CreatePipe(&hRead, &hWrite, &sa, 0))
	{
		MessageBoxW(NULL, L"CreatePipe failed", L"Error", MB_ICONERROR);

		return false;
	}
	SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = hWrite;
	si.hStdError  = hWrite;
	si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
	ZeroMemory(&pi, sizeof(pi));

	ok = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE,
						CREATE_NO_WINDOW, NULL, exeDir, &si, &pi);
	CloseHandle(hWrite);

	if (!ok)
	{
		CloseHandle(hRead);
		MessageBoxW(NULL, L"Failed to launch ZKLEP.",
					L"Error", MB_ICONERROR);

		return false;
	}

	nRead = 0;
	while (ReadFile(hRead, tmp, sizeof(tmp), &nRead, NULL) && nRead > 0)
	{
		if (outLog)
		{
			outLog->Append(tmp, nRead);
		}
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	ec = 0;
	GetExitCodeProcess(pi.hProcess, &ec);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	CloseHandle(hRead);

	if (outExitCode)
	{
		*outExitCode = ec;
	}

	return true;
}

static void ShowLogBox(const wchar_t* title, const char* utf8)
{
	int need;
	wchar_t* w;

	if (!utf8 || !*utf8)
	{
		MessageBoxW(NULL, L"(пусто)", title, MB_ICONINFORMATION);

		return;
	}

	need = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	if (need <= 0)
	{
		return;
	}

	w = (wchar_t*)malloc((size_t)need * sizeof(wchar_t));
	if (!w)
	{
		return;
	}

	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, need);

	MessageBoxW(NULL, w, title, MB_ICONINFORMATION);
	free(w);
}

static void OnGenerate(HWND hWnd)
{
	wchar_t bank[128];
	wchar_t outName[256];
	char* text;
	wchar_t tmpFile[MAX_PATH];
	DWORD ec;
	ByteBuf log;
	wchar_t msg[512];
	wchar_t title[128];
	size_t ol;
	int r;

	(void)hWnd;

	if (!GetSelectedBank(bank, 128))
	{
		MessageBoxW(NULL, L"Select bank (sounds\\<bank>\\*.wav).",
					L"Bank", MB_ICONWARNING);

		return;
	}

	GetWindowTextW(g_hEditOut, outName, 256);
	if (outName[0] == L'\0')
	{
		wcsncpy(outName, L"out.wav", 256);
	}

	ol = wcslen(outName);
	if (ol < 4 || _wcsicmp(outName + ol - 4, L".wav") != 0)
	{
		wcsncat(outName, L".wav", 256 - ol - 1);
	}

	text = GetEditTextUtf8(g_hEdit);
	if (!text)
	{
		SetWindowTextW(g_hStatus, L"Unable to read the text.");

		return;
	}

	if (!WriteTempTextFile(text, tmpFile, MAX_PATH))
	{
		free(text);
		SetWindowTextW(g_hStatus, L"Failed to create a temporary file.");

		return;
	}
	free(text);

	SetWindowTextW(g_hStatus, L"Generation...");
	UpdateWindow(g_hStatus);

	ec = 0;
	if (!RunZklep(tmpFile, bank, outName, &ec, &log))
	{
		SetWindowTextW(g_hStatus, L"Startup error.");
		DeleteFileW(tmpFile);

		return;
	}

	DeleteFileW(tmpFile);

	if (ec == 0)
	{
		r = _snwprintf(msg, 512, L"Ready: %s\\%s\\%s", kOutputDir, L"", outName);
	}
	else
	{
		r = _snwprintf(msg, 512, L"ZKLEP error, code %lu. See the window for details.",
					   (unsigned long)ec);
	}
	if (r <= 0 || (size_t)r >= 512)
	{
		wcsncpy(msg, L"Done.", 512);
	}

	SetWindowTextW(g_hStatus, msg);

	if (ec != 0)
	{
		r = _snwprintf(title, 128, L"ZKLEP exit=%lu", (unsigned long)ec);
		if (r <= 0 || (size_t)r >= 128)
		{
			wcsncpy(title, L"ZKLEP error", 128);
		}

		ShowLogBox(title, log.p);
	}
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_CREATE:
		{
			HWND all[8];
			int i;
			int nAll;

			g_hFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
								  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
								  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
								  DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

			g_hLblBank = CreateWindowExW(0, L"STATIC", L"Bank:",
										 WS_CHILD | WS_VISIBLE | SS_LEFT,
										 10, 10, 60, 20, hWnd, (HMENU)IDC_STATIC_BANK, NULL, NULL);

			g_hComboBank = CreateWindowExW(0, L"COMBOBOX", L"",
										   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
										   70, 8, 220, 200, hWnd, (HMENU)IDC_COMBO_BANK, NULL, NULL);

			g_hBtnRefresh = CreateWindowExW(0, L"BUTTON", L"Refresh",
											WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
											300, 7, 100, 24, hWnd, (HMENU)IDC_BTN_REFRESH, NULL, NULL);

			g_hLblOut = CreateWindowExW(0, L"STATIC", L"Output:",
										WS_CHILD | WS_VISIBLE | SS_LEFT,
										420, 10, 60, 20, hWnd, (HMENU)IDC_STATIC_OUT, NULL, NULL);

			g_hEditOut = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"out.wav",
										 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
										 480, 8, 190, 24, hWnd, (HMENU)IDC_EDIT_OUTNAME, NULL, NULL);

			g_hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
									  WS_CHILD | WS_VISIBLE | WS_VSCROLL |
									  ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
									  10, 40, 100, 100, hWnd, (HMENU)IDC_EDIT_TEXT, NULL, NULL);

			g_hButton = CreateWindowExW(0, L"BUTTON", L"Generate",
										WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
										10, 10, 140, 32, hWnd, (HMENU)IDC_BTN_GENERATE, NULL, NULL);

			g_hStatus = CreateWindowExW(0, L"STATIC", L"",
										WS_CHILD | WS_VISIBLE | SS_LEFT,
										10, 10, 100, 20, hWnd, (HMENU)IDC_STATIC_STATUS, NULL, NULL);

			all[0] = g_hLblBank;
			all[1] = g_hComboBank;
			all[2] = g_hBtnRefresh;
			all[3] = g_hLblOut;
			all[4] = g_hEditOut;
			all[5] = g_hEdit;
			all[6] = g_hButton;
			all[7] = g_hStatus;
			nAll = (int)(sizeof(all) / sizeof(all[0]));
			for (i = 0; i < nAll; ++i)
			{
				if (g_hFont && all[i]) SendMessageW(all[i], WM_SETFONT, (WPARAM)g_hFont, TRUE);
			}

			RefreshBankList();

			return 0;
		}

		case WM_SIZE:
		{
			int w;
			int h;
			int margin;
			int rowY;
			int btnH;
			int statusH;
			int editTop;
			int editH;

			w = LOWORD(lParam);
			h = HIWORD(lParam);
			margin = 10;
			rowY = 8;
			btnH = 32;
			statusH = 22;
			editTop = rowY + 26 + margin;
			editH = h - editTop - margin * 2 - btnH - statusH;

			if (g_hComboBank)
			{
				MoveWindow(g_hComboBank, 70, rowY, 220, 200, TRUE);
			}
			if (g_hBtnRefresh)
			{
				MoveWindow(g_hBtnRefresh, 300, rowY, 100, 24, TRUE);
			}
			if (g_hLblOut)
			{
				MoveWindow(g_hLblOut, 420, rowY + 2, 60, 20, TRUE);
			}
			if (g_hEditOut)
			{
				MoveWindow(g_hEditOut, 480, rowY,
						   (w > 690 ? w - margin - 480 : 190), 24, TRUE);
			}

			if (g_hEdit)
			{
				MoveWindow(g_hEdit, margin, editTop, w - margin * 2, editH, TRUE);
			}
			if (g_hButton)
			{
				MoveWindow(g_hButton, margin, editTop + editH + margin,
						   140, btnH, TRUE);
			}
			if (g_hStatus)
			{
				MoveWindow(g_hStatus, margin + 150, editTop + editH + margin + 6,
						   w - margin * 2 - 150, statusH, TRUE);
			}

			return 0;
		}

		case WM_COMMAND:
		{
			WORD id;
			WORD code;

			id   = LOWORD(wParam);
			code = HIWORD(wParam);
			if (id == IDC_BTN_GENERATE && code == BN_CLICKED)
			{
				OnGenerate(hWnd);
			}
			else if (id == IDC_BTN_REFRESH && code == BN_CLICKED)
			{
				RefreshBankList();
			}

			return 0;
		}

		case WM_DESTROY:
			if (g_hFont)
			{
				DeleteObject(g_hFont);
			}
			PostQuitMessage(0);

			return 0;
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	const wchar_t* kClassName;
	WNDCLASSEXW wc;
	HWND hWnd;
	MSG msg;

	SetProcessDPIAware();

	kClassName = L"ZKLEP_GUI_WINDOW";

	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize        = sizeof(wc);
	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = hInstance;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszClassName = kClassName;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassExW(&wc))
	{
		MessageBoxW(NULL, L"RegisterClassEx failed", L"Error", MB_ICONERROR);

		return 1;
	}

	hWnd = CreateWindowExW(
				0, kClassName, L"ZKLEP",
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, 760, 560,
				NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		MessageBoxW(NULL, L"CreateWindowEx failed", L"Error", MB_ICONERROR);

		return 1;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	while (GetMessageW(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return (int)msg.wParam;
}
