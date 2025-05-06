// SuffixTreeCode.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <iostream>
#include <string>
#include <vector>
#include <Windows.h>
#include <thread>
#include <process.h>
#include <sysinfoapi.h>
#include <mutex>
#include <signal.h>
#include "SuffixTree.h"
#include "AuxFunc.h"

using namespace std;

#define COMPARE_THREAD          1
// Threshold for Starting enable multi-thread calcualte Editdistance
// 0: Disable the Multi-thread CompareFunction
#define THRHD_MULTI_EDITDIST    5
#define READ_WHOLE_SRC_FILE     0

bool    gAbortTerminal = false;

extern size_t newAlloc;
extern size_t delRqst;

enum FileOpActEmnu {
    None = 0x00, FileCmp = 0x01, FileCmpDel = 0x02, RemoveEmptyDir = 0x04,
    FileDiffCpy = 0x08, FileListAllRst = 0x10, ChkRefDirMatch = 0x20, SilentMode=0x40,
};

struct ListClearParam
{
    std::wstring    DestDir;
    std::wstring    SrcDir;
    std::wstring    cpyDir;
    size_t          MaxThread;
    size_t          enumFileOp;
    bool            boolCmpUseThrd;
    bool            bAlgoEditDist;
    ListClearParam() : MaxThread(0), DestDir(), SrcDir(), enumFileOp(0), cpyDir(), boolCmpUseThrd(true), \
      bAlgoEditDist(false) {}
};

class EditSortStr
{
public:
    wstring             str;
    int                 EditDist;
    const static int    EDIT_NULL = -1;
    EditSortStr(wstring _str) :str(_str), EditDist(EDIT_NULL) {}
    EditSortStr() :str(), EditDist(EDIT_NULL) {}
    EditSortStr(const EditSortStr& obj) :str(obj.str), EditDist(obj.EditDist) {}
    EditSortStr& operator=(const wstring& obj)
    {
        str = obj;
        EditDist = EDIT_NULL;
        return *this;
    }
    EditSortStr& operator=(const EditSortStr& obj)
    {
        str = obj.str;
        EditDist = obj.EditDist;
        return *this;
    }
};

class csSortByWSDir
{
public:
    wstring     exptWStr;
    wstring     cutOffDir;
    bool        bAlgoEditDist;
    csSortByWSDir() :bAlgoEditDist(false) {}
    static int CalcEditDist(csSortByWSDir* _this, EditSortStr* a)
    {
        wstring aCutOffStr;

        if (a->EditDist == EditSortStr::EDIT_NULL)
        {
            aCutOffStr = a->str;
            if (_this->cutOffDir.size())
            {
                aCutOffStr = aCutOffStr.substr(aCutOffStr.find(_this->cutOffDir.c_str()) + _this->cutOffDir.size());
            }

            if (true == _this->bAlgoEditDist)
            {
                a->EditDist = EditDistance(_this->exptWStr.c_str(), (int)_this->exptWStr.length(), aCutOffStr.c_str(), (int)aCutOffStr.length());
            }
            else
            {
                a->EditDist = lstrcmp(_this->exptWStr.c_str(), aCutOffStr.c_str());
                if (EditSortStr::EDIT_NULL == a->EditDist)
                {
                    --(a->EditDist);
                }
            }
        }
        return a->EditDist;
    }
    bool operator() (EditSortStr& a, EditSortStr& b)
    {
        bool    bResult;

        a.EditDist = CalcEditDist(this, &a);
        b.EditDist = CalcEditDist(this, &b);

        if (true == bAlgoEditDist)
        {
            bResult = a.EditDist < b.EditDist;
        }
        else
        {
            bResult = (unsigned int)a.EditDist < (unsigned int)b.EditDist;
        }
        return bResult;
    }
};

