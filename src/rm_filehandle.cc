

#include <unistd.h>
#include <sys/types.h>
#include "pf.h"

#include <math.h>
#include <cstdio>

// Define the RM page header
struct RM_PageHeader {
    PageNum nextFreePage;
    int numRecords;
};

/*
 * 构造函数
 */
RM_FileHandle::RM_FileHandle(){
  // initially, it is not associated with an open file.
  header_modified = false; 
  openedFH = false;
}

/*
 * 解构函数
 */
RM_FileHandle::~RM_FileHandle(){
  openedFH = false; // disassociate from fileHandle from an open file
}
//////////////////////////////////////////////////////////////////////////////////////////
//功能函数

//给定RID，返回一个record的一个副本
RC RM_FileHandle::GetRec(const RID& rid, RM_Record& rec) const {
    // 只有当其连接到一个打开的record时，这个语句才有用，用下面的标记参量判断是否打开
    if (!isValidFH())
        return (RM_INVALIDFILE);

    // 根据RID中获取page和slot
    int rc = 0;
    PageNum page;
    SlotNum slot;
    if ((rc = GetPageNumAndSlot(rid, page, slot)))//如果获取失败，返回错误信息
        return (rc);

    //liyongPF检索符合条件的也买你
    PF_PageHandle ph;
    if ((rc = pfh.GetThisPage(page, ph))) {
        return (rc);
    }
    char* bitmap;
    struct RM_PageHeader* pageheader;
    if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))//如果获取页面内容失败，清楚信息并且退出
        goto cleanup_and_exit;

    // Check if there really exists a record here according to the header
    //根据页头来判断这里是否已经存在记录
    bool recordExists;
    rc = CheckBitSet(bitmap, header.numRecordsPerPage, slot, recordExists)
       if(rc!=0) goto cleanup_and_exit;
    if (!recordExists) {
        rc = RM_INVALIDRECORD;
        goto cleanup_and_exit;
    }

    // 设置记录
    if ((rc = rec.SetRecord(rid, bitmap + (header.bitmapSize) + slot * (header.recordSize),
        header.recordSize)))
        goto cleanup_and_exit;

    //返回前将页面标记为不再使用便于后期pool对其回收。如果有修改还要额外mark dirty
cleanup_and_exit:
    RC rc2;
    if ((rc2 = pfh.UnpinPage(page)))
        return (rc2);
    return (rc);
}

//将给定的记录数据插入到文件的可用slot中并且返回RID，如果页面已满或不足以容纳这一页，创建一个新页面
int RM_FileHandle::InsertRec(const char* pData, RID& rid) {


    if (!isValidFH())//判断是否制定了一个打开的文件
        return (RM_INVALIDFILE);

    int rc = 0;

    if (pData == NULL) // 如果数据指针指向null，即不可用，返回相应错误信息
        return RM_INVALIDRECORD;

    PF_PageHandle ph;
    PageNum page;
    if (header.firstFreePage == NO_FREE_PAGES) {       //没有可用页面就分配一个，并把页码存到page中
        AllocateNewPage(ph, page);
    }
    else {
        rc = pfh.GetThisPage(header.firstFreePage, ph)//有就把他写入到第一个可用页面，页码设置为第一个可用页面的页码
        if(rc!=0) return (rc);
        page = header.firstFreePage;
    }

    //检索这个页面的bitmap和页头
    char* bitmap;
    struct RM_PageHeader* pageheader;
    int slot;
    if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
        goto cleanup_and_exit;
    //获得页面上的第一个可用槽，然后在bitmap上把这个槽设为占用
    if ((rc = GetFirstZeroBit(bitmap, header.numRecordsPerPage, slot)))
        goto cleanup_and_exit;
    if ((rc = SetBit(bitmap, header.numRecordsPerPage, slot)))
        goto cleanup_and_exit;
    //根据bitmap以及bitmap offset把数据布置到页面的合适位置，同时更新页头
    memcpy(bitmap + (header.bitmapSize) + slot * (header.recordSize),
        pData, header.recordSize);
    (pageheader->numRecords)++;
    rid = RID(page, slot); // 根据页码和槽设置RID

    // if page is full, update the free-page-list in the file header
    //如果页面已经满了，翻到下一页。每一次都是满了之后就翻，故不会发生页面满了再添加的错误。
    if (pageheader->numRecords == header.numRecordsPerPage) {
        header.firstFreePage = pageheader->nextFreePage;
    }

    //返回之前取消页面
cleanup_and_exit:
    RC rc2;
    if ((rc2 = pfh.MarkDirty(page)) || (rc2 = pfh.UnpinPage(page)))
        return (rc2);


    return (rc);
}


