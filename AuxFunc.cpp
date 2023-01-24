#include "AuxFunc.h"

//
// Global variable
//

CLS_PRINT	gWsPrint;

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

    tmpStrBuf = new wchar_t[MAX_PATH*2];

    do
    {
        childDirList.clear();
        CurDirWStr = curDirListStack[curDirListStack.size() - 1];
        curDirListStack.pop_back();

        lstrcpy(tmpStrBuf, CurDirWStr.c_str());
        lstrcat(tmpStrBuf, L"\\*");

        hFile = FindFirstFileW(tmpStrBuf, &WinFileData);
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
    int			RetValue = 0;
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
            WSPrint(L"GetFileAttributes, GetLastError(%x) Failed - %s\n", FileAttribute, FileName);
            bSuccess = false;
            break;
        }

        FileHandle = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (NULL == FileHandle)
        {
            WSPrint(L"FileHandle is Invalid parameter - %s\n", FileName);
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
                WSPrint(L"File read failed\n");
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
