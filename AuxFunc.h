#pragma once
#include <Windows.h>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <mutex>
#include <profileapi.h>

#define DISP_MEASRE_TIME    0

#if DISP_MEASRE_TIME
#define SMD_TIME_START_TAG(tag) {\
        LARGE_INTEGER tag##TimeStart, tag##TimeEnd; \
        SMD_TimeTicket(& tag##TimeStart);

#define SMD_TIME_END_TAG(arg,tag) \
        SMD_TimeTicket(& tag##TimeEnd); \
        WSPrint arg; \
        SMD_TimeDisp( tag##TimeStart, tag##TimeEnd ); }
#else
#define SMD_TIME_START_TAG(tag)
#define SMD_TIME_END_TAG(arg,tag)
#endif

#define WSPrint gWsPrint.wsPrint
#define ConOutFile gWsPrint.outFileInit

using namespace std;

class CLS_PRINT;
extern CLS_PRINT    gWsPrint;

int ListAllFileByAttribue(const wchar_t* CurDir, vector<wstring>* RetDispFileList, DWORD OrFileAttrMask, DWORD ExcludeFileAttrMask);
bool GetFileHdl(const wchar_t* FileName, HANDLE* FileHandle, size_t* FileSize);
bool GetFileBuf(const wchar_t* FileName, BYTE** outFile, size_t* outBufSize);
size_t SystemDelEmptyDir(const wchar_t* Dest);
size_t SystemCpyFile(const wchar_t* Dest, const wchar_t* Src);
size_t SystemDelFile(const wchar_t* Dest);

typedef int (*wtCmp_func)(wchar_t, wchar_t);
int EditCmp_func(wchar_t t1, wchar_t t2);
int EditDistance(const wchar_t* s1, int m, const wchar_t* s2, int n, wtCmp_func CmpFunc = &EditCmp_func);

class CLS_PRINT
{
  FILE*             pConOutFile;
  bool              bConOutDisp;
  bool              bTmrthrClose;
  bool              bWaitLongFunc;
  size_t            szDCountMis;
  std::thread*      pTmrthr;
  std::mutex        Mutx;
public:
  static void       TmrthrFunc(CLS_PRINT*);
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
void SMD_TimeTicket(LARGE_INTEGER* _Val);
void SMD_TimeDisp(LARGE_INTEGER StartVal, LARGE_INTEGER EndVal);