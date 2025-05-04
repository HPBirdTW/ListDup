#include "AuxFunc.h"

//
// Global variable
//

CLS_PRINT   gWsPrint;

std::string ConvertWideToANSI(const std::wstring& wstr)
{
    int count = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    std::string str(count, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &str[0], count, NULL, NULL);
    return str;
}

std::wstring ConvertAnsiToWide(const char* ansiStr, int multyCount)
{
    int count = MultiByteToWideChar(CP_ACP, 0, ansiStr, multyCount, NULL, 0);
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_ACP, 0, ansiStr, multyCount, &wstr[0], count);
    return wstr;
}

CLS_PRINT:: ~CLS_PRINT()
{
    if (this->pConOutFile)
    {
        this->Mutx.lock();
        fclose(this->pConOutFile);
        this->Mutx.unlock();
    }
    this->pConOutFile = NULL;
    this->bTmrthrClose = true;
    if (this->pTmrthr)
    {
        this->pTmrthr->join();
    }
}

bool CLS_PRINT::EnConOutSilent()
{
    Mutx.lock();
    bConOutDisp = false;
    Mutx.unlock();
    return true;
}

bool CLS_PRINT::DisConOutSilent()
{
    Mutx.lock();
    bConOutDisp = true;
    Mutx.unlock();
    return true;
}

bool CLS_PRINT::EnWaitLongFunc()
{
    Mutx.lock();
    bWaitLongFunc = true;
    Mutx.unlock();
    return true;
}

bool CLS_PRINT::DisWaitLongFunc()
{
    Mutx.lock();
    bWaitLongFunc = false;
    Mutx.unlock();
    this->wsPrint(L"\n");
    return true;
}

void CLS_PRINT::TmrthrFunc(CLS_PRINT* _this)
{
    const long  DurationTime = 10;
    while (false == _this->bTmrthrClose)
    {
        _this->szDCountMis += DurationTime;
        this_thread::sleep_for(chrono::milliseconds(DurationTime));

        if (_this->pConOutFile && (_this->szDCountMis % 200 == 0) )
        {
            if(_this->Mutx.try_lock())
            {
                fflush(_this->pConOutFile);
                _this->Mutx.unlock();
            }
        }

        if (_this->szDCountMis % 500 == 0)
        {
            if (true == _this->bWaitLongFunc)
            {
                _this->wsPrint(L".");
            }
        }
    }
    return;
}

int CLS_PRINT::outFileInit(const wchar_t* outFileName)
{
    if (pConOutFile)
    {
        fclose(pConOutFile);
    }
    return _wfopen_s(&pConOutFile, outFileName, L"a+");
}
int CLS_PRINT::wsPrint(wchar_t const* const _Format, ...)
{
    int                 _Result = 0;

    va_list _ArgList;
    __crt_va_start(_ArgList, _Format);

    this->Mutx.lock();
    if (this->pConOutFile)
    {
        _Result = _vfwprintf_l(this->pConOutFile, _Format, NULL, _ArgList);
    }
    
    if (this->bConOutDisp)
    {
        _Result = _vfwprintf_l(stdout, _Format, NULL, _ArgList);
    }
    this->Mutx.unlock();

    __crt_va_end(_ArgList);
    return _Result;
}

wstring GetSysLastErrString()
{
    LPWSTR  lpMsgBuf = NULL;
    DWORD   dw;
    wstring retWStr;

    dw = GetLastError();
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    retWStr = lpMsgBuf;
    if (lpMsgBuf) LocalFree(lpMsgBuf);

    return retWStr;
}

