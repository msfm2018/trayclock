// a64x.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include<iostream>
#include <stdio.h>
#undef   UNICODE 
#include <sysinfoapi.h>
#include <TlHelp32.h>
#include <string>
#include <io.h>
#include<cstring>
#include<tchar.h>
#include <atlstr.h>
using namespace std;

#define INJECT_PROCESS_NAME    "explorer.exe" //Ŀ�����
typedef WCHAR WPATH[MAX_PATH];
typedef DWORD64(WINAPI *PFNTCREATETHREADEX)
(
	PHANDLE                 ThreadHandle,
	ACCESS_MASK             DesiredAccess,
	LPVOID                  ObjectAttributes,
	HANDLE                  ProcessHandle,
	LPTHREAD_START_ROUTINE  lpStartAddress,
	LPVOID                  lpParameter,
	BOOL                    CreateSuspended,
	DWORD64                   dwStackSize,
	DWORD64                   dw1,
	DWORD64                   dw2,
	LPVOID                  Unknown
	);

//����ǰ������ 
 BOOL   CreateRemoteThreadLoadDll(LPCWSTR   lpwLibFile, DWORD64   dwProcessId);
 BOOL   CreateRemoteThreadUnloadDll(LPCWSTR   lpwLibFile, DWORD64   dwProcessId);
 HANDLE MyCreateRemoteThread(HANDLE hProcess, LPTHREAD_START_ROUTINE pThreadProc, LPVOID pRemoteBuf);

BOOL   EnableDebugPrivilege(VOID);
int   AddPrivilege(LPCWSTR   *Name);
void   GetWorkPath(TCHAR   szPath[], int   nSize);

//ȫ�ֱ������� 
HANDLE   hProcessSnap = NULL;     //���̿��վ�� 
DWORD64   dwRemoteProcessId;       //Ŀ�����ID 

								   //--------------------------------------------------------------------- 
								   //ע�뺯�������øú�������
//int injectDll()
//{
//	BOOL result = FALSE;
//	//����Ȩ��
//	result = EnableDebugPrivilege();
//	if (result != TRUE)
//	{
//		printf("add privilege failed!\n");
//		return -1;
//	}
//	PROCESSENTRY32   pe32 = { 0 };
//	//�򿪽��̿���
//	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//
//	if (hProcessSnap == (HANDLE)-1)
//	{
//		return   -1;
//	}
//
//	pe32.dwSize = sizeof(PROCESSENTRY32);
//
//	//��ȡĿ����̵�ID
//	if (Process32First(hProcessSnap, &pe32))   //��ȡ��һ������ 
//	{
//		do {
//			char te[MAX_PATH];
//			strcpy(te, pe32.szExeFile);
//			if (strcmp(te, INJECT_PROCESS_NAME) == 0)
//			{
//				dwRemoteProcessId = pe32.th32ProcessID;
//				printf("%d\n", dwRemoteProcessId);
//				break;
//			}
//		} while (Process32Next(hProcessSnap, &pe32));//��ȡ��һ������ 
//	}
//	else
//	{
//		return   -1;
//	}
//
//
//	WCHAR   wsz[MAX_PATH];
//	swprintf(wsz, L"%S ", "F:\\hookdll.dll"); //dll��ַ
//
//	LPCWSTR   p = wsz;
//	//��Ŀ������д����̲߳�ע��dll
//	if (CreateRemoteThreadLoadDll(p, dwRemoteProcessId))
//		return 1;
//}

BOOL SetPrivilege(
	HANDLE hToken,          // access token handle
	LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
	BOOL bEnablePrivilege   // to enable or disable privilege
	)
{
	TOKEN_PRIVILEGES tp;
	LUID luid;

	if (!LookupPrivilegeValue(
		NULL,            // lookup privilege on local system
		lpszPrivilege,   // privilege to lookup 
		&luid))        // receives LUID of privilege
	{
		printf("LookupPrivilegeValue error: %u/n", GetLastError());
		return FALSE;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	if (bEnablePrivilege)
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes = 0;

	// Enable the privilege or disable all privileges.

	if (!AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tp,
		sizeof(TOKEN_PRIVILEGES),
		(PTOKEN_PRIVILEGES)NULL,
		(PDWORD)NULL))
	{
		printf("AdjustTokenPrivileges error: %u/n", GetLastError());
		return FALSE;
	}

	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

	{
		printf("The token does not have the specified privilege. /n");
		return FALSE;
	}

	return TRUE;
}