struct FileOpActExtParam
{
    volatile bool*  FileDeleted;
    volatile bool*  ReadDestFileFailed;
    volatile bool*  ReadSrcFileFailed;
    volatile int    szTempCmpMatch;
    BYTE*           destFileBuf;
    size_t          destFileBufSize;
    std::mutex      fileOpMutex;
    std::mutex      fileOpMutex2;
    size_t          szReadFileErr;
    size_t          szDelFileCount;
    size_t          szSysCpyFileErr;
    size_t          szMatchCount;
    FileOpActExtParam() :FileDeleted(NULL), ReadDestFileFailed(NULL), ReadSrcFileFailed(NULL), szTempCmpMatch(0), destFileBuf(NULL),\
      destFileBufSize(0), szReadFileErr(0), szDelFileCount(0), szSysCpyFileErr(0), szMatchCount(0){}
};
size_t FileOpAction(const wchar_t* destFile, const wchar_t* srcFile, size_t multiOp, FileOpActExtParam* extParam)
{
    size_t              szTmpVal = 0;
    size_t              retVal = 0;
    bool                bFileCmpRst = false;
    BYTE*               destFileBuf = NULL;
    size_t              destFileBufSize = 0;
    BYTE*               srcFileBuf = NULL;
    size_t              srcFileBufSize = 0;
#if READ_WHOLE_SRC_FILE == 0
    const size_t        CONST_READ_SIZE = 0x100000;      // 1MB
    HANDLE              srcFileHandle = NULL;
    size_t              szIdx;
#endif

    do
    {
        if (multiOp & FileOpActEmnu::FileCmp)
        {
            while (!extParam->fileOpMutex.try_lock())
            {
                this_thread::sleep_for(chrono::milliseconds(5));
            }

            if (extParam->ReadDestFileFailed)
            {
                if (true == *extParam->ReadDestFileFailed)
                {
                    extParam->fileOpMutex.unlock();
                    break;
                }
            }
            if (extParam->ReadSrcFileFailed)
            {
                if (true == *extParam->ReadSrcFileFailed)
                {
                    extParam->fileOpMutex.unlock();
                    break;
                }
            }


            if (multiOp & FileOpActEmnu::FileCmpDel)
            {
                if (extParam->FileDeleted)
                {
                    if (true == *extParam->FileDeleted)
                    {
                        extParam->fileOpMutex.unlock();
                        break;
                    }
                }
            }

            if (0 == (multiOp & FileOpActEmnu::FileListAllRst))
            {
                if (extParam->szTempCmpMatch)
                {
                    extParam->fileOpMutex.unlock();
                    break;
                }
            }
            extParam->fileOpMutex.unlock();

            if (extParam->destFileBuf)
            {
                destFileBuf = extParam->destFileBuf;
                destFileBufSize = extParam->destFileBufSize;
            }
            else
            {
                bFileCmpRst = GetFileBuf(destFile, &destFileBuf, &destFileBufSize);
                if (bFileCmpRst == false)
                {
                    if (extParam->ReadDestFileFailed)    *extParam->ReadDestFileFailed = true;
                    retVal = FileOpActEmnu::FileCmp;
                    extParam->szReadFileErr++;
                    break;
                }
            }
#if READ_WHOLE_SRC_FILE
            bFileCmpRst = GetFileBuf(srcFile, &srcFileBuf , &srcFileBufSize);
            if (bFileCmpRst == false)
            {
                if (extParam->ReadSrcFileFailed)    *(extParam->ReadSrcFileFailed) = true;
                retVal = FileOpActEmnu::FileCmp;
                extParam->szReadFileErr++;
                break;
            }

            bFileCmpRst = false;
            if (destFileBufSize == srcFileBufSize)
            {
                if (0 == memcmp(destFileBuf, srcFileBuf, destFileBufSize))
                {
                    bFileCmpRst = true;
                }
            }
#else       
            bFileCmpRst = GetFileHdl(srcFile, &srcFileHandle, &srcFileBufSize);
            if (bFileCmpRst == false)
            {
                if (extParam->ReadSrcFileFailed)    *(extParam->ReadSrcFileFailed) = true;
                retVal = FileOpActEmnu::FileCmp;
                extParam->szReadFileErr++;
                break;
            }

            if (destFileBufSize != srcFileBufSize)
            {
                bFileCmpRst = false;
            }
            else if (0 == destFileBufSize)
            {
                bFileCmpRst = true;
            }
            else
            {
                srcFileBuf = new UINT8[CONST_READ_SIZE];

                for (szIdx = 0; szIdx < srcFileBufSize; szIdx += szTmpVal)
                {
                    if (extParam->szTempCmpMatch)
                    {
                        // Preivous find the match file.
                        break;
                    }

                    szTmpVal = CONST_READ_SIZE;
                    bFileCmpRst = ReadFile(srcFileHandle, (LPVOID)srcFileBuf, (DWORD)szTmpVal, (LPDWORD)&szTmpVal, NULL);
                    if (bFileCmpRst == false)
                    {
                        WSPrint(L"ReadFile - Failed - %s\n", srcFile);
                        if (extParam->ReadSrcFileFailed)    *extParam->ReadSrcFileFailed = true;
                        retVal = FileOpActEmnu::FileCmp;
                        extParam->szReadFileErr++;
                        break;
                    }

                    bFileCmpRst = false;
                    if (0 == memcmp(destFileBuf + szIdx, srcFileBuf, szTmpVal))
                    {
                        bFileCmpRst = true;
                    }
                    else
                    {
                      break;
                    }
                }
                // End Read File sequence
            }

            CloseHandle(srcFileHandle);
            srcFileHandle = NULL;
            // Check the for loop error
            if (retVal)
            {
                break;
            }
#endif

            extParam->fileOpMutex2.lock();
            if (bFileCmpRst)
            {
                extParam->szTempCmpMatch++;
                WSPrint(L"  [Match]\n");
                WSPrint(L"  %s\n  %s\n", destFile, srcFile);
            }
            else
            {
//                WSPrint(L"  [Diff]\n");
//                WSPrint(L"  %s\n  %s\n", destFile, srcFile);
            }            
            extParam->fileOpMutex2.unlock();
        }

        if (multiOp & FileOpActEmnu::FileCmpDel)
        {
            if (bFileCmpRst == false)
            {
                break;
            }

            WSPrint(L"    --Delete.\n");
            if (0 == SystemDelFile(destFile))
            {
                // Success Delete file
                extParam->szDelFileCount++;
            }
            else
            {
                retVal = FileOpActEmnu::FileCmpDel;
            }

            if (extParam->FileDeleted)    *extParam->FileDeleted = true;
        }
    } while (false);

    if (retVal)
    {
        if (multiOp & FileOpActEmnu::FileCmp)
        {
            WSPrint(L"%s", L"Fail on FileOpActEmnu::FileCmp\n");
        }
        if (multiOp & FileOpActEmnu::FileCmpDel)
        {
            WSPrint(L"%s", L"Fail on FileOpActEmnu::FileCmpDel\n");
        }
    }

    // Free Rsource
    if (NULL == extParam->destFileBuf)
    {
        if (destFileBuf)
            delete[] destFileBuf;
    }
    if (srcFileBuf)
        delete[] srcFileBuf;

    return retVal;
}

