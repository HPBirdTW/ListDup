#include <iostream>
#include <string>
#include "SuffixTree.h"


using namespace std;

UINT8 SuffixTree::UNIQUE_CHAR = 0;

// The newAlloc / delRqst golbal value that is for use Malloc() and Free(), that we should be the same.
size_t newAlloc = 0;
size_t delRqst = 0;


End::End(int end)
{
    this->end = end;
}

End::End(const End& obj) : end(obj.end) 
{
}


void* End::operator new(std::size_t count)
{
    SuffixNodePtr   node = NULL;

    node = (SuffixNodePtr)std::malloc(count);
    if (node == NULL)
    {
        throw std::bad_alloc{};
    }

    newAlloc += count;

    return node;
}

void End::operator delete(void* ptr) noexcept
{
    delRqst += sizeof(End);

    if (ptr)
        std::free(ptr);
}

void End::operator delete(void* ptr, std::size_t size) noexcept
{
    delRqst += size;

    if (ptr)
        std::free(ptr);
}

End::End() : end(-1) 
{
}


void* End::operator new[](std::size_t count)
{
    SuffixNodePtr   node = NULL;

    node = (SuffixNodePtr)std::malloc(count);
    if (node == NULL)
    {
        throw std::bad_alloc{};
    }

    newAlloc += count;

    return node;
}

#if USE_SUFFIX_ARRAY == 0
SuffixNodePtr SuffixNode::GetChildNode(int charIdx, VECT_CHILDNODE* _child)
{
    for (auto& obj : *_child)
    {
        if (obj.charIdx == charIdx)
            return obj.node;
    }

    return NULL;
}

void SuffixNode::SetChildNode(int charIdx, SuffixNodePtr node, VECT_CHILDNODE* _child)
{
    bool        exist = false;
    ChildNode   childObj;

    for (auto& obj : *_child)
    {
        if (obj.charIdx == charIdx)
        {
            obj.node = node;
            exist = true;
        }
    }
    if (exist == false && charIdx < SuffixNode::TOTAL)
    {
        childObj.charIdx = charIdx;
        childObj.node = node;
        _child->push_back(childObj);
    }
}
#endif

SuffixNodePtr SuffixNode::createNode(int start, EndPtr end)
{
    SuffixNodePtr node = new SuffixNode;
    node->start = start;
    node->end = end;
    node->index = 0;
    return node;
}


void SuffixNode::operator delete(void* ptr) noexcept
{
    delRqst += sizeof(SuffixNode);

    if (ptr)
        std::free(ptr);
}

void SuffixNode::operator delete(void* ptr, std::size_t size) noexcept
{
    delRqst += size;

    if (ptr)
        std::free(ptr);
}


SuffixNode::SuffixNode()
{
    start = 0;
    end = NULL;
    index = 0;
#if USE_SUFFIX_ARRAY == 0
    child.clear();
#else
    for (int idx = 0; idx < TOTAL; ++idx)
    {
        child[idx] = NULL;

    }
#endif
    suffixLink = NULL;
};

SuffixNode::SuffixNode(const SuffixNode& obj) :start(obj.start), end(obj.end), index(obj.index), suffixLink(obj.suffixLink)
{
    int     idx;
    for (idx = 0; idx < TOTAL; ++idx)
    {
        child[idx] = obj.child[idx];
    }
}

void* SuffixNode::operator new(std::size_t count)
{
    SuffixNodePtr   node = NULL;

    newAlloc += count;

    node = (SuffixNodePtr)std::malloc(count);
    if (node == NULL)
    {
        throw std::bad_alloc{};
    }

    return node;
}

void* SuffixNode::operator new[](std::size_t count)
{
    SuffixNodePtr   node = NULL;

    newAlloc += count;

    node = (SuffixNodePtr)std::malloc(count);
    if (node == NULL)
    {
        throw std::bad_alloc{};
    }

    return node;
}


SuffixTree::SuffixTree() :root(NULL), input(NULL), active(), end(-1),
inputExtLen(0), remainingSuffixCount(0)
{
}

int SuffixTree::diff(SuffixNodePtr node)
{
    return node->end->end - node->start;
}

SuffixNodePtr SuffixTree::selectNode()
{
#if USE_SUFFIX_ARRAY == 0
    for (auto& obj : active.activeNode->child)
    {
        if (obj.charIdx == input[active.activeEdge])
        {
            return obj.node;
        }
    }
    return NULL;
#else
    return active.activeNode->child[input[active.activeEdge]];
#endif
}