HANDLE GetProcessHandle(int nID)
{
	HANDLE hToken;
	bool flag = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);
	if (!flag)
	{
		DWORD err = GetLastError();
		printf("OpenProcessToken error:%d", err);
	}
	SetPrivilege(hToken, SE_DEBUG_NAME, true);
	CloseHandle(hToken);
	return OpenProcess(PROCESS_ALL_ACCESS, FALSE, nID);
}
//--------------------------------------------------------------------- 
//��Ŀ������д����̲߳�ע��dll
BOOL   CreateRemoteThreadLoadDll(LPCWSTR   lpwLibFile, DWORD64   dwProcessId)
{
	BOOL   bRet = FALSE;
	HANDLE   hProcess = NULL, hThread = NULL;
	LPVOID pszLibRemoteFile = NULL;
	SIZE_T dwWritten = 0;
	__try
	{
		//1.�򿪽��̣�ͬʱ����Ȩ�ޣ�����������PROCESS_ALL_ACCESS
		hProcess = GetProcessHandle(dwProcessId);
		//hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, dwProcessId);
		if (hProcess == NULL)
		{
			MessageBox(0, L"ssss", L"hProcess == NULL ", 0);
			__leave;
		}
		int   cch = 1 + lstrlenW(lpwLibFile);
		int   cb = cch   *   sizeof(WCHAR);
		printf("cb:%d\n", cb);
		printf("cb1:%d\n", sizeof(lpwLibFile));
		//2.�����㹻�Ŀռ䣬�Ա�����ǵ�dllд��Ŀ�����������ռ���
		pszLibRemoteFile = VirtualAllocEx(hProcess, NULL, cb, MEM_COMMIT, PAGE_READWRITE);

		if (pszLibRemoteFile == NULL)
			__leave;
		//3.��ʽ�����ǵ�dllд����������Ŀռ�
		BOOL   bw = WriteProcessMemory(hProcess, pszLibRemoteFile, (PVOID)lpwLibFile, cb, &dwWritten);
		if (dwWritten != cb)
		{
			printf("write error!\n");
		}
		if (!bw)
			__leave;
		//4.��ùؼ�����LoadLibraryW��ַ
		PTHREAD_START_ROUTINE   pfnThreadRnt = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(L"Kernel32"), "LoadLibraryW");
		if (pfnThreadRnt == NULL)
		{
			MessageBox(0, L"ssss", L"pfnThreadRnt == NULL", 0);
			__leave;
		}
		//5.�����̲߳���LoadLibraryW��Ϊ�߳���ʼ����������LoadLibraryW�Ĳ��������ǵ�dll
		hThread = MyCreateRemoteThread(hProcess, pfnThreadRnt, pszLibRemoteFile);
		if (hThread == NULL)
			__leave;
		//6.�ȴ���һ�����
		WaitForSingleObject(hThread, INFINITE);

		bRet = TRUE;
	}
	__finally
	{
		if (pszLibRemoteFile != NULL)
			VirtualFreeEx(hProcess, pszLibRemoteFile, 0, MEM_RELEASE);

		if (hThread != NULL)
			CloseHandle(hThread);

		if (hProcess != NULL)
			CloseHandle(hProcess);
	}

	return   bRet;
}
//--------------------------------------------------------------------- 
//ж��dll
BOOL   CreateRemoteThreadUnloadDll(LPCWSTR   lpwLibFile, DWORD64   dwProcessId)
{
	BOOL   bRet = FALSE;
	HANDLE   hProcess = NULL, hThread = NULL;
	HANDLE   hSnapshot = NULL;

	__try
	{
		hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
		if (hSnapshot == NULL)
			__leave;
		MODULEENTRY32W   me = { sizeof(MODULEENTRY32W) };
		BOOL   bFound = FALSE;
		BOOL   bMoreMods = Module32FirstW(hSnapshot, &me);
		for (; bMoreMods; bMoreMods = Module32NextW(hSnapshot, &me))
		{
			bFound = (lstrcmpiW(me.szModule, lpwLibFile) == 0) ||
				(lstrcmpiW(me.szExePath, lpwLibFile) == 0);
			if (bFound)
				break;
		}

		if (!bFound)
			__leave;

		hProcess = OpenProcess(
			PROCESS_CREATE_THREAD |
			PROCESS_VM_OPERATION,
			FALSE, dwProcessId);

		if (hProcess == NULL)
			__leave;

		PTHREAD_START_ROUTINE   pfnThreadRnt = (PTHREAD_START_ROUTINE)
			GetProcAddress(GetModuleHandle(L"Kernel32"), "FreeLibrary");
		if (pfnThreadRnt == NULL)
			__leave;

		//hThread   =   CreateRemoteThread(hProcess,   NULL,   0,pfnThreadRnt,   me.modBaseAddr,   0,   NULL); 
		hThread = MyCreateRemoteThread(hProcess, pfnThreadRnt, me.modBaseAddr);

		if (hThread == NULL)
			__leave;

		WaitForSingleObject(hThread, INFINITE);

		bRet = TRUE;
	}
	__finally
	{
		if (hSnapshot != NULL)
			CloseHandle(hSnapshot);

		if (hThread != NULL)
			CloseHandle(hThread);

		if (hProcess != NULL)
			CloseHandle(hProcess);
	}

	return   bRet;
}
//--------------------------------------------------------------------- 
//��������Ȩ��
BOOL   EnableDebugPrivilege()
{
	HANDLE   hToken;
	BOOL   fOk = FALSE;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		TOKEN_PRIVILEGES   tp;
		tp.PrivilegeCount = 1;
		if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid));
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL));
		else
			fOk = TRUE;
		CloseHandle(hToken);
	}
	return   fOk;
}