size_t RemoveEmptyFolder(const wchar_t* destFolder)
{
    size_t          szTmpVal;
    size_t          szIdx;
    size_t          szIdx2;
    size_t          szRetVal = 0;
    wstring         tmpWStr;
    vector<wstring> destListFile;
    vector<wstring> searchListFile;

    do
    {
        // Delete empty folder
        destListFile.clear();
        // get folder list
        szTmpVal = ListAllFileByAttribue(destFolder, &destListFile, FILE_ATTRIBUTE_DIRECTORY, 0);
        if (szTmpVal != 0)
        {
            szRetVal = szTmpVal;
            break;
        }
        sort(destListFile.begin(), destListFile.end());

        for (szIdx = 0; szIdx < destListFile.size(); ++szIdx)
        {
            szIdx2 = destListFile.size() - szIdx - 1;
            tmpWStr = destListFile[szIdx2];
            //WSPrint(L"  [EmptyFolder] %s\n", tmpWStr.c_str());
            searchListFile.clear();
            szTmpVal = ListAllFileByAttribue(tmpWStr.c_str(), &searchListFile, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_DIRECTORY);
            if (szTmpVal != 0)
            {
                WSPrint(L"Fail: %s ,Line[%ld]\n", __FUNCTIONW__, __LINE__);
                szRetVal = szTmpVal;
                continue;
            }

            if (searchListFile.size() == 0)
            {
                WSPrint(L"  %s\n", tmpWStr.c_str());
                WSPrint(L"    --Delete Empty Folder.\n");
                SystemDelEmptyDir(tmpWStr.c_str());
            }
            else
            {
                //WSPrint(L"    fileCount [%zd].\n", searchListFile.size());
            }
        }
    } while (FALSE);

    return szRetVal;
}

