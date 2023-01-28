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
#include "SuffixTree.h"
#include "AuxFunc.h"

using namespace std;

#define COMPARE_THREAD          1
// Threshold for Starting enable multi-thread calcualte Editdistance
// 0: Disable the Multi-thread CompareFunction
#define THRHD_MULTI_EDITDIST    0
#define USING_EDIT_DISTANCE     0

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
    ListClearParam() : MaxThread(0), DestDir(), SrcDir(), enumFileOp(0), cpyDir(), boolCmpUseThrd(true) {}
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
    bool operator() (EditSortStr& a, EditSortStr& b)
    {
        wstring aCutOffStr;
        wstring bCutOffStr;

        if (a.EditDist == EditSortStr::EDIT_NULL)
        {
            aCutOffStr = a.str;
            if (cutOffDir.size())
            {
                aCutOffStr = aCutOffStr.substr(aCutOffStr.find(cutOffDir.c_str()) + cutOffDir.size());
            }
#if USING_EDIT_DISTANCE
            a.EditDist = EditDistance(exptWStr.c_str(), (int)exptWStr.length(), aCutOffStr.c_str(), (int)aCutOffStr.length());
#else
            a.EditDist = lstrcmp(exptWStr.c_str(), aCutOffStr.c_str());
            if (EditSortStr::EDIT_NULL == a.EditDist)
            {
                --a.EditDist;
            }
#endif
        }

        if (b.EditDist == EditSortStr::EDIT_NULL)
        {
            bCutOffStr = b.str;
            if (cutOffDir.size())
            {
                bCutOffStr = bCutOffStr.substr(bCutOffStr.find(cutOffDir.c_str()) + cutOffDir.size());
            }
#if USING_EDIT_DISTANCE
            b.EditDist = EditDistance(exptWStr.c_str(), (int)exptWStr.length(), bCutOffStr.c_str(), (int)bCutOffStr.length());
#else
            b.EditDist = lstrcmp(exptWStr.c_str(), bCutOffStr.c_str());
            if (EditSortStr::EDIT_NULL == b.EditDist)
            {
                --b.EditDist;
            }
#endif
        }
#if USING_EDIT_DISTANCE
        return a.EditDist < b.EditDist;
#else
        return (unsigned int)a.EditDist < (unsigned int)b.EditDist;
#endif
    }

    static int CalcEditDist(csSortByWSDir* _this, EditSortStr *a)
    {
        wstring aCutOffStr;

        if (a->EditDist == EditSortStr::EDIT_NULL)
        {
            aCutOffStr = a->str;
            if (_this->cutOffDir.size())
            {
                aCutOffStr = aCutOffStr.substr(aCutOffStr.find(_this->cutOffDir.c_str()) + _this->cutOffDir.size());
            }
#if USING_EDIT_DISTANCE
            a->EditDist = EditDistance(_this->exptWStr.c_str(), (int)_this->exptWStr.length(), aCutOffStr.c_str(), (int)aCutOffStr.length());
#else
            a->EditDist = lstrcmp(_this->exptWStr.c_str(), aCutOffStr.c_str());
            if (EditSortStr::EDIT_NULL == a->EditDist)
            {
                --(a->EditDist);
            }
#endif
        }
        return a->EditDist;
    }
};

void PrintBuf(UINT8* Buf, int sizeBuf)
{
    int i;
    for (i = 0; i < sizeBuf; ++i)
    {
        if ((i % 0x10) == 0)
        {
            WSPrint(L"\n");
        }

        WSPrint(L" %02x", Buf[i]);
    }
    WSPrint(L"\n");
}

void SPrint(UINT8* strBuf, int sizeBuf, int suffixIndex)
{
    int i;
    for (i = 0; i < sizeBuf; i++)
    {
//        WSPrint(L" %02x", strBuf[i]);
        WSPrint(L"%c", strBuf[i]);
    }

    WSPrint(L" [%d]\n", suffixIndex);
}

