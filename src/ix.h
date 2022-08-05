

#ifndef IX_H
#define IX_H


#include "redbase.h" 
#include "rm_rid.h"  
#include "pf.h"
#include <string>
#include <stdlib.h>
#include <cstring>
//////////////////////////////////////////////////////定义结构体/////////////////////////////////////
// 定义索引数据页的头部，来记录该节点的基本信息
struct IX_IndexHeader {
    AttrType attr_type; // 参数长度和类型
    int attr_length;

    int entryOffset_N;  // 节点入口的偏移量
    int entryOffset_B;  // bucket的入口偏移量

    int keysOffset_N;   // 属性的偏移量

    int maxKeys_N;      // 最大数量的bucket和节点
    int maxKeys_B;

    PageNum rootPage;   //找到根节点所在的page
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////创建manager类///////////////////////////////////////
//
// IX_Manager: provides IX index file management
//
class IX_Manager {
    static const char UNOCCUPIED = 'u';
public:
    IX_Manager(PF_Manager& pfm);
    ~IX_Manager();


    // 打开一个索引
    RC OpenIndex(const char* fileName, int indexNo,
        IX_IndexHandle& indexHandle);

    //关闭一个索引
    RC CloseIndex(IX_IndexHandle& indexHandle);


    //创建新的索引
    RC CreateIndex(const char* fileName, int indexNo,
        AttrType attrType, int attrLength);

    // 销毁一个索引
    RC DestroyIndex(const char* fileName, int indexNo);



private:

    //通过文件名和索引号来获得索引名
    RC GetIndexFileName(const char* fileName, int indexNo, std::string& indexname);
    //打开索引后，设置索引的内部相关参数
    RC SetUpIH(IX_IndexHandle& ih, PF_FileHandle& fh, struct IX_IndexHeader* header);
    //判断给的索引是可用的
    bool IsValidIndex(AttrType attrType, int attrLength);
    //关闭文件设置索引的内部参数
    RC CleanUpIH(IX_IndexHandle& indexHandle);

    PF_Manager& pfm; // 创建一个PF对象来利用PF_manager的方法。

};

//////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////创建IndexHandle类//////////////////////////////////////////
class IX_IndexHandle {
    friend class IX_Manager;
    friend class IX_IndexScan;
    static const int BEGINNING_OF_SLOTS = -2; // 类常量
    static const int END_OF_SLOTS = -3;
    static const char UNOCCUPIED = 'u';
    static const char OCCUPIED_NEW = 'n';
    static const char OCCUPIED_DUP = 'r';
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // 在打开的索引文件中插入一个新的索引，实现方式类似于在打开的文件页里面插入一条记录,属性值为pdata，记录为rid
    RC InsertEntry(void *pData, const RID &rid);

    // D在打开的索引文中中删除属性值为pdata，记录为rid的一条索引
    RC DeleteEntry(void *pData, const RID &rid);

    // 把handle的这个数据页全部从内存写到硬盘
    RC ForcePages();
    RC PrintIndex();
private:
    // 内部接口和辅助接口
    //在后面具体用到时再详细介绍
    RC CreateNewNode(PF_PageHandle &ph, PageNum &page, char *& nData, bool isLeaf);
    RC CreateNewBucket(PageNum &page);
    RC InsertIntoNonFullNode(struct IX_NodeHeader *nHeader, PageNum thisNodeNum, void *pData, const RID &rid);
    RC InsertIntoBucket(PageNum page, const RID &rid);
    RC SplitNode(struct IX_NodeHeader *pHeader, struct IX_NodeHeader *oldHeader, PageNum oldPage, int index, 
        int &newKeyIndex, PageNum &newPageNum);
    RC FindPrevIndex(struct IX_NodeHeader *nHeader, int thisIndex, int &prevIndex);
    RC FindNodeInsertIndex(struct IX_NodeHeader *nHeader, 
        void* pData, int& index, bool& isDup);
    RC DeleteFromNode(struct IX_NodeHeader *nHeader, void *pData, const RID &rid, bool &toDelete);
    RC DeleteFromBucket(struct IX_BucketHeader *bHeader, const RID &rid, 
  bool &deletePage, RID &lastRID, PageNum &nextPage);
    RC DeleteFromLeaf(struct IX_NodeHeader_L *nHeader, void *pData, const RID &rid, bool &toDelete);
    bool isValidIndexHeader() const;
    static int CalcNumKeysNode(int attrLength);
    static int CalcNumKeysBucket(int attrLength);
    RC GetFirstLeafPage(PF_PageHandle &leafPH, PageNum &leafPage);
    RC FindRecordPage(PF_PageHandle &leafPH, PageNum &leafPage, void * key);
    bool isOpenHandle;     
    PF_FileHandle pfh;     
    bool header_modified;  
    PF_PageHandle rootPH; //创建一个PF_pagehandle的对象便于利用PF_pagehandle里面的方法。
    struct IX_IndexHeader header; 
    int (*comparator) (void * , void *, int);
    bool (*printer) (void *, int);

};
//////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////创建IndexScan类/////////////////////////////////////////


class IX_IndexScan {
    static const char UNOCCUPIED = 'u';    // class constants to check whether
    static const char OCCUPIED_NEW = 'n';  // a slot in a node is valid
    static const char OCCUPIED_DUP = 'r';
public:g
    IX_IndexScan();
    ~IX_IndexScan();