int ListAllFileByAttribue(const wchar_t* _CurDir, vector<wstring>* RetDispFileList, DWORD OrFileAttrMask, DWORD ExcludeFileAttrMask)
{
    WIN32_FIND_DATAW        WinFileData;
    wchar_t*                tmpStrBuf;
    HANDLE                  hFile = 0;
    size_t                  Index;
    int                     RetValue = 0;

    vector<wstring>         childDirList;
    vector<wstring>         curDirListStack;
    wstring                 CurDirWStr;

    CurDirWStr = _CurDir;
    curDirListStack.push_back(CurDirWStr);

    tmpStrBuf = new wchar_t[MAX_PATH*4];

    do
    {
        childDirList.clear();
        CurDirWStr = curDirListStack[curDirListStack.size() - 1];
        curDirListStack.pop_back();

        lstrcpy(tmpStrBuf, CurDirWStr.c_str());
        lstrcat(tmpStrBuf, L"\\*");

        hFile = FindFirstFile(tmpStrBuf, &WinFileData);
        if (INVALID_HANDLE_VALUE == hFile)
        {
            WSPrint(L"Not find file in %s\n", tmpStrBuf);
            //RetValue = -1;
            continue;
        }

        do
        {
            //wsprintf(TempStrBuf, (PTCHAR)L"%s\\%s", CurDirWStr.c_str(), WinFileData.cFileName);
            //WSPrint(L"%s - Attribute[%x]\n", TempStrBuf, WinFileData.dwFileAttributes);
            if (WinFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (0 == lstrcmp(L".", WinFileData.cFileName) || 0 == lstrcmp(L"..", WinFileData.cFileName))
                    continue;
                wsprintf(tmpStrBuf, (PTCHAR)L"%s\\%s", CurDirWStr.c_str(), WinFileData.cFileName);
                childDirList.push_back(tmpStrBuf);
            }

            if (0 == (WinFileData.dwFileAttributes & ExcludeFileAttrMask))
            {
                if (WinFileData.dwFileAttributes & OrFileAttrMask)
                {
                    if (0 == lstrcmp(L".", WinFileData.cFileName) || 0 == lstrcmp(L"..", WinFileData.cFileName))
                        continue;

                    wsprintf(tmpStrBuf, L"%s\\%s", CurDirWStr.c_str(), WinFileData.cFileName);

                    RetDispFileList->push_back(tmpStrBuf);
                }
            }
        } while (FindNextFile(hFile, &WinFileData) != 0);

        sort(childDirList.begin(), childDirList.end());

        for (Index = 0; Index < childDirList.size(); ++Index)
        {
            curDirListStack.push_back(childDirList[Index].c_str());
        }
    } while (curDirListStack.size());

    curDirListStack.clear();
    childDirList.clear();
    delete[] tmpStrBuf;

    return RetValue;
}

bool GetFileBuf(const wchar_t* FileName, BYTE** outFile, size_t* OutBufSize)
{
    int         RetValue = 0;
    HANDLE      FileHandle = NULL;
    DWORD       FileSize;
    BOOLEAN     bSuccess;
    DWORD       FileReadCount = 0;
    DWORD       FileAttribute = 0;
    BYTE*       FileBuf = NULL;

    do
    {
        bSuccess = true;
        FileAttribute = GetFileAttributes(FileName);
        if (INVALID_FILE_ATTRIBUTES == FileAttribute)
        {
            FileAttribute = GetLastError();
            WSPrint(L"GetFileBuf - GetFileAttributes (%x) Failed - %s\n", FileAttribute, FileName);
            bSuccess = false;
            break;
        }

        FileHandle = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (NULL == FileHandle)
        {
            WSPrint(L"GetFileBuf - FileHandle is Invalid parameter - %s\n", FileName);
            bSuccess = false;
            break;
        }

        FileSize = GetFileSize(FileHandle, NULL);
        if (0 == FileSize)
        {
            WSPrint(L"File is Empty\n");
            //break;
        }
        else
        {
            FileBuf = new UINT8[FileSize];
            FileReadCount = 0;
            bSuccess = ReadFile(FileHandle, FileBuf, (DWORD)FileSize, &FileReadCount, NULL);
            if (FALSE == bSuccess || 0 == FileReadCount)
            {
                bSuccess = false;
                WSPrint(L"GetFileBuf - File read failed - %s\n", FileName);
                break;
            }
        }
    } while (FALSE);

    if (FileHandle)
    {
        CloseHandle(FileHandle);
    } 
    FileHandle = NULL;

    if (false == bSuccess)
    {
        if (FileBuf)
            delete[] FileBuf;
        FileBuf = NULL;
        FileSize = 0;
    }

    if (OutBufSize)
    {
        *OutBufSize = FileSize;
    }
    if (outFile)
    {
        *outFile = FileBuf;
    }

    return bSuccess;
}