SuffixNodePtr SuffixTree::selectNode(int NodeIdx)
{
#if USE_SUFFIX_ARRAY == 0
    for (auto& obj : active.activeNode->child)
    {
        if (obj.charIdx == input[NodeIdx])
        {
            return obj.node;
        }
    }

    return NULL;
#else
    return active.activeNode->child[input[NodeIdx]];
#endif
}

void SuffixTree::walkDown(int NodeIdx)
{
    SuffixNodePtr node = selectNode();
    //active length is greater than path edge length.
    //walk past current node so change active point.
    //This is as per rules of walk down for rule 3 extension.
    if (diff(node) < active.activeLength)
    {
        active.activeNode = node;
        active.activeLength = active.activeLength - diff(node);
#if USE_SUFFIX_ARRAY == 0
        active.activeEdge = SuffixNode::GetChildNode(input[NodeIdx], &node->child)->start;
#else
        active.activeEdge = node->child[input[NodeIdx]]->start;
#endif
    }
    else
    {
        active.activeLength++;
    }
}

UINT8 SuffixTree::nextChar(int i, bool* CreateInternalLeaf)
{
    SuffixNodePtr   node;
    bool            IsLastLeaf;
    do
    {
        IsLastLeaf = false;
        if (i == this->inputExtLen - 1)
        {
            IsLastLeaf = true;
        }
        node = selectNode();
        if (diff(node) >= active.activeLength)
        {
#if USE_SUFFIX_ARRAY == 0
            return input[SuffixNode::GetChildNode(input[active.activeEdge], &(active.activeNode->child))->start + active.activeLength];
#else
            return input[active.activeNode->child[input[active.activeEdge]]->start + active.activeLength];
#endif
        }

        if (diff(node) + 1 == active.activeLength)
        {
#if USE_SUFFIX_ARRAY == 0
            if ((SuffixNode::GetChildNode(input[i], &node->child) != NULL) && (IsLastLeaf == false) )
#else
            if ((node->child[input[i]] != NULL) && (IsLastLeaf == false) )
#endif
            {
                return input[i];
            }
            *CreateInternalLeaf = true;
            break;
        }
        else
        {
            active.activeNode = node;
            active.activeLength = active.activeLength - diff(node) - 1;
            active.activeEdge = active.activeEdge + diff(node) + 1;
        }
    } while (true);

    return UNIQUE_CHAR;
}

void SuffixTree::build(const UINT8* _input, int inputLen)
{
    if (_input == NULL || inputLen == 0)
        return;
    try
    {
        this->clear();
        this->inputExtLen = inputLen + 1;
        this->input = new UINT8[(size_t)this->inputExtLen];
        if (this->input)
        {
            memcpy(this->input, _input, inputLen);
            this->input[inputLen] = UNIQUE_CHAR;

            this->root = NULL;
            this->remainingSuffixCount = 0;
            this->build();
        }
    }
    catch (std::bad_alloc&)
    {
//        cout << "Failed to new operation\n";
        this->clear();
    }
    catch (...)
    {
//        cout << "Exception failed\n";
    }
}
#if VALIDATE_CHECK == 1