size_t ProcFileOp(ListClearParam* lcParam)
{
    std::vector<wstring>        DestListFile;
    std::vector<wstring>        SrcListFile;
    std::vector<wstring>        FindListFile;
    std::vector<EditSortStr>    SortListFile;
    std::wstring                tmpWStr;
    std::wstring                ShortFileWStr;
    size_t                      szTmpVal;
    size_t                      destFileIdx;
    size_t                      szIdx2;
    size_t                      szRetValue = 0;
    int                         tmpVal;
    SuffixTree                  suffixTree;
    VECT_INT                    SearchResult;
    FileOpActExtParam           fileOpExtParam;
    volatile bool               bSkipFile;
    const wchar_t*              END_CHAR = L"$";
#if COMPARE_THREAD
    std::vector<std::thread>    threads;
    std::vector<std::thread>    thrdEditFunc;
    std::vector<std::thread>::iterator  _pThread;
#endif
    csSortByWSDir               SortByWSDir;
    SYSTEM_INFO                 SystemInfo;

    //WSPrint(L"[%d]: %s\n", __LINE__, __FUNCTIONW__);
    do
    {
        GetSystemInfo(&SystemInfo);
        if (SystemInfo.dwNumberOfProcessors - 1 > THRHD_MULTI_EDITDIST)
        {
            // For File Operation, we seems not need to set "SystemInfo.dwNumberOfProcessors - 1"
            lcParam->MaxThread = THRHD_MULTI_EDITDIST;
        }

        SortByWSDir.bAlgoEditDist = lcParam->bAlgoEditDist;

        if (lcParam->SrcDir.size() == 0 || lcParam->DestDir.size() == 0)
        {
            szRetValue = -1;
            WSPrint(L"Line[%ld]: Error. %s\n", __LINE__, __FUNCTIONW__);
            break;
        }

        WSPrint(L"\n[Get Source file system list] : %s\n", lcParam->SrcDir.c_str());
        gWsPrint.EnWaitLongFunc();
        szTmpVal = ListAllFileByAttribue(lcParam->SrcDir.c_str(), &SrcListFile, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_DIRECTORY);
        gWsPrint.DisWaitLongFunc();
        if (szTmpVal != 0)
        {
            szRetValue = szTmpVal;
            WSPrint(L"Line[%ld]: Error. %s\n", __LINE__, __FUNCTIONW__);
            break;
        }

        if ((lcParam->enumFileOp & FileOpActEmnu::ChkRefDirMatch) && (lcParam->enumFileOp & FileOpActEmnu::FileListAllRst))
        {
            WSPrint(L"Fail on conflict feature (ChkRefDirMatch, FileListAllRst)\n");
            break;
        }

        WSPrint(L"\n[Get Destion file system list] : %s\n", lcParam->DestDir.c_str());
        gWsPrint.EnWaitLongFunc();
        szTmpVal = ListAllFileByAttribue(lcParam->DestDir.c_str(), &DestListFile, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_DIRECTORY);
        gWsPrint.DisWaitLongFunc();
        if (szTmpVal != 0)
        {
            szRetValue = szTmpVal;
            WSPrint(L"\nLine[%ld]: Error. %s\n", __LINE__, __FUNCTIONW__);
            break;
        }

        tmpWStr.clear();
        for (szIdx2 = 0; szIdx2 < SrcListFile.size(); ++szIdx2)
        {
            tmpWStr += SrcListFile[szIdx2].c_str();
            tmpWStr += END_CHAR;
        }

        WSPrint(L"\n[Build] Suffix Tree : size(0x%08x)\n", (UINT)(tmpWStr.size() * sizeof(wchar_t)) );
        gWsPrint.EnWaitLongFunc();
        suffixTree.build((UINT8*)tmpWStr.c_str(), (int)(tmpWStr.size() * sizeof(wchar_t)));
        gWsPrint.DisWaitLongFunc();
        WSPrint(L"\n  -- [Finish] Suffix Tree.\n");

        fileOpExtParam.FileDeleted = &bSkipFile;
        fileOpExtParam.ReadDestFileFailed = &bSkipFile;
        fileOpExtParam.ReadSrcFileFailed = &bSkipFile;

        if (lcParam->enumFileOp & FileOpActEmnu::SilentMode)
        {
            gWsPrint.EnConOutSilent();
        }

        for (destFileIdx = 0; destFileIdx < DestListFile.size() && false == gAbortTerminal; ++destFileIdx)
        {
            if (lcParam->enumFileOp & FileOpActEmnu::SilentMode)
            {
                gWsPrint.DisConOutSilent();
            }
            WSPrint(L"\n[%s]\n", DestListFile[destFileIdx].c_str());
            if (lcParam->enumFileOp & FileOpActEmnu::SilentMode)
            {
                gWsPrint.EnConOutSilent();
            }

            ShortFileWStr = DestListFile[destFileIdx];
            ShortFileWStr = ShortFileWStr.substr(ShortFileWStr.find_last_of(L"\\/") + 1);

            SMD_TIME_START_TAG(FindSuffix);
            tmpWStr = ShortFileWStr + END_CHAR;
            SearchResult.clear();
            suffixTree.findIdxBuf((UINT8*)tmpWStr.c_str(), (int)tmpWStr.size() * sizeof(wchar_t), &SearchResult);
            sort(SearchResult.begin(), SearchResult.end());
            SMD_TIME_END_TAG((L"FindSuffix: "), FindSuffix);

            SMD_TIME_START_TAG(GetFileList);
            SortListFile.clear();
            tmpVal = 0;
            szTmpVal = 0;
            for (szIdx2 = 0; szIdx2 < SearchResult.size(); ++szIdx2)
            {
                for (; szTmpVal < SrcListFile.size(); ++szTmpVal)
                {
                    tmpVal += (int)((SrcListFile[szTmpVal].size() + wcslen(END_CHAR)) * sizeof(wchar_t));
                    if (tmpVal > SearchResult[szIdx2])
                    {
                        szTmpVal += 1;
                        tmpWStr = SrcListFile[szTmpVal-1];
                        tmpWStr = tmpWStr.substr(tmpWStr.find_last_of(L"\\/") + 1);

                        if ((tmpWStr.size() != ShortFileWStr.size()) || lstrcmp(tmpWStr.c_str(), ShortFileWStr.c_str()))
                        {
                            //WSPrint(L"    Err.Find [%s]: %s\n", tmpWStr.c_str(), ShortFileWStr.c_str());
                            break;
                        }

                        SortListFile.push_back(SrcListFile[szTmpVal-1]);
                        break;
                    }
                }
            }
            SMD_TIME_END_TAG((L"GetMatchFile: "), GetFileList);

            SMD_TIME_START_TAG(EditCompare);
            tmpWStr = DestListFile[destFileIdx];
            tmpWStr = tmpWStr.substr(tmpWStr.find(lcParam->DestDir.c_str()) + lcParam->DestDir.size());
            SortByWSDir.exptWStr = tmpWStr;
            SortByWSDir.cutOffDir = lcParam->SrcDir.c_str();
#if COMPARE_THREAD
            if (true == lcParam->boolCmpUseThrd)
            {
                if (THRHD_MULTI_EDITDIST && (SortListFile.size() > THRHD_MULTI_EDITDIST))
                {
                    for (szIdx2=0; szIdx2 < SortListFile.size(); szIdx2++)
                    {
                        thrdEditFunc.push_back(std::thread(csSortByWSDir::CalcEditDist, &SortByWSDir, &(SortListFile[szIdx2])) );
                    }
                    if (thrdEditFunc.size())
                    {
                        for (_pThread = thrdEditFunc.begin(); _pThread != thrdEditFunc.end(); ++_pThread)
                        {
                            _pThread->join();
                        }
                    }
                    thrdEditFunc.clear();
                }
            }
#endif
            sort(SortListFile.begin(), SortListFile.end(), SortByWSDir);
            SMD_TIME_END_TAG((L"EditCompare: "), EditCompare);

            FindListFile.clear();
            for (szIdx2 = 0; szIdx2 < SortListFile.size(); ++szIdx2)
            {
                FindListFile.push_back(SortListFile[szIdx2].str);
            }

            if (FindListFile.size() != SearchResult.size())
            {
                //WSPrint(L"  - [FINDINGS COUTING ERR.]\n");
                //WSPrint(L"    Restult[%d],FindListCount[%d]\n", SearchResult.size(), FindListFile.size());
            }

            if ((lcParam->enumFileOp & FileOpActEmnu::ChkRefDirMatch) && FindListFile.size())
            {
                tmpWStr = SortListFile[0].str;
                FindListFile.clear();
                // EDIT_NULL means that there is only one item, no need to compare.
                if (SortListFile[0].EditDist == EditSortStr::EDIT_NULL)
                {
                    csSortByWSDir::CalcEditDist(&SortByWSDir, &(SortListFile[0]));
                }
                if (SortListFile[0].EditDist == 0)
                {
                    FindListFile.push_back(tmpWStr);
                }
            }

            SMD_TIME_START_TAG(FileCompare);
            szRetValue = 0;
            bSkipFile = false;
            fileOpExtParam.szTempCmpMatch = 0;
            fileOpExtParam.destFileBuf = NULL;
            fileOpExtParam.destFileBufSize = 0;
            if (FindListFile.size())
            {
                if (false == GetFileBuf(DestListFile[destFileIdx].c_str(), &(fileOpExtParam.destFileBuf), &(fileOpExtParam.destFileBufSize)))
                {
                    // GetFileBuf - Error
                    fileOpExtParam.szReadFileErr++;
                    *(fileOpExtParam.ReadDestFileFailed) = true;
                }
            }
            for (szIdx2 = 0; szIdx2 < FindListFile.size() && (bSkipFile == false) && (false == gAbortTerminal); ++szIdx2)
            {
#if COMPARE_THREAD
                // Using the 0 == szIdx2, because the first might will be the nearest edit distance.
                //   with direct calling will be fast than thread compare
                if ((0 == szIdx2) || (false == lcParam->boolCmpUseThrd))
                {
                    FileOpAction(DestListFile[destFileIdx].c_str(), FindListFile[szIdx2].c_str(), lcParam->enumFileOp, &fileOpExtParam);
                }
                else
                {
                    threads.push_back(std::thread(FileOpAction, DestListFile[destFileIdx].c_str(), FindListFile[szIdx2].c_str(), lcParam->enumFileOp, &fileOpExtParam));
                    if (lcParam->MaxThread == threads.size())
                    {
                        _pThread = threads.begin();
                        _pThread->join();
                        threads.erase(_pThread);
                    }
                }
                continue;
#else
                szRetValue = FileOpAction(DestListFile[szIdx].c_str(), FindListFile[szIdx2].c_str(), lcParam->enumFileOp, &fileOpExtParam);
                if (szRetValue)
                {
                    // Fail, Add to Skip List
                    WSPrint(L"  -- Skip\n");
                    break;
                }
#endif
            }
#if COMPARE_THREAD
            if (threads.size())
            {
                for (_pThread = threads.begin(); _pThread != threads.end(); ++_pThread)
                {
                    _pThread->join();
                }
                threads.clear();
            }
#endif
            if (fileOpExtParam.destFileBuf)
            {
                delete[] fileOpExtParam.destFileBuf;
                fileOpExtParam.destFileBuf = NULL;
            }
            SMD_TIME_END_TAG((L"FileCompare: "), FileCompare);

            if (0 == FindListFile.size())
            {
                WSPrint(L"  - [NOT FOUND]\n");
            }

            if (lcParam->enumFileOp & FileOpActEmnu::FileDiffCpy)
            {
                if (0 == fileOpExtParam.szTempCmpMatch)
                {
                    tmpWStr = DestListFile[destFileIdx];
                    tmpWStr = tmpWStr.substr(tmpWStr.find(lcParam->DestDir) + wcslen(lcParam->DestDir.c_str()));
                    tmpWStr = lcParam->cpyDir + tmpWStr;
                    WSPrint(L"Copy [%s]\n", DestListFile[destFileIdx].c_str());
                    WSPrint(L"  To [%s]\n", tmpWStr.c_str());
                    szTmpVal = SystemCpyFile(tmpWStr.c_str(), DestListFile[destFileIdx].c_str());
                    if (szTmpVal)
                    {
                        fileOpExtParam.szSysCpyFileErr++;
                    }
                }
            }

            if (fileOpExtParam.szTempCmpMatch)
            {
                fileOpExtParam.szMatchCount++;
            }

        }

        if (lcParam->enumFileOp & FileOpActEmnu::RemoveEmptyDir)
        {
            RemoveEmptyFolder(lcParam->DestDir.c_str());
        }

        // we still need to disply the error message, enable the ConOut
        gWsPrint.DisConOutSilent();
        WSPrint(L"\nSystem Report:\n");
        WSPrint(L"  SrcFolder files   [%8d] : [%s]\n", SrcListFile.size(), lcParam->SrcDir.c_str());
        WSPrint(L"  DestFolder files  [%8d] : [%s]\n\n", DestListFile.size(), lcParam->DestDir.c_str());
        WSPrint(L"  Match             [%8d]\n", fileOpExtParam.szMatchCount);
        WSPrint(L"  DelFile           [%8d]\n", fileOpExtParam.szDelFileCount);
        
        WSPrint(L"\nSystem Error Report:\n");
        WSPrint(L"  Detected (Fail on Copy) error               [%4d]\n", fileOpExtParam.szSysCpyFileErr);
        WSPrint(L"  Detected system Read file error(GetFileBuf) [%4d]\n", fileOpExtParam.szReadFileErr);

    } while (FALSE);

    if (gAbortTerminal)
    {
        WSPrint(L"\n[CTRL+C Terminal]\n");
    }

    if (false)
    {
        // Do not process the Suffix.clear (), it will cause too much time to walk through tree.
        WSPrint(L"\nResource release:\n");
        WSPrint(L"  -List vector clear()\n");
        FindListFile.clear();
        SortListFile.clear();
        DestListFile.clear();
        SrcListFile.clear();
        WSPrint(L"  -SuffixTree.clear()\n  ");
        gWsPrint.EnWaitLongFunc();
        suffixTree.clear();
        gWsPrint.DisWaitLongFunc();
        WSPrint(L"\n  suffixTree, newAlloc [%10d], delRqst [%10d]\n", newAlloc, delRqst);
    }


    return szRetValue;
}

