

#include <unistd.h>
#include <sys/types.h>
#include "pf.h"
#include "ix_internal.h"
#include <climits>
#include <string>
#include <sstream>
#include <cstdio>
#include "comparators.h"


IX_Manager::IX_Manager(PF_Manager &pfm) : pfm(pfm){
}

IX_Manager::~IX_Manager(){
}

/////////////////////////////////////////////功能接口/外部接口///////////////////////////////////////////////////

//////////////////////////////创建索引////////////////////////////////////
/*
 给定文件名，索引号，参数的类型和长度来创建一个索引
 */
int IX_Manager::CreateIndex(const char* fileName, int indexNo,
    AttrType attrType, int attrLength) {

    // 创建PF的对象，为了利用这些对象的函数来设置索引所需要的必要信息
    PF_FileHandle fh;
    PF_PageHandle ph_header;
    PF_PageHandle ph_root;
    // 创建的新索引有两个页面：一个是索引文件头页
   //记录了索引对应属性的相关信息，一个是空的根索引页面
    PageNum headerpage;
    PageNum rootpage;
    struct IX_IndexHeader* header;
    struct IX_NodeHeader_L* rootheader;
    struct Node_Entry* entries;
    //计算每个节点的key的数量和计算每个bucket的key的数量，调用两个内部函数
    int numKeys_N = IX_IndexHandle::CalcNumKeysNode(attrLength);
    int numKeys_B = IX_IndexHandle::CalcNumKeysBucket(attrLength);

    if (fileName == NULL || indexNo < 0) // 看下文件名和索引号是否合法
        return (IX_BADFILENAME);
    int rc = 0;
    if (!IsValidIndex(attrType, attrLength)) // 利用isvalidindex来看下属性的长度和类型是否合法
        return (IX_BADINDEXSPEC);

    std::string indexname;
    if ((rc = GetIndexFileName(fileName, indexNo, indexname)))
        return (rc);
    //.c_str是因为在c语言中没有string类型，故必须通过string类对象
    //的成员函数c_str()把string 对象转换成c中的字符串样式。这样是为了和c兼容
    if ((rc = pfm.CreateFile(indexname.c_str()))) {
        return (rc);
    }
    if ((rc = pfm.OpenFile(indexname.c_str(), fh)))
        return (rc);
    if ((rc = fh.AllocatePage(ph_header)) || (rc = ph_header.GetPageNum(headerpage))
        || (rc = fh.AllocatePage(ph_root)) || (rc = ph_root.GetPageNum(rootpage))) {
        return (rc);
    }

    //让header指针指向ph_header指向的这个页面的数据
    //让rootheader指向ph_root指向的页面的数据
    if ((rc = ph_header.GetData((char*&)header)) || (rc = ph_root.GetData((char*&)rootheader))) {
        goto cleanup_and_exit; 
    }

    // 设置索引头的相关信息
    header->attr_type = attrType;
    header->attr_length = attrLength;
    header->maxKeys_N = numKeys_N;
    header->maxKeys_B = numKeys_B;
    header->entryOffset_N = sizeof(struct IX_NodeHeader_I);//node的偏移量
    header->entryOffset_B = sizeof(struct IX_BucketHeader);//bucket的入口地址，因为
    //bucket就是硬盘的读取位置，所以应该放在文件的头部
    header->keysOffset_N = header->entryOffset_N + numKeys_N * sizeof(struct Node_Entry);
    //属性的偏移量
    header->rootPage = rootpage;
    //设置根索引页面的相关参数
    rootheader->isLeafNode = true;
    rootheader->isEmpty = true;
    rootheader->num_keys = 0;
    rootheader->nextPage = NO_MORE_PAGES;//因为是新建立的索引所以下一页和上一页都还没有信息
    rootheader->prevPage = NO_MORE_PAGES;
    rootheader->firstSlotIndex = NO_MORE_SLOTS;
    rootheader->freeSlotIndex = 0;
    entries = (struct Node_Entry*) ((char*)rootheader + header->entryOffset_N);
    //设置
    int i = 0;
    while(i < header->maxKeys_N ) {
        entries[i].isValid = UNOCCUPIED;
        entries[i].page = NO_MORE_PAGES;
        entries[i].slot = NO_MORE_SLOTS;
        if (i == (header->maxKeys_N - 1))
            entries[i].nextSlot = NO_MORE_SLOTS;
        else
            entries[i].nextSlot = i + 1;
        i++;
    }
    //printf("NODE CREATION: entries[0].nextSlot: %d \n", entries[0].nextSlot);

    // Mark both pages as dirty, and close the file
cleanup_and_exit:
    RC rc2;
    if ((rc2 = fh.MarkDirty(headerpage)) || (rc2 = fh.UnpinPage(headerpage)) ||
        (rc2 = fh.MarkDirty(rootpage)) || (rc2 = fh.UnpinPage(rootpage)) || (rc2 = pfm.CloseFile(fh)))
        return (rc2);

    return (rc);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////销毁索引///////////////////////////////
/*
 * This function destroys a valid index given the file name and index number.
 */
RC IX_Manager::DestroyIndex(const char* fileName, int indexNo) {
    int rc;
    if (fileName == NULL || indexNo < 0)
        return (IX_BADFILENAME);
    std::string indexname;
    rc = GetIndexFileName(fileName, indexNo, indexname)
    if(rc!=0)    return (rc);
    rc = pfm.DestroyFile(indexname.c_str())
    if(rc!=0)    return (rc);
    return (0);
}
//////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////打开索引////////////////////////////////////////

int IX_Manager::OpenIndex(const char* fileName, int indexNo,
    IX_IndexHandle& indexHandle) {
    if (fileName == NULL || indexNo < 0) { // check for valid filename, and valid indexHandle
        return (IX_BADFILENAME);
    }
    if (indexHandle.isOpenHandle!=false) {
        return (IX_INVALIDINDEXHANDLE);
    }
    int rc = 0;
    PF_PageHandle ph;
    PageNum firstpage;
    PF_FileHandle fh;
    std::string indexname;
    char* pData;
    if ((rc = GetIndexFileName(fileName, indexNo, indexname)) ||
        (rc = pfm.OpenFile(indexname.c_str(), fh)))//打开这个索引文件
        return (rc);
    if ((rc = fh.GetFirstPage(ph)) || (ph.GetPageNum(firstpage)) || (ph.GetData(pData))) {
        fh.UnpinPage(firstpage);
        pfm.CloseFile(fh);
        return (rc);
    }
    struct IX_IndexHeader* header = (struct IX_IndexHeader*) pData;//将第一页的索引头取出来

    rc = SetUpIH(indexHandle, fh, header);
    int rc2;
    if ((rc2 = fh.UnpinPage(firstpage)))
        return (rc2);

    if (rc != 0) {
        pfm.CloseFile(fh);
    }
    return (rc);
}
/////////////////////////////////////////////////////////////////////////////
///////////////////////////////////关闭索引//////////////////////////////////

RC IX_Manager::CloseIndex(IX_IndexHandle& indexHandle) {
    int rc = 0;
    PF_PageHandle ph;
    PageNum page;
    char* pData;

    if (indexHandle.isOpenHandle == false) { //判断这个handle是否是打开状态，即索引是否
        //打开，没打开就返回报错
        return (IX_INVALIDINDEXHANDLE);
    }

    // 将根节点所在页面markdity，保证能重写回硬盘
    PageNum root = indexHandle.header.rootPage;
    if ((rc = indexHandle.pfh.MarkDirty(root)) || (rc = indexHandle.pfh.UnpinPage(root)))
        return (rc);

    // 看下这个索引的头页面是否存在修改，如果有，也把修改过的头页面进行更新
    if (indexHandle.header_modified == true) {
        if ((rc = indexHandle.pfh.GetFirstPage(ph)) || ph.GetPageNum(page))
            return (rc);
        rc = ph.GetData(pData);
        if (rc!=0) {
            RC rc2;
            if ((rc2 = indexHandle.pfh.UnpinPage(page)))
                return (rc2);
            return (rc);
        }
        memcpy(pData, &indexHandle.header, sizeof(struct IX_IndexHeader));
        if ((rc = indexHandle.pfh.MarkDirty(page)) || (rc = indexHandle.pfh.UnpinPage(page)))
            return (rc);
    }

    // 关闭文件
    if ((rc = pfm.CloseFile(indexHandle.pfh)))
        return (rc);

    if (indexHandle.isOpenHandle == false)
        return (IX_INVALIDINDEXHANDLE);
    indexHandle.isOpenHandle = false;
    return (rc);
}

////////////////////////////////////////////////////////////////////////////////

















/*
 * This function checks that the parameters passed in for attrType and attrLength
 * make it a valid Index. Return true if so.
 */
bool IX_Manager::IsValidIndex(AttrType attrType, int attrLength){
  if(attrType == INT && attrLength == 4)
    return true;
  else if(attrType == FLOAT && attrLength == 4)
    return true;
  else if(attrType == STRING && attrLength > 0 && attrLength <= MAXSTRINGLEN)
    return true;
  else
    return false;
}

/*
 * This function is given a fileName and index number, and returns the name of the
 * index file to be created as a string in indexname. 
 */
RC IX_Manager::GetIndexFileName(const char *fileName, int indexNo, std::string &indexname){

  std::stringstream convert;
  convert << indexNo;
  std::string idx_num = convert.str();
  indexname = std::string(fileName);
  indexname.append(".");
  indexname.append(idx_num);
  if(indexname.size() > PATH_MAX || idx_num.size() > 10) 
    return (IX_BADINDEXNAME);                            
  return (0);
}




RC IX_Manager::SetUpIH(IX_IndexHandle &ih, PF_FileHandle &fh, struct IX_IndexHeader *header){
  int rc = 0;
  memcpy(&ih.header, header, sizeof(struct IX_IndexHeader));
  if(! IsValidIndex(ih.header.attr_type, ih.header.attr_length)) return (IX_INVALIDINDEXFILE);
  if(! ih.isValidIndexHeader()) return (rc);
 

  rc = fh.GetThisPage(header->rootPage, ih.rootPH) 
  if(rc!=0) return (rc);
 /* swith(ih.header.attr_type) {
  case INT:
      ih.comparator = compare_int;
      ih.printer = print_int;
      break;
  case FLOAT:
      ih.comparator = compare_float;
      ih.printer = print_float;
      break;
  default:
      ih.comparator = compare_string;
      ih.printer = print_string;
  }*/




  if(ih.header.attr_type == INT){ 
    ih.comparator = compare_int;
    ih.printer = print_int;
  }
  else if(ih.header.attr_type == FLOAT){
    ih.comparator = compare_float;
    ih.printer = print_float;
  }
  else{
    ih.comparator = compare_string;
    ih.printer = print_string;
  }

  ih.header_modified = false;
  ih.pfh = fh;
  ih.isOpenHandle = true;
  return (rc);
}

RC IX_Manager::CleanUpIH(IX_IndexHandle &indexHandle){
  if(indexHandle.isOpenHandle == false)
    return (IX_INVALIDINDEXHANDLE);
  indexHandle.isOpenHandle = false;
  return (0);
}