bool SuffixTree::validate()
{
    struct Param
    {
        SuffixNodePtr   root;
        int             val;
        Param() :root(NULL), val(0){}
    };
    vector<Param>   NodeStack;
    Param           paramNode;
    int             val = 0;
    bool*           pValidIdxBuf;
    size_t          FailValue = 0;
    size_t          tmpValue;
    SuffixNodePtr   node;
    VECT_INT        result;
    int             Idx = 0;

    paramNode.root = this->root;
    paramNode.val = 0;
    NodeStack.push_back(paramNode);

    wprintf(L"validate suffix tree:\n");
    pValidIdxBuf = new bool[this->inputExtLen];
    if (pValidIdxBuf == NULL)
    {
        wprintf(L"  [%d]: Fail to allocate buf\n", __LINE__);
        return false;
    }
    for (val = 0; val < this->inputExtLen; ++val)
    {
        pValidIdxBuf[val] = false;
    }

    FailValue = 0;
    do
    {
        paramNode = NodeStack[NodeStack.size() - 1];
        node = paramNode.root;
        val = paramNode.val;
        NodeStack.pop_back();

        if (node == NULL)
        {
            continue;
        }

        if (node == this->root)
        {
            // For root, don't need check.
        }
        else
        {
            val += node->end->end - node->start + 1;
            // internal node
            if (node->index == INNER_NODE_INDEX)
            {
                if (node->start > node->end->end)
                {
                    wprintf(L"  [%d]: Incorrect IntNode start(%d),end(%d)\n", __LINE__, node->start, node->end->end);
                    FailValue = -1;
                    break;
                }
                if (node->end == &this->end)
                {
                    wprintf(L"  [%d]: Incorrect IntNode end - It is root end\n", __LINE__);
                    FailValue = -1;
                    break;
                }

                tmpValue = 0;
                for (auto& obj : node->child)
                {
#if USE_SUFFIX_ARRAY == 0
                    if (obj.node != NULL)
#else
                    if (obj != NULL)
#endif
                    {
                        tmpValue++;
                    }
                }
                if (0 == tmpValue)
                {
                    wprintf(L"  [%d]: No child node in IntNode\n", __LINE__);
                    FailValue = -1;
                    break;
                }
            }
            // leaf
            else
            {
                if (node->index >= this->inputExtLen)
                {
                    wprintf(L"  [%d]: Out of range - leaf Index(%d)\n", __LINE__, node->index);
                    FailValue = -1;
                    break;
                }

                if (node->index != this->inputExtLen - val)
                {
                    wprintf(L"  [%d]: Incorrect leaf Index(%d)\n", __LINE__, node->index);
                    FailValue = -1;
                    break;
                }

                if (true == pValidIdxBuf[node->index])
                {
                    wprintf(L"  [%d]: duplicate leaf Index(%d)\n", __LINE__, node->index);
                    FailValue = -1;
                    break;
                }
                else
                {
                    pValidIdxBuf[node->index] = true;
                }

                if (node->end != &this->end)
                {
                    wprintf(L"  [%d]: Incorrect leaf end(%d)\n", __LINE__, node->index);
                    FailValue = -1;
                    break;
                }

                for (auto& obj : node->child)
                {
#if USE_SUFFIX_ARRAY == 0
                    if (obj.node != NULL)
#else
                    if (obj != NULL)
#endif
                    {
                        wprintf(L"  [%d]: leaf child not empty (%d)\n", __LINE__, node->index);
                        FailValue = -1;
                        break;
                    }
                }
            }
        }

        // For every child node
        for (auto& node : node->child)
        {
#if USE_SUFFIX_ARRAY == 0
            paramNode.root = node.node;
#else
            paramNode.root = node;
#endif
            paramNode.val = val;
            NodeStack.push_back(paramNode);
        }
    } while (NodeStack.size() && (FailValue == 0) );

    for (val = 0; val < this->inputExtLen; ++val)
    {
        if (pValidIdxBuf[val] == false)
        {
            wprintf(L"  [%d]: lose suffix tree leaf index(%d)\n", __LINE__, val);
            FailValue = -1;
            break;
        }
    }
#if 0
    // For every Index should have find
    for (Idx = 0; Idx < this->inputExtLen; ++Idx)
    {
        result.clear();
        this->findIdxBuf(this->input + Idx, (this->inputExtLen - 1) - Idx, &result);
        for (val = 0; val < result.size(); ++val)
        {
            if (Idx == result[val])
                break;
        }
        if (val == result.size())
        {
            wprintf(L"  [%d]: did not find suffix tree leaf index - expect Index[%d]\n", __LINE__, Idx);
            FailValue = -1;
            break;
        }
    }
#endif
    if (FailValue == 0)
    {
        wprintf(L"  Success\n");
    }
    delete[] pValidIdxBuf;
    NodeStack.clear();

    return FailValue == 0 ? true : false;
}

#endif

void SuffixTree::setIndexUsingDfs(SuffixNodePtr root, int size)
{
    struct Param
    {
        SuffixNodePtr   root;
        int             val;
    };
    vector<Param>   NodeStack;
    Param           paramNode;
    int             val = 0;
    
    paramNode.root = root;
    paramNode.val = val;
    NodeStack.push_back(paramNode);
    do
    {
        paramNode = NodeStack[NodeStack.size() - 1];
        root = paramNode.root;
        val = paramNode.val;
        NodeStack.pop_back();

        if (root == NULL)
        {
            continue;
        }

        val += root->end->end - root->start + 1;
        if (root->index != INNER_NODE_INDEX)
        {
            root->index = size - val;
            continue;
        }
        // Internal Node
        for (auto& node : root->child)
        {
#if USE_SUFFIX_ARRAY == 0
            paramNode.root = node.node;
#else
            paramNode.root = node;
#endif
            paramNode.val = val;
            NodeStack.push_back(paramNode);
        }
    } while (NodeStack.size());
    NodeStack.clear();
}
  