//给定RID，删除文件中的一条记录
int RM_FileHandle::DeleteRec(const RID& rid) {

    if (!isValidFH())
        return (RM_INVALIDFILE);
    int rc = 0;

    PageNum page;
    SlotNum slot;
    rc = GetPageNumAndSlot(rid, page, slot)
       if(rc!=0) return (rc);
    PF_PageHandle ph;
   rc = pfh.GetThisPage(page, ph)
    if(rc!=0)    return (rc);
    //以上与之前的相同
    char* bitmap;
    struct RM_PageHeader* pageheader;
rc = GetPageDataAndBitmap(ph, bitmap, pageheader)
    if(rc!=0)    goto cleanup_and_exit;

    // 为了保证删除不出错，确保确实存在这样一条记录
    bool recordExists;
   rc = CheckBitSet(bitmap, header.numRecordsPerPage, slot, recordExists)
    if(rc!=0)    goto cleanup_and_exit;
    if (!recordExists) {
        rc = RM_INVALIDRECORD;
        goto cleanup_and_exit;
    }

    // 重置一个这个记录的位实现删除操作
    if ((rc = ResetBit(bitmap, header.numRecordsPerPage, slot)))
        goto cleanup_and_exit;
    pageheader->numRecords--;
    //更新页面的记录数量
    if (pageheader->numRecords == header.numRecordsPerPage - 1) {
        pageheader->nextFreePage = header.firstFreePage;
        header.firstFreePage = page;
    }


cleanup_and_exit:
    RC rc2;
    if ((rc2 = pfh.MarkDirty(page)) || (rc2 = pfh.UnpinPage(page)))//因为涉及到更改记录需要反馈到硬盘，需要mark
        return (rc2);

    return (rc);
}

//更新记录内容
RC RM_FileHandle::UpdateRec(const RM_Record& rec) {

    if (!isValidFH())
        return (RM_INVALIDFILE);
    RC rc = 0;


    RID rid;
    if ((rc = rec.GetRid(rid)))
        return (rc);
    PageNum page;
    SlotNum slot;
    if ((rc = GetPageNumAndSlot(rid, page, slot)))
        return (rc);


    PF_PageHandle ph;
    if ((rc = pfh.GetThisPage(page, ph)))
        return (rc);
    char* bitmap;
    struct RM_PageHeader* pageheader;
    if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
        goto cleanup_and_exit;


    bool recordExists;
    if ((rc = (bitmap, header.numRecordsPerPage, slot, recordExists)))
        goto cleanup_and_exit;
    if (!recordExists) {
        rc = RM_INVALIDRECORD;
        goto cleanup_and_exit;
    }

    // 上述与之前同，都是一些准备工作，下面时更新内容
    char* recData;
    if ((rc = rec.GetData(recData)))
        goto cleanup_and_exit;
    memcpy(bitmap + (header.bitmapSize) + slot * (header.recordSize),
        recData, header.recordSize);

    // 返回前unpin页面
cleanup_and_exit:
    RC rc2;
    if ((rc2 = pfh.MarkDirty(page)) || (rc2 = pfh.UnpinPage(page)))//因为涉及到更改记录需要反馈到硬盘，需要mark
        return (rc2);
    return (rc);
}


//更新硬盘上的信息
RC RM_FileHandle::ForcePages(PageNum pageNum) {

    if (!isValidFH())
        return (RM_INVALIDFILE);
    pfh.ForcePages(pageNum);
    return (0);
}