std::mutex g_mutex;
std::mutex g_mutex_thread;
int SuffixVldBuild(int cpuIdx,size_t MaxCpuThread, std::vector<wstring> *FileList)
{

    int             Idx;
    SuffixTree      st;
    BYTE*           FileBuf = NULL;
    size_t          FileBufSize = 0;
    bool            bSuccess;

    Idx = 0;
    for (auto &obj : *FileList)
    {
        if (cpuIdx == Idx % MaxCpuThread)
        {
            do
            {
                this_thread::sleep_for(chrono::milliseconds(200));
            } while (!g_mutex_thread.try_lock());
//            g_mutex_thread.lock();
            bSuccess = GetFileBuf(obj.c_str(), &FileBuf, &FileBufSize);
            g_mutex_thread.unlock();
            if (NULL == FileBuf || FileBufSize == 0)
            {
                g_mutex.lock();
                WSPrint(L"[%04d]: Fail ReadFile[%s]\n", Idx, obj.c_str());
                g_mutex.unlock();
            }
            else
            {
                g_mutex.lock();
                WSPrint(L"[%04d]: CpuId[%02d] - %s (0x%08x) \n", Idx, cpuIdx, obj.c_str(), (UINT)FileBufSize);
                g_mutex.unlock();
#if USE_SUFFIX_ARRAY == 0
                if (FileBufSize < 0x100000 * 20)
                {
                    st.build((UINT8*)FileBuf, (int)FileBufSize);
                    st.validate();
                }
                else
                {
                    g_mutex_thread.lock();
                    st.build((UINT8*)FileBuf, (int)FileBufSize);
                    st.validate();
                    g_mutex_thread.unlock();
                }
#else
                {
                    g_mutex_thread.lock();
                    st.build((UINT8*)FileBuf, (int)FileBufSize);
                    st.validate();
                    g_mutex_thread.unlock();
                }
#endif

                delete[] FileBuf;
            }
        }
        Idx++;
    }

    return 0;
}

int SuffixTestProc(const wchar_t* OutDir)
{
    std::vector<wstring>		ListFileContext;
    size_t                      szTmpVal;
    int                         RetValue = 0;
    BYTE*                       FileBuf = NULL;
    size_t                      FileBufSize = 0;
    VECT_INT                    result;
    std::vector<std::thread>	threads;
    std::vector<std::thread>::iterator	_pThread;
    size_t						ThreadCount = 0;
    SYSTEM_INFO					SystemInfo;
    size_t						LocalMaxThreadCount = 12;

    do
    {
        szTmpVal = ListAllFileByAttribue(OutDir, &ListFileContext, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_DIRECTORY);
        if (szTmpVal != 0)
        {
            RetValue = (int)szTmpVal;
            break;
        }

    } while (FALSE);

    GetSystemInfo(&SystemInfo);
    LocalMaxThreadCount = SystemInfo.dwNumberOfProcessors - 1;
    WSPrint(L"LocalMaxThreadCount %zd\n", LocalMaxThreadCount);

    for (szTmpVal = 0; szTmpVal < LocalMaxThreadCount; ++szTmpVal)
    {
        threads.push_back(std::thread(SuffixVldBuild, (int)szTmpVal, LocalMaxThreadCount, &ListFileContext));
        ++ThreadCount;
    }
    if (ThreadCount > 0)
    {
        _pThread = threads.end();
        --_pThread;
        for (szTmpVal = 0; szTmpVal < ThreadCount; ++szTmpVal)
        {
            (_pThread - szTmpVal)->join();
        }
        ThreadCount = 0;
    }
    threads.clear();
    return RetValue;
}