void SuffixTree::getLeafIndex(SuffixNodePtr node, int rootLen, VECT_INT* result)
{
    vector< SuffixNodePtr>  NodeStack;
    int                     valLen;
    VECT_INT                VectLenStack;

    NodeStack.push_back(node);
    valLen = rootLen + (node->end->end - node->start + 1);
    VectLenStack.push_back(valLen);
    do
    {
        node = NodeStack[NodeStack.size() - 1];
        NodeStack.pop_back();
        valLen = VectLenStack[VectLenStack.size() - 1];
        VectLenStack.pop_back();

        if (node == NULL)
        {
            continue;
        }

        if (node->index != INNER_NODE_INDEX)
        {
            // result->push_back(node->index);
            result->push_back((this->inputExtLen -1) - valLen);
            continue;
        }

        // Internal Node
        for (auto& node : node->child)
        {
#if USE_SUFFIX_ARRAY == 0
            NodeStack.push_back(node.node);
            VectLenStack.push_back(valLen + (node.node->end->end - node.node->start + 1));
#else
            NodeStack.push_back(node);
            VectLenStack.push_back(valLen + (node->end->end - node->start + 1));
#endif
        }
    } while (NodeStack.size());
    NodeStack.clear();
    VectLenStack.clear();
}

bool SuffixTree::findIdxBuf(UINT8* patternBuf, int bufLen, VECT_INT* result)
{
    int             Idx;
    int             tmpVal = 0;
    SuffixNodePtr   nodePtr = this->root;
    bool            matchResult = true;
    do
    {
        if (this->root == NULL)
            return false;

        for (Idx = 0; Idx < bufLen && matchResult; )
        {
#if USE_SUFFIX_ARRAY == 0
            if (SuffixNode::GetChildNode(patternBuf[Idx],&nodePtr->child) == NULL)
#else
            if (nodePtr->child[patternBuf[Idx]] == NULL)
#endif
            {
                matchResult = false;
                break;
            }

#if USE_SUFFIX_ARRAY == 0
            nodePtr = SuffixNode::GetChildNode(patternBuf[Idx], &nodePtr->child);
#else
            nodePtr = nodePtr->child[patternBuf[Idx]];
#endif
            tmpVal = diff(nodePtr) + 1 > (bufLen - Idx) ? (bufLen - Idx) : diff(nodePtr) + 1;
            if (memcmp(this->input + nodePtr->start, patternBuf + Idx, tmpVal))
            {
                matchResult = false;
                break;
            }
            Idx += tmpVal;
        }

        result->clear();
        if (matchResult)
        {
            getLeafIndex(nodePtr, (Idx - tmpVal), result);
        }
    } while (false);

    return matchResult;
}

void SuffixTree::clearLeafNode(SuffixNodePtr node)
{
    vector< SuffixNodePtr>  NodeStack;

    NodeStack.push_back(node);
    do
    {
        node = NodeStack[NodeStack.size() - 1];
        NodeStack.pop_back();

        if (node == NULL)
        {
            continue;
        }

        // Internal Node
        if (node->index == INNER_NODE_INDEX)
        {
            for (auto& node : node->child)
            {
#if USE_SUFFIX_ARRAY == 0
                NodeStack.push_back(node.node);
#else
                NodeStack.push_back(node);
#endif
            }
        }
        else
        {
            // Leaf
            // Destroctor by RootTree End
            node->end = NULL;
        }

        if (node->end)
            delete node->end;
        node->end = NULL;
#if USE_SUFFIX_ARRAY == 0
        node->child.clear();
#else
        for (int idx = 0; idx < SuffixNode::TOTAL; ++idx)
        {
            node->child[idx] = NULL;

        }
#endif
        delete node;
    } while (NodeStack.size());
    NodeStack.clear();
}

void SuffixTree::clear()
{
    if (this->root)
    {
        clearLeafNode(this->root);
        this->root = NULL;
    }
    if (this->input)
    {
        delete[] this->input;
        this->input = NULL;
    }

    this->inputExtLen = 0;
}