/*///////////////////////////////////////////////////////////////////////////
 * 把对象从硬盘copy到内存池中
 */
RM_FileHandle& RM_FileHandle::operator= (const RM_FileHandle &fileHandle){
  // sets all contents equal to another RM_FileHandle object
  if (this != &fileHandle){
    this->openedFH = fileHandle.openedFH;
    this->header_modified = fileHandle.header_modified;
    this->pfh = fileHandle.pfh;
    memcpy(&this->header, &fileHandle.header, sizeof(struct RM_FileHeader));
  }
  return (*this);
}


//利用PF来分配一个新页面还有它的页码，并且设置页头，最后更新linked list保证它被添加进去
RC RM_FileHandle::AllocateNewPage(PF_PageHandle &ph, PageNum &page){
  RC rc;
  //分配一个页面，如果不为0，说明有错，输出报错信息
  if((rc = pfh.AllocatePage(ph))){
    return (rc);
  }
  //获取页码，如果不返回0，返回错误信息
  if((rc = ph.GetPageNum(page)))
    return (rc);
 
  // 创建页头文件
  char *bitmap;
  struct RM_PageHeader *pageheader;
  if((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))//GetPageDataAndBitmap方法判断是否创建成功
    return (rc);
  pageheader->nextFreePage = header.firstFreePage;
  pageheader->numRecords = 0;
  if((rc = ResetBitmap(bitmap, header.numRecordsPerPage)))
    return (rc);

  header.numPages= header.numPages+1; 
  header.firstFreePage = page;
  return (0);
}

RC RM_FileHandle::GetPageDataAndBitmap(PF_PageHandle &ph, char *&bitm, struct RM_PageHeader *&pageheader) const{
  int rc;
  // 检索页头指针
  char * pData;
  rc = ph.GetData(pData)
  if(rc!=0) return (rc);
  pageheader = (struct RM_PageHeader *) pData;
  bitm = pData + header.bitmapOffset; // 检索bitmap指针
  return (0);
}


//给定RID对象，获取信息
RC RM_FileHandle::GetPageNumAndSlot(const RID &rid, PageNum &page, SlotNum &slot) const {
  int rc;
  rc = rid.isValidRID()//判断是否可用
    if(rc!=0)return (rc);  
  rc = rid.GetPageNum(page)//获取页码
      if (rc != 0)return (rc);
  rc = rid.GetSlotNum(slot)//获取slot
      if (rc != 0)return (rc);
  return (0);
}