    //打开一个
    RC OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT);


    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID &rid);

    // Close index scan
    RC CloseScan();
private:
    RC BeginScan(PF_PageHandle &leafPH, PageNum &pageNum);
    // Sets up the scan private variables to the first entry within the given leaf
    RC GetFirstEntryInLeaf(PF_PageHandle &leafPH);
    // Sets up the scan private variables to the appropriate entry within the given leaf
    RC GetAppropriateEntryInLeaf(PF_PageHandle &leafPH);
    // Sets up the scan private variables to the first entry within this bucket
    RC GetFirstBucketEntry(PageNum nextBucket, PF_PageHandle &bucketPH);
    // Sets up the scan private variables to the next entry in the index
    RC FindNextValue(); 

    // Sets the RID
    RC SetRID(bool setCurrent);


    bool openScan;              // Indicator for whether the scan is being used
    IX_IndexHandle *indexHandle;// Pointer to the indexHandle that modifies the
                                // file that the scan will try to traverse

    // The comparison to determine whether a record satisfies given scan conditions
    bool (*comparator) (void *, void*, AttrType, int);
    int attrLength;     // Comparison type, and information about the value
    void *value;        // to compare to
    AttrType attrType;
    CompOp compOp;

    bool scanEnded;     // Indicators for whether the scan has started or 
    bool scanStarted;   // ended

    PF_PageHandle currLeafPH;   // Currently pinned Leaf and Bucket PageHandles
    PF_PageHandle currBucketPH; // that the scan is accessing

    char *currKey;              // the keys of the current record, and the following
    char *nextKey;              // two records after that
    char *nextNextKey;
    struct IX_NodeHeader_L *leafHeader;     // the scan's current leaf and bucket header
    struct IX_BucketHeader *bucketHeader;   // and entry and key pointers
    struct Node_Entry *leafEntries;
    struct Bucket_Entry *bucketEntries;
    char * leafKeys;
    int leafSlot;               // the current leaf and bucket slots of the scan
    int bucketSlot;
    PageNum currLeafNum;        // the current and next bucket slots of the scan
    PageNum currBucketNum;
    PageNum nextBucketNum;

    RID currRID;    // the current RID and the next RID in the scan
    RID nextRID;

    bool hasBucketPinned; // whether the scan has pinned a bucket or a leaf page
    bool hasLeafPinned;
    bool initializedValue; // Whether value variable has been initialized (malloced)
    bool endOfIndexReached; // Whether the end of the scan has been reached


    bool foundFirstValue;
    bool foundLastValue;
    bool useFirstLeaf;
};



//
// 打印错误信息
//
void IX_PrintError(RC rc);

#define IX_BADINDEXSPEC         (START_IX_WARN + 0) // Bad Specification for Index File
#define IX_BADINDEXNAME         (START_IX_WARN + 1) // Bad index name
#define IX_INVALIDINDEXHANDLE   (START_IX_WARN + 2) // FileHandle used is invalid
#define IX_INVALIDINDEXFILE     (START_IX_WARN + 3) // Bad index file
#define IX_NODEFULL             (START_IX_WARN + 4) // A node in the file is full
#define IX_BADFILENAME          (START_IX_WARN + 5) // Bad file name
#define IX_INVALIDBUCKET        (START_IX_WARN + 6) // Bucket trying to access is invalid
#define IX_DUPLICATEENTRY       (START_IX_WARN + 7) // Trying to enter a duplicate entry
#define IX_INVALIDSCAN          (START_IX_WARN + 8) // Invalid IX_Indexscsan
#define IX_INVALIDENTRY         (START_IX_WARN + 9) // Entry not in the index
#define IX_EOF                  (START_IX_WARN + 10)// End of index file
#define IX_LASTWARN             IX_EOF

#define IX_ERROR                (START_IX_ERR - 0) // error
#define IX_LASTERROR            IX_ERROR

#endif