//--------------------------------------------------------------------- 
//Ϊ��ǰ��������ָ������Ȩ 
int   AddPrivilege(LPCWSTR Name)
{
	HANDLE   hToken;
	TOKEN_PRIVILEGES   tp;
	LUID   Luid;

	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
		&hToken))
	{
#ifdef   _DEBUG 
		printf("OpenProcessToken   error.\n ");
#endif 
		return   1;
	}

	if (!LookupPrivilegeValue(NULL, Name, &Luid))
	{
#ifdef   _DEBUG 
		printf("LookupPrivilegeValue   error.\n ");
#endif 
		return   1;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	tp.Privileges[0].Luid = Luid;

	if (!AdjustTokenPrivileges(hToken,
		0,
		&tp,
		sizeof(TOKEN_PRIVILEGES),
		NULL,
		NULL))
	{
#ifdef   _DEBUG 
		printf("AdjustTokenPrivileges   error.\n ");
#endif 
		return   1;
	}

	return   0;
}

BOOL IsWow64()
{
	typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;
	BOOL bIsWow64 = FALSE;
	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
		GetModuleHandle(L"kernel32"), "IsWow64Process");
	if (NULL != fnIsWow64Process)
	{
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			return FALSE;
		}
		else
			return TRUE;
	}
	return bIsWow64;
}



//--------------------------------------------------------------------- 
//����ϵͳ�汾�ж�
BOOL IsVistaOrLater()
{
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
#pragma warning(disable:4996)
	GetVersionEx(&osvi);
	if (osvi.dwMajorVersion >= 6)
		return TRUE;
	return FALSE;
}
//--------------------------------------------------------------------- 
//OD���٣����������õ���NtCreateThreadEx,���������ֶ�����
HANDLE MyCreateRemoteThread(HANDLE hProcess, LPTHREAD_START_ROUTINE pThreadProc, LPVOID pRemoteBuf)
{
	HANDLE      hThread = NULL;
	FARPROC     pFunc = NULL;
	if (IsVistaOrLater())    // Vista, 7, Server2008  
	{
		pFunc = GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtCreateThreadEx");
		if (pFunc == NULL)
		{
			printf("MyCreateRemoteThread() : GetProcAddress(\"NtCreateThreadEx\") ����ʧ�ܣ��������: [%d]/n",
				GetLastError());
			return FALSE;
		}
		((PFNTCREATETHREADEX)pFunc)(
			&hThread,
			0x1FFFFF,
			NULL,
			hProcess,
			pThreadProc,
			pRemoteBuf,
			FALSE,
			NULL,
			NULL,
			NULL,
			NULL);
		if (hThread == NULL)
		{
			printf("MyCreateRemoteThread() : NtCreateThreadEx() ����ʧ�ܣ��������: [%d]/n", GetLastError());
			return FALSE;
		}
	}
	else                    // 2000, XP, Server2003  
	{
		
		hThread = CreateRemoteThread(hProcess,
			NULL,
			0,
			pThreadProc,
			pRemoteBuf,
			0,
			NULL);
		if (hThread == NULL)
		{
			
			printf("MyCreateRemoteThread() : CreateRemoteThread() ����ʧ�ܣ��������: [%d]/n", GetLastError());
			return FALSE;
		}
	}
	if (WAIT_FAILED == WaitForSingleObject(hThread, INFINITE))
	{
		printf("MyCreateRemoteThread() : WaitForSingleObject() ����ʧ�ܣ��������: [%d]/n", GetLastError());
		return FALSE;
	}
	return hThread;
}


int main()
{
HWND h1 = FindWindow(L"Shell_TrayWnd", NULL);
HWND h2 = FindWindowEx(h1, 0, L"TrayNotifyWnd", NULL);
	HWND h3 = FindWindowEx(h2, 0, L"TrayClockWClass", NULL);
	DWORD pid;
 GetWindowThreadProcessId(h3, &pid); // PChar(Edit1.Text)
 
  TCHAR szBuf[MAX_PATH];

         ZeroMemory(szBuf, MAX_PATH);

		 if (GetCurrentDirectory(MAX_PATH, szBuf) > 0)
		 {
			 lstrcat(szBuf, L"\\a64.dll");
			 CreateRemoteThreadLoadDll(szBuf, pid);
		 }

  getchar();
    return 0;
}

