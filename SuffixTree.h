#pragma once
#include <vector>
#include <string>

typedef class SuffixNode*       SuffixNodePtr;
typedef class End*              EndPtr;
typedef class Active*           ActivePtr;

typedef unsigned char           UINT8;
typedef std::vector<UINT8>      VECT_UINT8;
typedef std::vector<int>        VECT_INT;
typedef void (*VSPRINT)(UINT8*, int, int suffixIndex);

#define USE_SUFFIX_ARRAY        0
#define VALIDATE_CHECK          0

#if USE_SUFFIX_ARRAY == 0
struct ChildNode;
typedef std::vector<ChildNode>  VECT_CHILDNODE;
#endif

class EndOfPathException
{
};

class End
{
private:
    End();
    static void* operator new[](std::size_t count);

public:
    int end;

    End(int end);
    End(const End& obj);
    static void operator delete(void* ptr) noexcept;
    static void operator delete(void* ptr, std::size_t size) noexcept;
    static void* operator new(std::size_t count);
};

#if USE_SUFFIX_ARRAY == 0
struct ChildNode
{
    SuffixNodePtr   node;
    int             charIdx;
};
#endif

class SuffixNode
{
public:
    int     start;
    EndPtr  end;
    int     index;
    const static size_t TOTAL = 257;
#if USE_SUFFIX_ARRAY == 0
    VECT_CHILDNODE child;
#else
    SuffixNodePtr child[TOTAL];
#endif
    SuffixNodePtr suffixLink;
    static SuffixNodePtr createNode(int start, EndPtr end);
#if USE_SUFFIX_ARRAY == 0
    static SuffixNodePtr GetChildNode(int index, VECT_CHILDNODE* child);
    static void SetChildNode(int index, SuffixNodePtr node, VECT_CHILDNODE* child);
#endif
    static void operator delete(void* ptr) noexcept;
    static void operator delete(void* ptr, std::size_t size) noexcept;
private:
    SuffixNode();
    SuffixNode(const SuffixNode& obj);
    static void* operator new(std::size_t count);
    static void* operator new[](std::size_t count);
};

class Active
{
public:
    int             activeEdge;
    int             activeLength;
    SuffixNodePtr   activeNode;

    Active(SuffixNodePtr node)
    {
        activeLength = 0;
        activeNode = node;
        activeEdge = -1;
    }
    Active()
    {
        activeEdge = -1;
        activeLength = 0;
        activeNode = NULL;
    }
#if 0
    string ToString()
    {
        // "Active [activeNode=" + activeNode + ", activeIndex=" + activeEdge + ", activeLength=" + activeLength + "]"
        string  str;

        str += "Active [activeNode=";
        str += this->activeNode->toString();
        str += ", activeIndex=";
        str += to_string(this->activeEdge);
        str += ", activeLength=";
        str += to_string(this->activeLength);
        str += "]";

        return str;
    }
#endif
};

class SuffixTree
{
private:
    SuffixNodePtr       root;
    Active              active;
    int                 remainingSuffixCount;
    End                 end;
    UINT8*              input;
    int                 inputExtLen;
    const static int    INNER_NODE_INDEX = -2;

public:
    //    const static UINT8   UNIQUE_CHAR = '$';
    static UINT8        UNIQUE_CHAR;
#if 0
     SuffixTree(const UINT8* _input, int inputLen)
     {
       this->inputExtLen = inputLen+1;
       this->input = new UINT8[(size_t)this->inputExtLen];
       memcpy(this->input, _input, inputLen);
       this->input[inputLen] = UNIQUE_CHAR;

       this->root = NULL;
       this->remainingSuffixCount = 0;
     }
#endif
    SuffixTree();
    void build(const UINT8* _input, int inputLen);
    void clear();
    void dfsTraversal(VSPRINT func);
    bool findIdxBuf(UINT8* patternBuf, int bufLen, VECT_INT* result);
#if VALIDATE_CHECK == 1
    bool validate();
#endif
private:
    void build();
    void startPhase(int i);
    void getLeafIndex(SuffixNodePtr node, int rootLen, VECT_INT* result);
    void clearLeafNode(SuffixNodePtr node);
    int diff(SuffixNodePtr node);
    SuffixNodePtr selectNode();
    SuffixNodePtr selectNode(int NodeIdx);
    void walkDown(int NodeIdx);
    UINT8 nextChar(int i, bool* CreateInternalLeaf);
    void dfsTraversal(SuffixNodePtr root, VECT_UINT8* result, VSPRINT printFunc);
    void setIndexUsingDfs(SuffixNodePtr root, int size);
};