void SuffixTree::SuffixTree::dfsTraversal(VSPRINT printFunc)
{
    VECT_UINT8      result;
    for (auto& node : root->child)
    {
#if USE_SUFFIX_ARRAY == 0
        dfsTraversal(node.node, &result, printFunc);
#else
        dfsTraversal(node, &result, printFunc);
#endif
    }
}

void SuffixTree::dfsTraversal(SuffixNodePtr root, VECT_UINT8* result, VSPRINT printFunc)
{
    int         i;
    UINT8* strBuf;
    if (root == NULL)
    {
        return;
    }

    if (root->index != INNER_NODE_INDEX)
    {
        for (i = root->start; i <= root->end->end; i++)
        {
            result->push_back(input[i]);
        }

        strBuf = new UINT8[result->size()];
        if (strBuf)
        {
            i = 0;
            for (auto& obj : *result)
            {
                strBuf[i++] = obj;
            }
            if (printFunc)
            {
                printFunc(strBuf, (int)result->size(), root->index);
            }
            delete[] strBuf;
        }

        for (i = root->start; i <= root->end->end; i++)
        {
            result->pop_back();
        }

        return;
    }

    for (i = root->start; i <= root->end->end; i++)
    {
        result->push_back(input[i]);
    }

    for (auto& node : root->child)
    {
#if USE_SUFFIX_ARRAY == 0
        dfsTraversal(node.node, result, printFunc);
#else
        dfsTraversal(node, result, printFunc);
#endif
    }

    for (i = root->start; i <= root->end->end; i++)
    {
        result->pop_back();
    }
}

void SuffixTree::build()
{
    int     i;
    root = SuffixNode::createNode(1, new End(0));
    root->index = INNER_NODE_INDEX;
    active.activeNode = this->root;
    active.activeLength = 0;
    end.end = -1;
    for (i = 0; i < inputExtLen; ++i)
    {
        startPhase(i);
    }

    if (remainingSuffixCount != 0)
    {
        std::cout << "Something wrong happened " << remainingSuffixCount << endl;
    }

    //finally walk the tree again and set up the index.
#if VALIDATE_CHECK == 1
    setIndexUsingDfs(root, inputExtLen);
#endif
}