size_t SystemDelFile(const wchar_t* Dest)
{
    size_t              retVal = 0;
    wchar_t* TempStrBuf = NULL;

    TempStrBuf = new wchar_t[MAX_PATH * 4];
    /*
     * Use the follow, will never see the error message "ECHO F | XCOPY \"%s\" \"%s\" /Y /Q /S /E /R >nul 2>nul"
     */
#if 0
    const wchar_t       CnCmdLine[] = L"DEL \"%s\" /F /Q >nul";
    //wchar_t             TempStrBuf[MAX_PATH * 2 + (sizeof(CnCmdLine) / sizeof(CnCmdLine[0]))];  // Consider to have 2 file path.

    wsprintf(TempStrBuf, CnCmdLine, Dest);
    _wsystem(TempStrBuf);
#else
    {
        bool        bDelFile = false;
        DWORD       FileAttribute;
        const DWORD FILE_REMV_ATTR = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM;

        FileAttribute = GetFileAttributes(Dest);
        if (FileAttribute & FILE_REMV_ATTR)
        {
            SetFileAttributes(Dest, FileAttribute & ~FILE_REMV_ATTR);
        }

        bDelFile = DeleteFile(Dest);
        if (false == bDelFile)
        {
            WSPrint(L"Delete File failed: %s", Dest);
            retVal = -1;
        }
    }
#endif
    delete[] TempStrBuf;
    return retVal;
}


bool CreateDir(wstring sPath)
{
    size_t      pos = 0;
    DWORD       FileAttribute;
    const DWORD FILE_REMV_ATTR = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM;
    bool        bSuccess = true;
    wstring     tmpWStr;

    sPath += L"\\";
    while (std::string::npos != (pos = sPath.find_first_of(L"\\/", pos + 1)))
    {
        tmpWStr = sPath.substr(0, pos).c_str();
        FileAttribute = GetFileAttributes(tmpWStr.c_str());
        if (INVALID_FILE_ATTRIBUTES == FileAttribute)
        {
            bSuccess = CreateDirectory(tmpWStr.c_str(), NULL);
            if (false == bSuccess)
            {
                WSPrint(L"    CreateDirectoryW (%s) - %s\n", tmpWStr.c_str(), GetSysLastErrString().c_str());
            }
        }
        else
        {
            //WSPrint(L"Exist: %s\n", tmpWStr.c_str());
        }
    }

    return bSuccess;
}

size_t SystemCpyFile(const wchar_t* Dest, const wchar_t* Src)
{
    size_t      retVal = 0;
    wchar_t* tmpStrBuf = NULL;

    tmpStrBuf = new wchar_t[MAX_PATH * 8 + 0x100]; // Consider to have 2 file path + CmdLine cout.
#if 0
    /*
     * Use the follow, will never see the error message "ECHO F | XCOPY \"%s\" \"%s\" /Y /Q /S /E /R >nul 2>nul"
     */
    const wchar_t       CnCmdLine[] = L"ECHO F | XCOPY \"%s\" \"%s\" /Y /Q /V /R >nul";

    wsprintf(tmpStrBuf, CnCmdLine, Src, Dest);
    //WSPrint(tmpStrBuf, L"XCOPY %s %s* /Y /S /F /Q", Src, Dest);
    _wsystem(tmpStrBuf);
#else
    bool        bSuccess;
    wstring     tmpWStr;

    tmpWStr = Dest;
    tmpWStr = tmpWStr.substr(0, tmpWStr.find_last_of(L"\\/"));
    CreateDir(tmpWStr);
    bSuccess = CopyFile(Src, Dest, false);
    if (false == bSuccess)
    {
        WSPrint(L"    Fail on Copy - %s\n", GetSysLastErrString().c_str());
    }
    retVal = (bSuccess == true ? 0 : -1);
#endif
    delete[] tmpStrBuf;
    return retVal;
}