//给定一个页码和slot码来寻找这个记录之后的下一条记录，nextpage来说明下一条记录是在当前页还是下一页
RC RM_FileHandle::GetNextRecord(PageNum page, SlotNum slot, RM_Record &rec, PF_PageHandle &ph, bool nextPage){
  RC rc = 0;
  char *bitmap;
  struct RM_PageHeader *pageheader;
  int nextRec;
  PageNum nextRecPage = page;
  SlotNum nextRecSlot;

  //寻找第一个存在记录的页面
  if(nextPage){
    while(true){
      //printf("Getting next page\n");
      if((PF_EOF == pfh.GetNextPage(nextRecPage, ph)))
        return (RM_EOF); //去下一个页面，如果到达文件维末尾，返回错误信息

      // 检索页面和bitmap信息
      if((rc = ph.GetPageNum(nextRecPage)))
        return (rc);
      if((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
        return (rc);
      // 寻找下一条记录
      if(GetNextOneBit(bitmap, header.numRecordsPerPage, 0, nextRec) != RM_ENDOFPAGE)
        break;
      // 如果这个页面上不存在记录，unpin这个页面然后寻找下一个
      if((rc = pfh.UnpinPage(nextRecPage)))
        return (rc);
    }
  }
  else{
    // 得到这个页面的bitmap，并依次找到下一个record的位置
    if((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
      return (rc);
    if(GetNextOneBit(bitmap, header.numRecordsPerPage, slot + 1, nextRec) == RM_ENDOFPAGE)
      return (RM_EOF);
  }
  // 找到这个记录的RID以及它的数据部分
  nextRecSlot = nextRec;
  RID rid(nextRecPage, nextRecSlot);
  if((rc = rec.SetRecord(rid, bitmap + (header.bitmapSize) + (nextRecSlot)*(header.recordSize), 
    header.recordSize)))
    return (rc);

  return (0);
}

//判断文件打开状态
bool RM_FileHandle::isValidFH() const{
  if(openedFH == true)
    return true;
  return false;
}

/*
 *判断文件头是否是时合法的可用的
 */
bool RM_FileHandle::isValidFileHeader() const{
  if(!isValidFH()){
    return false;
  }
  if(header.recordSize <= 0 || header.numRecordsPerPage <= 0 || header.numPages <= 0){
    return false;
  }
  if((header.bitmapOffset + header.bitmapSize + header.recordSize*header.numRecordsPerPage) >
    PF_PAGE_SIZE){
    return false;
  }
  return true;
}

/*
 * 获取文件中记录的长度
 */
int RM_FileHandle::getRecordSize(){
  return this->header.recordSize;
}


//将bitmap重置为全0
RC RM_FileHandle::ResetBitmap(char *bitmap, int size){
  int char_num = NumBitsToCharSize(size);
  for(int i=0; i < char_num; i++)
    bitmap[i] = bitmap[i] ^ bitmap[i];
  return (0);
}

//将特定位数的bitmap设置为1
RC RM_FileHandle::SetBit(char *bitmap, int size, int bitnum){
  if (bitnum > size)//检验设置的是否超出范围
    return (RM_INVALIDBITOPERATION);
  int chunk = bitnum /8;
  int offset = bitnum - chunk*8;
  bitmap[chunk] |= 1 << offset;
  return (0);
}

//同上，设置为0
RC RM_FileHandle::ResetBit(char *bitmap, int size, int bitnum){
if (bitnum > size)//判断slot是否在最大记录数量之内
    return (RM_INVALIDBITOPERATION);
  int mainADD = bitnum / 8;
  int offset = bitnum - mainADD*8;
  bitmap[mainADD] &= ~(1 << offset);
  return (0);
}

//检查特定位数的bitmap是否为1或0
RC RM_FileHandle::CheckBitSet(char *bitmap, int size, int bitnum, bool &set) const{
  if(bitnum > size)
    return (RM_INVALIDBITOPERATION);
  int chunk = bitnum / 8;
  int offset = bitnum - chunk*8;
  //我们将bitmap搞成了char类型，但是每一个记录在bitmap中的指示是按照
  //bit来指示的，所以我们通过按位与来将其他位清零保留指示这个record的位来判断这个位是否
  //为0，如果为0说明这个record无效，如果！=0说明有效
  if ((bitmap[chunk] & (1 << offset)) != 0)
    set = true;//！=0为有效
  else
    set = false;//等于0无效
  return (0);
}

//寻找bitmap第一个为0的位置，若没有报相应错误
RC RM_FileHandle::GetFirstZeroBit(char *bitmap, int size, int &location){
    i=0;
 while(i<size){
    int chunk = i /8;
    int offset = i - chunk*8;
    if ((bitmap[chunk] & (1 << offset)) == 0){
      location = i;
      return (0);
    }
    i++;
  }
  return RM_PAGEFULL;
}

//同上，不过找的是在为0之后第一个为1的
RC RM_FileHandle::GetNextOneBit(char *bitmap, int size, int start, int &location){
  for(int i = start; i < size; i++){
    int chunk = i /8;
    int offset = i - chunk*8;
    if (((bitmap[chunk] & (1 << offset)) != 0)){
      location = i;
      return (0);
    }
  }
  return RM_ENDOFPAGE;
}

//计算存储bitmap所需的char的数量
int RM_FileHandle::NumBitsToCharSize(int size){
  int bitmapSize = size/8;
  if(bitmapSize * 8 < size) bitmapSize++;
  return bitmapSize;
}


//计算一个页面能容纳的记录的数量
int RM_FileHandle::CalcNumRecPerPage(int recSize){
  return floor((PF_PAGE_SIZE * 1.0) / (1.0 * recSize + 1.0/8));
}
