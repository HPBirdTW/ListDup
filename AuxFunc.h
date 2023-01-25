#pragma once
#include <Windows.h>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <mutex>

#define WSPrint gWsPrint.wsPrint
#define ConOutFile gWsPrint.outFileInit

using namespace std;

class CLS_PRINT;
extern CLS_PRINT	gWsPrint;

int ListAllFileByAttribue(const wchar_t* CurDir, vector<wstring>* RetDispFileList, DWORD OrFileAttrMask, DWORD ExcludeFileAttrMask);
bool GetFileBuf(const wchar_t* FileName, BYTE** outFile, size_t* outBufSize);

typedef int (*wtCmp_func)(wchar_t, wchar_t);
int EditCmp_func(wchar_t t1, wchar_t t2);
int EditDistance(const wchar_t* s1, int m, const wchar_t* s2, int n, wtCmp_func CmpFunc = &EditCmp_func);

class CLS_PRINT
{
	FILE*           pConOutFile;
	bool            bConOutDisp;
	bool			bTmrthrClose;
	bool			bWaitLongFunc;
	size_t			szDCountMis;
	std::thread*    pTmrthr;
	std::mutex	    Mutx;
public:
	static void		TmrthrFunc(CLS_PRINT*);
	CLS_PRINT() : pConOutFile(NULL), bConOutDisp(true), pTmrthr(NULL), bTmrthrClose(false), szDCountMis(0), bWaitLongFunc(false)
	{
		pTmrthr = new std::thread(TmrthrFunc, this);
	};
	int outFileInit(const wchar_t* outFileName);
	int wsPrint(wchar_t const* const _Format, ...);
	bool EnConOutSilent();
	bool DisConOutSilent();
	bool EnWaitLongFunc();
	bool DisWaitLongFunc();
	~CLS_PRINT();

};

wstring GetSysLastErrString();
std::wstring ConvertAnsiToWide(const char* ansiStr, int multyCount);
std::string ConvertWideToANSI(const std::wstring& wstr);