size_t SystemDelEmptyDir(const wchar_t* Dest)
{
    size_t      retVal = 0;
    wchar_t* TempStrBuf = NULL;
    TempStrBuf = new wchar_t[MAX_PATH * 4];
#if 0
    const wchar_t       CnCmdLine[] = L"ECHO F | RMDIR /Q \"%s\" >nul";
    //wchar_t             TempStrBuf[MAX_PATH * 2 + (sizeof(CnCmdLine) / sizeof(CnCmdLine[0]))];  // Consider to have 2 file path.

    wsprintf(TempStrBuf, CnCmdLine, Dest);
    //  wsprintf(TempStrBuf, L"XCOPY %s %s* /Y /S /F /Q", Source, Dest);
    _wsystem(TempStrBuf);
#else
    bool        bSuccess;
    DWORD       FileAttribute;
    const DWORD FILE_REMV_ATTR = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM;

    FileAttribute = GetFileAttributes(Dest);
    if (FileAttribute & FILE_REMV_ATTR)
    {
        SetFileAttributes(Dest, FileAttribute & ~FILE_REMV_ATTR);
    }

    bSuccess = RemoveDirectory(Dest);
    retVal = bSuccess == true ? 0 : -1;
#endif
    delete[] TempStrBuf;
    return retVal;
}

int min_3s(int x, int y, int z)
{
    if (x < y)
        return x < z ? x : z;
    else
        return y < z ? y : z;
}

int EditCmp_func(wchar_t t1, wchar_t t2)
{
    int retValue = 0;
    if (t1 == t2)
    {
        retValue = 0;
    }
    else if (t1 == L'*' || t2 == L'*')
    {
        retValue = 0;
    }
    else if (t1 == L'\\' && t2 == L'/')
    {
        retValue = 0;
    }
    else if (t2 == L'\\' && t1 == L'/')
    {
        retValue = 0;
    }
    else
    {
        retValue = 1;
    }

    return retValue;
}

int EditDistance(const wchar_t* s1, int m, const wchar_t* s2, int n, wtCmp_func CmpFunc)
{
    int** dp;
    int res, i, j;

    dp = new int* [size_t(m) + 1];
    if (dp == NULL)
        return -1;

    for (i = 0; i < m + 1; i++)
    {
        dp[i] = new int[size_t(n) + 1];
        if (dp[i] == NULL)
            return -1;
    }

    //caculate
    for (i = 0; i < m + 1; i++)
    {
        for (j = 0; j < n + 1; j++)
        {
            if (i == 0)
                dp[i][j] = j;
            else if (j == 0)
                dp[i][j] = i;
            else
            {
                dp[i][j] = (*CmpFunc)(s1[i - 1], s2[j - 1]) + min_3s(dp[i - 1][j],
                    dp[i][j - 1],
                    dp[i - 1][j - 1]);
            }
        }
    }

    res = dp[m][n];

    for (i = 0; i < m + 1; i++)
    {
        delete[] dp[i];
    }

    delete[] dp;

    return res;
}

void SMD_TimeTicket(LARGE_INTEGER* _Val)
{
    QueryPerformanceCounter(_Val);
}

void SMD_TimeDisp(LARGE_INTEGER StartVal, LARGE_INTEGER EndVal)
{
    LARGE_INTEGER   TimerVale;
    LARGE_INTEGER   Freq;

    QueryPerformanceCounter(&EndVal);
    QueryPerformanceFrequency(&Freq);

    TimerVale.QuadPart = EndVal.QuadPart - StartVal.QuadPart;
    TimerVale.QuadPart *= 1000000;
    TimerVale.QuadPart /= Freq.QuadPart;

    WSPrint(L"%d.%03d ms\n", TimerVale.QuadPart / 1000, TimerVale.QuadPart % 1000);
}