void SuffixTree::startPhase(int i)
{
    //set lastCreatedInternalNode to null before start of every phase.
    SuffixNodePtr       lastCreatedInternalNode = NULL;
    bool                createIntLeaf = false;
    UINT8               ch;
    bool                IsLastLeaf = false;

    //global end for leaf. Does rule 1 extension as per trick 3 by incrementing end.
    end.end++;

    if (i == this->inputExtLen-1)
    {
        IsLastLeaf = true;
    }

    //these many suffixes need to be created.
    remainingSuffixCount++;
    while (remainingSuffixCount > 0)
    {
        //if active length is 0 then look for current character from root.
        if (active.activeLength == 0)
        {
            //if current character from root is not null then increase active length by 1
            //and break out of while loop. This is rule 3 extension and trick 2 (show stopper)
            if (selectNode(i) != NULL && IsLastLeaf == false)
            {
                active.activeEdge = selectNode(i)->start;
                active.activeLength++;
                break;
            }
            else
            {
                //create a new leaf node with current character from leaf. This is rule 2 extension.
                if (IsLastLeaf == false)
                {
#if USE_SUFFIX_ARRAY == 0
                    SuffixNode::SetChildNode(input[i], SuffixNode::createNode(i, &end), &root->child);
#else
                    root->child[input[i]] = SuffixNode::createNode(i, &end);
#endif
                }
                else
                {
#if USE_SUFFIX_ARRAY == 0
                    SuffixNode::SetChildNode(SuffixNode::TOTAL - 1, SuffixNode::createNode(i, &end), &root->child);
#else
                    root->child[SuffixNode::TOTAL - 1] = SuffixNode::createNode(i, &end);
#endif
                }

                remainingSuffixCount--;
            }
        }
        else
        {
            //if active length is not 0 means we are traversing somewhere in middle. So check if next character is same as
            //current character. 
            createIntLeaf = false;
            ch = nextChar(i, &createIntLeaf);
            if (createIntLeaf == false)
            {
                //if next character is same as current character then do a walk down. This is again a rule 3 extension and
                //trick 2 (show stopper).
                if (ch == input[i] && IsLastLeaf == false)
                {
                    //if lastCreatedInternalNode is not null means rule 2 extension happened before this. Point suffix link of that node
                    //to selected node using active point.
                    //TODO - Could be wrong here. Do we only do this if when walk down goes past a node or we do it every time.
                    if (lastCreatedInternalNode != NULL)
                    {
                        lastCreatedInternalNode->suffixLink = selectNode();
                    }
                    //walk down and update active node if required as per rules of active node update for rule 3 extension.
                    walkDown(i);
                    break;
                }
                else
                {
                    //next character is not same as current character so create a new internal node as per 
                    //rule 2 extension.
                    SuffixNodePtr newLeafNode = NULL;
                    SuffixNodePtr newInternalNode = NULL;
                    SuffixNodePtr node = selectNode();
                    int oldStart = node->start;
                    node->start = node->start + active.activeLength;
                    //create new internal node
                    newInternalNode = SuffixNode::createNode(oldStart, new End(oldStart + active.activeLength - 1));

                    //create new leaf node
                    newLeafNode = SuffixNode::createNode(i, &this->end);

                    //set internal nodes child as old node and new leaf node.
#if USE_SUFFIX_ARRAY == 0
                    SuffixNode::SetChildNode(input[newInternalNode->start + active.activeLength], node, &(newInternalNode->child));
#else
                    newInternalNode->child[input[newInternalNode->start + active.activeLength]] = node;
#endif
                    if (IsLastLeaf == false)
                    {
#if USE_SUFFIX_ARRAY == 0
                        SuffixNode::SetChildNode(input[i], newLeafNode, &(newInternalNode->child));
#else
                        newInternalNode->child[input[i]] = newLeafNode;
#endif
                    }
                    else
                    {
#if USE_SUFFIX_ARRAY == 0
                        SuffixNode::SetChildNode(SuffixNode::TOTAL - 1, newLeafNode, &(newInternalNode->child));
#else
                        newInternalNode->child[SuffixNode::TOTAL - 1] = newLeafNode;
#endif
                    }
                    newInternalNode->index = INNER_NODE_INDEX;
#if USE_SUFFIX_ARRAY == 0
                    SuffixNode::SetChildNode(input[newInternalNode->start], newInternalNode, &(active.activeNode->child));
#else
                    active.activeNode->child[input[newInternalNode->start]] = newInternalNode;
#endif

                    //if another internal node was created in last extension of this phase then suffix link of that
                    //node will be this node.
                    if (lastCreatedInternalNode != NULL)
                    {
                        lastCreatedInternalNode->suffixLink = newInternalNode;
                    }
                    //set this guy as lastCreatedInternalNode and if new internalNode is created in next extension of this phase
                    //then point suffix of this node to that node. Meanwhile set suffix of this node to root.
                    lastCreatedInternalNode = newInternalNode;
                    newInternalNode->suffixLink = root;

                    //if active node is not root then follow suffix link
                    if (active.activeNode != root)
                    {
                        active.activeNode = active.activeNode->suffixLink;
                    }
                    //if active node is root then increase active index by one and decrease active length by 1
                    else
                    {
                        active.activeEdge = active.activeEdge + 1;
                        active.activeLength--;
                    }
                    remainingSuffixCount--;
                }
            }
            else
            {
                //this happens when we are looking for new character from end of current path edge. Here we already have internal node so
                //we don't have to create new internal node. Just create a leaf node from here and move to suffix new link.
                SuffixNodePtr node = selectNode();
                if (IsLastLeaf == false)
                {
#if USE_SUFFIX_ARRAY == 0
                    SuffixNode::SetChildNode(input[i], SuffixNode::createNode(i, &this->end), &node->child);
#else
                    node->child[input[i]] = SuffixNode::createNode(i, &this->end);
#endif
                }   
                else
                {
#if USE_SUFFIX_ARRAY == 0  
                    SuffixNode::SetChildNode(SuffixNode::TOTAL - 1, SuffixNode::createNode(i, &this->end), &node->child);
#else
                    node->child[SuffixNode::TOTAL - 1] = SuffixNode::createNode(i, &this->end);
#endif
                }

                if (lastCreatedInternalNode != NULL)
                {
                    lastCreatedInternalNode->suffixLink = node;
                }
                lastCreatedInternalNode = node;
                //if active node is not root then follow suffix link
                if (active.activeNode != root)
                {
                    active.activeNode = active.activeNode->suffixLink;
                }
                //if active node is root then increase active index by one and decrease active length by 1
                else
                {
                    active.activeEdge = active.activeEdge + 1;
                    active.activeLength--;
                }

                remainingSuffixCount--;
            }
        }
    }
}