struct FileOpActExtParam
{
    volatile bool*  FileDeleted;
    volatile bool*  ReadDestFileFailed;
    volatile bool*  ReadSrcFileFailed;
    volatile int    MatchCount;
    FileOpActExtParam() :FileDeleted(NULL), ReadDestFileFailed(NULL), ReadSrcFileFailed(NULL), MatchCount(0) {}
};
size_t FileOpAction(const wchar_t* destFile, const wchar_t* srcFile, size_t multiOp, FileOpActExtParam* extParam = NULL)
{
    size_t              szTmpVal = 0;
    size_t              retVal = 0;
    bool                bFileCmpRst = false;
    BYTE*               destFileBuf = NULL;
    size_t              destFileBufSize = 0;
    BYTE*               srcFileBuf = NULL;
    size_t              srcFileBufSize = 0;
    static std::mutex   fileOpMutex;
    static std::mutex   fileOpMutex2;

    do
    {
        if (multiOp & FileOpActEmnu::FileCmp)
        {
            while (!fileOpMutex.try_lock())
            {
                this_thread::sleep_for(chrono::milliseconds(1));
            }

            if (extParam && extParam->ReadDestFileFailed)
            {
                if (true == *extParam->ReadDestFileFailed)
                {
                    fileOpMutex.unlock();
                    break;
                }
            }
            if (extParam && extParam->ReadSrcFileFailed)
            {
                if (true == *extParam->ReadSrcFileFailed)
                {
                    fileOpMutex.unlock();
                    break;
                }
            }


            if (multiOp & FileOpActEmnu::FileCmpDel)
            {
                if (extParam && extParam->FileDeleted)
                {
                    if (true == *extParam->FileDeleted)
                    {
                        fileOpMutex.unlock();
                        break;
                    }
                }
            }

            if (0 == (multiOp & FileOpActEmnu::FileListAllRst))
            {
                if (extParam)
                {
                    if (extParam->MatchCount)
                    {
                        fileOpMutex.unlock();
                        break;
                    }
                }
            }
            fileOpMutex.unlock();

            bFileCmpRst = GetFileBuf(destFile, &destFileBuf , &destFileBufSize);
            if (bFileCmpRst == false)
            {
                if (extParam && extParam->ReadDestFileFailed)    *extParam->ReadDestFileFailed = true;
                retVal = FileOpActEmnu::FileCmp;
                break;
            }

            bFileCmpRst = GetFileBuf(srcFile, &srcFileBuf , &srcFileBufSize);
            if (bFileCmpRst == false)
            {
                if (extParam && extParam->ReadSrcFileFailed)    *extParam->ReadSrcFileFailed = true;
                retVal = FileOpActEmnu::FileCmp;
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

            fileOpMutex2.lock();
            if (bFileCmpRst)
            {
                if (extParam) extParam->MatchCount++;
                WSPrint(L"  [Match]\n");
            }
            else
            {
                WSPrint(L"  [Diff]\n");
            }
            WSPrint(L"  %s\n  %s\n", destFile, srcFile);
            fileOpMutex2.unlock();
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
            }
            else
            {
                retVal = FileOpActEmnu::FileCmpDel;
            }

            if (extParam && extParam->FileDeleted)    *extParam->FileDeleted = true;
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
    if (destFileBuf)
        delete[] destFileBuf;
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
    std::wstring                tmp2WStr;
    size_t                      szTmpVal;
    size_t                      szIdx, szIdx2;
    size_t                      szRetValue = 0;
    int                         tmpVal;
    SuffixTree                  suffixTree;
    VECT_INT                    SearchResult;
    FileOpActExtParam           fileOpExtParam;
    volatile bool               bSkipFile;
    const wchar_t*              END_CHAR = L"$";
#if COMPARE_THREAD
    std::vector<std::thread>	threads;
    std::vector<std::thread>	thrdEditFunc;
    std::vector<std::thread>::iterator	_pThread;
#endif
    csSortByWSDir               SortByWSDir;
    SYSTEM_INFO                 SystemInfo;

    //WSPrint(L"[%d]: %s\n", __LINE__, __FUNCTIONW__);
    do
    {
        GetSystemInfo(&SystemInfo);
        lcParam->MaxThread = SystemInfo.dwNumberOfProcessors - 1;

        if (lcParam->SrcDir.size() == 0 || lcParam->DestDir.size() == 0)
        {
            szRetValue = -1;
            WSPrint(L"Line[%ld]: Error. %s\n", __LINE__, __FUNCTIONW__);
            break;
        }

        WSPrint(L"GetSrcFileList - %s\n", lcParam->SrcDir.c_str());
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

        WSPrint(L"GetDestFileList - %s\n", lcParam->DestDir.c_str());
        gWsPrint.EnWaitLongFunc();
        szTmpVal = ListAllFileByAttribue(lcParam->DestDir.c_str(), &DestListFile, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_DIRECTORY);
        gWsPrint.DisWaitLongFunc();
        if (szTmpVal != 0)
        {
            szRetValue = szTmpVal;
            WSPrint(L"Line[%ld]: Error. %s\n", __LINE__, __FUNCTIONW__);
            break;
        }

        tmp2WStr.clear();
        for (auto& obj : SrcListFile)
        {
            tmp2WStr += obj.c_str();
            tmp2WStr += END_CHAR;
        }

        WSPrint(L"[Build] Suffix Tree - size(0x%08x)\n", (UINT)(tmp2WStr.size() * sizeof(wchar_t)) );
        gWsPrint.EnWaitLongFunc();
        suffixTree.build((UINT8*)tmp2WStr.c_str(), (int)(tmp2WStr.size() * sizeof(wchar_t)) );
        gWsPrint.DisWaitLongFunc();
        WSPrint(L"  -- [Finish] Suffix Tree\n");
#if 0
        // Validate the Dest source suffix
        WSPrint(L"Validate Suffix Tree\n");
        szTmpVal = 0;
        for (szIdx = 0; szIdx < DestListFile.size(); ++szIdx)
        {
            ShortFileWStr = DestListFile[szIdx];
            ShortFileWStr = ShortFileWStr.substr(ShortFileWStr.find_last_of(L"\\/") + 1);
            tmpWStr = ShortFileWStr + END_CHAR;
            SearchResult.clear();
            suffixTree.findIdxBuf((UINT8*)tmpWStr.c_str(), (int)tmpWStr.size() * sizeof(wchar_t), &SearchResult);
            sort(SearchResult.begin(), SearchResult.end());
            if (0 == SearchResult.size())
            {
                WSPrint(L"  [%s]\n", DestListFile[szIdx].c_str());
                WSPrint(L"  Short[%s]\n", ShortFileWStr.c_str());
                WSPrint(L"  - [NOT FOUND]\n");
                szTmpVal++;

                for (auto& obj : SrcListFile)
                {
                    if (std::string::npos != obj.find(ShortFileWStr))
                    {
                        WSPrint(L"  - [BUT FOUND SRC] %s\n", obj.c_str());
                    }
                }
            }
        }
        WSPrint(L"  -- Total not match find file -%d\n", szTmpVal);
#endif
        fileOpExtParam.FileDeleted = &bSkipFile;
        fileOpExtParam.ReadDestFileFailed = &bSkipFile;
        fileOpExtParam.ReadSrcFileFailed = &bSkipFile;

        if (lcParam->enumFileOp & FileOpActEmnu::SilentMode)
        {
            gWsPrint.EnConOutSilent();
        }

        for (szIdx = 0; szIdx < DestListFile.size(); ++szIdx)
        {
            if (lcParam->enumFileOp & FileOpActEmnu::SilentMode)
            {
                gWsPrint.DisConOutSilent();
            }
            WSPrint(L"\n[%s]\n", DestListFile[szIdx].c_str());
            if (lcParam->enumFileOp & FileOpActEmnu::SilentMode)
            {
                gWsPrint.EnConOutSilent();
            }

            ShortFileWStr = DestListFile[szIdx];
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
                        if (DestListFile[szIdx] == SrcListFile[szTmpVal-1])
                        {
                            // The same file compare
                            WSPrint(L"    [Find_Same_File] - %s\n");
                            break;
                        }
                        SortListFile.push_back(SrcListFile[szTmpVal-1]);
                        break;
                    }
                }
            }
            SMD_TIME_END_TAG((L"GetMatchFile: "), GetFileList);

            SMD_TIME_START_TAG(EditCompare);
            tmpWStr = DestListFile[szIdx];
            tmpWStr = tmpWStr.substr(tmpWStr.find(lcParam->DestDir.c_str()) + lcParam->DestDir.size());
            SortByWSDir.exptWStr = tmpWStr;
            SortByWSDir.cutOffDir = lcParam->SrcDir.c_str();
#if COMPARE_THREAD
            if (true == lcParam->boolCmpUseThrd)
            {
                if (THRHD_MULTI_EDITDIST && (SortListFile.size() > THRHD_MULTI_EDITDIST))
                {
                    for (auto& obj : SortListFile)
                    {
                        thrdEditFunc.push_back(std::thread(csSortByWSDir::CalcEditDist, &SortByWSDir, &obj));
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
            fileOpExtParam.MatchCount = 0;
            for (szIdx2 = 0; szIdx2 < FindListFile.size() && (bSkipFile == false); ++szIdx2)
            {
#if COMPARE_THREAD
                if (false == lcParam->boolCmpUseThrd)
                {
                    FileOpAction(DestListFile[szIdx].c_str(), FindListFile[szIdx2].c_str(), lcParam->enumFileOp, &fileOpExtParam);
                }
                else
                {
                    threads.push_back(std::thread(FileOpAction, DestListFile[szIdx].c_str(), FindListFile[szIdx2].c_str(), lcParam->enumFileOp, &fileOpExtParam));
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
            SMD_TIME_END_TAG((L"FileCompare: "), FileCompare);

            if (0 == FindListFile.size())
            {
                WSPrint(L"  - [NOT FOUND]\n");
            }

            if (lcParam->enumFileOp & FileOpActEmnu::FileDiffCpy)
            {
                if (0 == fileOpExtParam.MatchCount)
                {
                    tmpWStr = DestListFile[szIdx];
                    tmpWStr = tmpWStr.substr(tmpWStr.find(lcParam->DestDir) + wcslen(lcParam->DestDir.c_str()));
                    tmp2WStr = lcParam->cpyDir;
                    tmp2WStr += tmpWStr;
                    WSPrint(L"Copy [%s]\n", DestListFile[szIdx].c_str());
                    WSPrint(L"  To [%s]\n", tmp2WStr.c_str());
                    SystemCpyFile(tmp2WStr.c_str(), DestListFile[szIdx].c_str());
                }
            }
        }

        if (lcParam->enumFileOp & FileOpActEmnu::RemoveEmptyDir)
        {
            RemoveEmptyFolder(lcParam->DestDir.c_str());
        }
    } while (FALSE);

    return szRetValue;
}

size_t RstVerifyFile(const wchar_t* dest)
{
    wifstream       openFile(dest);
    wstring         tmpWStr;
    wstring         destWStr;
    wstring         srcWStr;
    wchar_t*        p_wchar = NULL;
    BYTE*           destFileBuf = NULL;
    size_t          destFileBufSize = 0;
    BYTE*           srcFileBuf = NULL;
    size_t          srcFileBufSize = 0;
    bool            bSuccess;

    do
    {
        if (!openFile.is_open())
        {
            WSPrint(L"Fail to open file - %s\n", dest);
            break;
        }
        while (getline(openFile, tmpWStr))
        {
            if (0 == tmpWStr.size())
            {
                continue;
            }
            if (tmpWStr.find(L"[Match]") != std::string::npos)
            {
                // Read Next Two Line
                getline(openFile, destWStr);
                destWStr = destWStr.substr(wcslen(L"  "));
                //destWStr = wcstok_s(tmpStrBuf, L" \t", &p_wchar);

                getline(openFile, srcWStr);
                srcWStr = srcWStr.substr(wcslen(L"  "));
                //srcWStr = wcstok_s(tmpStrBuf, L" \t", &p_wchar);

                bSuccess = GetFileBuf(destWStr.c_str(), &destFileBuf, &destFileBufSize);
                if (false == bSuccess)
                {
                    // File Open File
                    continue;
                }
                bSuccess = GetFileBuf(srcWStr.c_str(), &srcFileBuf, &srcFileBufSize);
                if (false == bSuccess)
                {
                    // File Open File
                    continue;
                }

                // Compare the Two File
                WSPrint(L"\n  %s\n  %s\n", destWStr.c_str(), srcWStr.c_str());
                if (destFileBufSize == srcFileBufSize)
                {
                    if (0 == memcmp(destFileBuf, srcFileBuf, destFileBufSize))
                    {
                        WSPrint(L"  [Rst Match]\n");
                    }
                    else
                    {
                        WSPrint(L"  [Rst Diff]\n");
                    }
                }
            }
        }
    } while (FALSE);
    openFile.close();

    // Free Rsource
    if (destFileBuf) delete[] destFileBuf;
    if (srcFileBuf) delete[] srcFileBuf;

    return 0;
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

void SuffixTest()
{
    BYTE*       FileBuf = NULL;
    size_t      FileBufSize = 0;
    SuffixTree  st;
    const char* str = "mississippi";
    const char* findPattern = "iss";
    wchar_t     TempStrBuf[MAX_PATH];
    //            const UINT8 findPattern[5] = {0x92,0xa9,0xe6,0xd2,0x20};
    //            const UINT8 findPattern[5] = { 0x5d,0x0b,0xd0,0xd0,0x1d };
    VECT_INT    result;
    //            FileBuf = GetFileBuf(L"j:\\Download\\373635_ICH10_EDS_2.1.pdf", &FileBufSize);
    //FileBuf = GetFileBuf(L"j:\\Download\\bga1440_cfl_h62_5_ma_koz.dra", &FileBufSize);

    //            FileBuf = GetFileBuf(L"j:\\Download\\ACPI_5_1release.pdf", &FileBufSize);
    //st.build(FileBuf, (int)FileBufSize);
                st.build((UINT8*)str, (int)strlen(str));
                st.dfsTraversal(SPrint);
                st.validate();
    //            st.findIdxBuf((UINT8*)findPattern, sizeof(findPattern), &result);
    st.findIdxBuf((UINT8*)findPattern, (int)strlen(findPattern), &result);
    cout << endl << "Match List [" << result.size() << "]:\n";
    for (auto& obj : result)
    {
        wsprintf(TempStrBuf, (PTCHAR)L" %08x", obj);
        WSPrint(L"%s", TempStrBuf);
    }
    cout << endl;
}

int wmain(int argc, wchar_t* argv[], wchar_t* envp[])
{
    int             nIdx;
    wstring         WTmpStr;
    enum class      ParamAct { None, ChkValidate, ProcListClear, ProcListCmp, suffixValidate, logConOut, VerifyLog, CopyDiff, ProcListCmpAll, CopyFileDiff
    };
    ParamAct        ExeAct = ParamAct::None;
    wstring         OutFile;
    wstring         SrcFile;
    wstring         CopyDest;
    ListClearParam  lcParam;
    char*           prev_loc = NULL;
    size_t          enumExtFileOp = FileOpActEmnu::None;
    bool            boolCmpUseThrd = true;

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
        if (0 == WTmpStr.compare(L"-suffixVld"))
        {
            ExeAct = ParamAct::suffixValidate;
            continue;
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
        if (0 == WTmpStr.compare(L"-logVld"))
        {
            ExeAct = ParamAct::VerifyLog;
            OutFile = CovFullPath(argv[++nIdx]);
            continue;
        }
    }

    do
    {
        if (ExeAct == ParamAct::ProcListClear || ExeAct == ParamAct::ProcListCmp || ExeAct == ParamAct::CopyDiff
            || ExeAct == ParamAct::ProcListCmpAll || ExeAct == ParamAct::CopyFileDiff)
        {
            lcParam.DestDir = OutFile;
            lcParam.SrcDir = SrcFile;
            lcParam.cpyDir = CopyDest;
            lcParam.MaxThread = 0;
            lcParam.enumFileOp = FileOpActEmnu::FileCmp;
            lcParam.enumFileOp |= enumExtFileOp;
            lcParam.boolCmpUseThrd = boolCmpUseThrd;
            if (ExeAct == ParamAct::ProcListClear)
            {
                lcParam.enumFileOp |= (FileOpActEmnu::FileCmpDel | FileOpActEmnu::RemoveEmptyDir);
            }
            if (ExeAct == ParamAct::CopyDiff)
            {
                lcParam.enumFileOp |= (FileOpActEmnu::FileDiffCpy | FileOpActEmnu::ChkRefDirMatch);
            }
            if (ExeAct == ParamAct::CopyFileDiff)
            {
                lcParam.enumFileOp |= FileOpActEmnu::FileDiffCpy;
            }
            if (ExeAct == ParamAct::ProcListCmpAll)
            {
                lcParam.enumFileOp |= FileOpActEmnu::FileListAllRst;
            }
            ProcFileOp(&lcParam);
        }

        if (ExeAct == ParamAct::None)
        {
            WTmpStr =  L" ListDup.exe <parameters>          VER(1.12)\n";
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
            WTmpStr += L"  -disCmpUseThrd       : SomeTimes, thread will cost extra time\n";
            //WTmpStr += L"  -suffixVld           : Test SuffixAlgo\n";
            //WTmpStr += L"  -logVld <File>       : Verify the Result log\n";

            WSPrint(L"%s", WTmpStr.c_str());
        }

        if (ExeAct == ParamAct::suffixValidate)
        {
            SuffixTestProc(OutFile.c_str());
        }

        if (ExeAct == ParamAct::VerifyLog)
        {
            RstVerifyFile(OutFile.c_str());
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