wstring CovFullPath(const wchar_t *sPath)
{
    wchar_t         *tmpStrBuf;
    wstring         retWStr;
    tmpStrBuf = new wchar_t[MAX_PATH*2];
    if (_wfullpath(tmpStrBuf, sPath, MAX_PATH*2))
    {
        retWStr = tmpStrBuf;
    }
    else
    {
        // The Path or file did not recognize
    }
    //Free Resource
    delete [] tmpStrBuf;

    return retWStr;
}

void SignalHandler(int signal)
{
    size_t          szTimeOut = 60 * 1000;   // 60 sec.
    const size_t    DURATION = 10;

    gAbortTerminal = true;

    szTimeOut /= DURATION;
    while (gAbortTerminal && szTimeOut)
    {
        // wait the problem terminal
        this_thread::sleep_for(chrono::milliseconds(DURATION));
        --szTimeOut;
    }

    abort();
}

int wmain(int argc, wchar_t* argv[], wchar_t* envp[])
{
    int             nIdx;
    wstring         WTmpStr;
    enum class      ParamAct { None, ChkValidate, ProcListClear, ProcListCmp, logConOut, CopyDiff, ProcListCmpAll, CopyFileDiff
    };
    ParamAct        ExeAct = ParamAct::None;
    wstring         OutFile;
    wstring         SrcFile;
    wstring         CopyDest;
    ListClearParam  lcParam;
    char*           prev_loc = NULL;
    size_t          enumExtFileOp = FileOpActEmnu::None;
    bool            boolCmpUseThrd = true;
    bool            bAlgoEditDist = false;

    // Get the current deautlt local name.
    // Sets the locale to the ANSI code page obtained from the operating system.
    // The locale name is set to the value returned by GetUserDefaultLocaleName.
    prev_loc = std::setlocale(LC_ALL, ".ACP");
    std::setlocale(LC_ALL, "en_US.UTF-8");

    for (nIdx = 0; nIdx < argc; ++nIdx)
    {
        //WSPrint(L"[%d]: argv[%s]\n", nIdx, argv[nIdx]);
        WTmpStr = argv[nIdx];

        if (0 == WTmpStr.compare(L"-silent"))
        {
            enumExtFileOp |= FileOpActEmnu::SilentMode;
            continue;
        }

        if (0 == WTmpStr.compare(L"-disCmpUseThrd"))
        {
            boolCmpUseThrd = false;
            continue;
        }

        if (0 == WTmpStr.compare(L"-d"))
        {
            OutFile = CovFullPath(argv[++nIdx]);
            continue;
        }

        if (0 == WTmpStr.compare(L"-listCmp"))
        {
            ExeAct = ParamAct::ProcListCmp;
            continue;
        }

        if (0 == WTmpStr.compare(L"-listCmpAll"))
        {
            ExeAct = ParamAct::ProcListCmpAll;
            continue;
        }

        if (0 == WTmpStr.compare(L"-listClear"))
        {
            ExeAct = ParamAct::ProcListClear;
            continue;
        }

        if (0 == WTmpStr.compare(L"-cpyDiff"))
        {
            ExeAct = ParamAct::CopyDiff;
            CopyDest = CovFullPath(argv[++nIdx]);
            if (0 == CopyDest.size())
            {
                CopyDest = argv[nIdx];
            }
            continue;
        }

        if (0 == WTmpStr.compare(L"-src"))
        {
            SrcFile = CovFullPath(argv[++nIdx]);
            continue;
        }
        if (0 == WTmpStr.compare(L"-log"))
        {
            ConOutFile(CovFullPath(argv[++nIdx]).c_str());
        }
        if (0 == WTmpStr.compare(L"-cpyFileDiff"))
        {
            ExeAct = ParamAct::CopyFileDiff;
            CopyDest = CovFullPath(argv[++nIdx]);
            if (0 == CopyDest.size())
            {
                CopyDest = argv[nIdx];
            }
            continue;
        }
    }

    do
    {
        if (ExeAct == ParamAct::ProcListClear || ExeAct == ParamAct::ProcListCmp || ExeAct == ParamAct::CopyDiff
            || ExeAct == ParamAct::ProcListCmpAll || ExeAct == ParamAct::CopyFileDiff)
        {
            if (ExeAct == ParamAct::ProcListClear)
            {
                lcParam.enumFileOp |= (FileOpActEmnu::FileCmpDel | FileOpActEmnu::RemoveEmptyDir);
                boolCmpUseThrd = true;
                bAlgoEditDist = true;
            }
            if (ExeAct == ParamAct::CopyDiff)
            {
                lcParam.enumFileOp |= (FileOpActEmnu::FileDiffCpy | FileOpActEmnu::ChkRefDirMatch);
                boolCmpUseThrd = false;
                bAlgoEditDist = false;
            }
            if (ExeAct == ParamAct::CopyFileDiff)
            {
                lcParam.enumFileOp |= FileOpActEmnu::FileDiffCpy;
                boolCmpUseThrd = true;
                bAlgoEditDist = true;
            }
            if (ExeAct == ParamAct::ProcListCmpAll)
            {
                lcParam.enumFileOp |= FileOpActEmnu::FileListAllRst;
            }

            lcParam.DestDir = OutFile;
            lcParam.SrcDir = SrcFile;
            lcParam.cpyDir = CopyDest;
            lcParam.MaxThread = 0;
            lcParam.enumFileOp |= FileOpActEmnu::FileCmp;
            lcParam.enumFileOp |= enumExtFileOp;
            lcParam.boolCmpUseThrd = boolCmpUseThrd;
            lcParam.bAlgoEditDist = bAlgoEditDist;
            signal(SIGINT, SignalHandler);
            ProcFileOp(&lcParam);
            gAbortTerminal = false;
        }

        if (ExeAct == ParamAct::None)
        {
            WTmpStr =  L" ListDup.exe <parameters>          VER(1.18)\n";
            WTmpStr += L"  -d   <Directory>     : Set (Dest) Directory\n";
            WTmpStr += L"  -src <Directory>     : Set (Src) Directory\n";
            WTmpStr += L"  -listClear           : Proc ListClear Action\n";
            WTmpStr += L"  -listCmp             : Proc list compare match Action\n";
            WTmpStr += L"  -listCmpAll          : Proc list compare all file Action\n";
            WTmpStr += L"  -cpyDiff <cpyDir>    : copy -d <Dir> diff to <cpyDir>\n";
            WTmpStr += L"                         Only compare the reference Dir, in the same Dir structure\n";
            WTmpStr += L"  -cpyFileDiff <cpyDir>: copy -d <Dir> diff to <cpyDir>\n";
            WTmpStr += L"                         Find the same file name in different folder\n";
            WTmpStr += L"  -log <File>          : Log the console output\n";
            WTmpStr += L"  -silent              : Silent mode, no console out\n";

            WSPrint(L"%s", WTmpStr.c_str());
        }
    } while (FALSE);

    //std::setlocale(LC_ALL, prev_loc);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started:
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
