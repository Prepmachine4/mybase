

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
 * ���캯��
 */
RM_FileHandle::RM_FileHandle(){
  // initially, it is not associated with an open file.
  header_modified = false; 
  openedFH = false;
}

/*
 * �⹹����
 */
RM_FileHandle::~RM_FileHandle(){
  openedFH = false; // disassociate from fileHandle from an open file
}
//////////////////////////////////////////////////////////////////////////////////////////
//���ܺ���

//����RID������һ��record��һ������
RC RM_FileHandle::GetRec(const RID& rid, RM_Record& rec) const {
    // ֻ�е������ӵ�һ���򿪵�recordʱ������������ã�������ı�ǲ����ж��Ƿ��
    if (!isValidFH())
        return (RM_INVALIDFILE);

    // ����RID�л�ȡpage��slot
    int rc = 0;
    PageNum page;
    SlotNum slot;
    if ((rc = GetPageNumAndSlot(rid, page, slot)))//�����ȡʧ�ܣ����ش�����Ϣ
        return (rc);

    //liyongPF��������������Ҳ����
    PF_PageHandle ph;
    if ((rc = pfh.GetThisPage(page, ph))) {
        return (rc);
    }
    char* bitmap;
    struct RM_PageHeader* pageheader;
    if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))//�����ȡҳ������ʧ�ܣ������Ϣ�����˳�
        goto cleanup_and_exit;

    // Check if there really exists a record here according to the header
    //����ҳͷ���ж������Ƿ��Ѿ����ڼ�¼
    bool recordExists;
    rc = CheckBitSet(bitmap, header.numRecordsPerPage, slot, recordExists)
       if(rc!=0) goto cleanup_and_exit;
    if (!recordExists) {
        rc = RM_INVALIDRECORD;
        goto cleanup_and_exit;
    }

    // ���ü�¼
    if ((rc = rec.SetRecord(rid, bitmap + (header.bitmapSize) + slot * (header.recordSize),
        header.recordSize)))
        goto cleanup_and_exit;

    //����ǰ��ҳ����Ϊ����ʹ�ñ��ں���pool������ա�������޸Ļ�Ҫ����mark dirty
cleanup_and_exit:
    RC rc2;
    if ((rc2 = pfh.UnpinPage(page)))
        return (rc2);
    return (rc);
}

//�������ļ�¼���ݲ��뵽�ļ��Ŀ���slot�в��ҷ���RID�����ҳ������������������һҳ������һ����ҳ��
int RM_FileHandle::InsertRec(const char* pData, RID& rid) {


    if (!isValidFH())//�ж��Ƿ��ƶ���һ���򿪵��ļ�
        return (RM_INVALIDFILE);

    int rc = 0;

    if (pData == NULL) // �������ָ��ָ��null���������ã�������Ӧ������Ϣ
        return RM_INVALIDRECORD;

    PF_PageHandle ph;
    PageNum page;
    if (header.firstFreePage == NO_FREE_PAGES) {       //û�п���ҳ��ͷ���һ��������ҳ��浽page��
        AllocateNewPage(ph, page);
    }
    else {
        rc = pfh.GetThisPage(header.firstFreePage, ph)//�оͰ���д�뵽��һ������ҳ�棬ҳ������Ϊ��һ������ҳ���ҳ��
        if(rc!=0) return (rc);
        page = header.firstFreePage;
    }

    //�������ҳ���bitmap��ҳͷ
    char* bitmap;
    struct RM_PageHeader* pageheader;
    int slot;
    if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
        goto cleanup_and_exit;
    //���ҳ���ϵĵ�һ�����òۣ�Ȼ����bitmap�ϰ��������Ϊռ��
    if ((rc = GetFirstZeroBit(bitmap, header.numRecordsPerPage, slot)))
        goto cleanup_and_exit;
    if ((rc = SetBit(bitmap, header.numRecordsPerPage, slot)))
        goto cleanup_and_exit;
    //����bitmap�Լ�bitmap offset�����ݲ��õ�ҳ��ĺ���λ�ã�ͬʱ����ҳͷ
    memcpy(bitmap + (header.bitmapSize) + slot * (header.recordSize),
        pData, header.recordSize);
    (pageheader->numRecords)++;
    rid = RID(page, slot); // ����ҳ��Ͳ�����RID

    // if page is full, update the free-page-list in the file header
    //���ҳ���Ѿ����ˣ�������һҳ��ÿһ�ζ�������֮��ͷ����ʲ��ᷢ��ҳ����������ӵĴ���
    if (pageheader->numRecords == header.numRecordsPerPage) {
        header.firstFreePage = pageheader->nextFreePage;
    }

    //����֮ǰȡ��ҳ��
cleanup_and_exit:
    RC rc2;
    if ((rc2 = pfh.MarkDirty(page)) || (rc2 = pfh.UnpinPage(page)))
        return (rc2);


    return (rc);
}


//����RID��ɾ���ļ��е�һ����¼
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
    //������֮ǰ����ͬ
    char* bitmap;
    struct RM_PageHeader* pageheader;
rc = GetPageDataAndBitmap(ph, bitmap, pageheader)
    if(rc!=0)    goto cleanup_and_exit;

    // Ϊ�˱�֤ɾ��������ȷ��ȷʵ��������һ����¼
    bool recordExists;
   rc = CheckBitSet(bitmap, header.numRecordsPerPage, slot, recordExists)
    if(rc!=0)    goto cleanup_and_exit;
    if (!recordExists) {
        rc = RM_INVALIDRECORD;
        goto cleanup_and_exit;
    }

    // ����һ�������¼��λʵ��ɾ������
    if ((rc = ResetBit(bitmap, header.numRecordsPerPage, slot)))
        goto cleanup_and_exit;
    pageheader->numRecords--;
    //����ҳ��ļ�¼����
    if (pageheader->numRecords == header.numRecordsPerPage - 1) {
        pageheader->nextFreePage = header.firstFreePage;
        header.firstFreePage = page;
    }


cleanup_and_exit:
    RC rc2;
    if ((rc2 = pfh.MarkDirty(page)) || (rc2 = pfh.UnpinPage(page)))//��Ϊ�漰�����ļ�¼��Ҫ������Ӳ�̣���Ҫmark
        return (rc2);

    return (rc);
}

//���¼�¼����
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

    // ������֮ǰͬ������һЩ׼������������ʱ��������
    char* recData;
    if ((rc = rec.GetData(recData)))
        goto cleanup_and_exit;
    memcpy(bitmap + (header.bitmapSize) + slot * (header.recordSize),
        recData, header.recordSize);

    // ����ǰunpinҳ��
cleanup_and_exit:
    RC rc2;
    if ((rc2 = pfh.MarkDirty(page)) || (rc2 = pfh.UnpinPage(page)))//��Ϊ�漰�����ļ�¼��Ҫ������Ӳ�̣���Ҫmark
        return (rc2);
    return (rc);
}


//����Ӳ���ϵ���Ϣ
RC RM_FileHandle::ForcePages(PageNum pageNum) {

    if (!isValidFH())
        return (RM_INVALIDFILE);
    pfh.ForcePages(pageNum);
    return (0);
}



/*///////////////////////////////////////////////////////////////////////////
 * �Ѷ����Ӳ��copy���ڴ����
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


//����PF������һ����ҳ�滹������ҳ�룬��������ҳͷ��������linked list��֤������ӽ�ȥ
RC RM_FileHandle::AllocateNewPage(PF_PageHandle &ph, PageNum &page){
  RC rc;
  //����һ��ҳ�棬�����Ϊ0��˵���д����������Ϣ
  if((rc = pfh.AllocatePage(ph))){
    return (rc);
  }
  //��ȡҳ�룬���������0�����ش�����Ϣ
  if((rc = ph.GetPageNum(page)))
    return (rc);
 
  // ����ҳͷ�ļ�
  char *bitmap;
  struct RM_PageHeader *pageheader;
  if((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))//GetPageDataAndBitmap�����ж��Ƿ񴴽��ɹ�
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
  // ����ҳͷָ��
  char * pData;
  rc = ph.GetData(pData)
  if(rc!=0) return (rc);
  pageheader = (struct RM_PageHeader *) pData;
  bitm = pData + header.bitmapOffset; // ����bitmapָ��
  return (0);
}


//����RID���󣬻�ȡ��Ϣ
RC RM_FileHandle::GetPageNumAndSlot(const RID &rid, PageNum &page, SlotNum &slot) const {
  int rc;
  rc = rid.isValidRID()//�ж��Ƿ����
    if(rc!=0)return (rc);  
  rc = rid.GetPageNum(page)//��ȡҳ��
      if (rc != 0)return (rc);
  rc = rid.GetSlotNum(slot)//��ȡslot
      if (rc != 0)return (rc);
  return (0);
}


//����һ��ҳ���slot����Ѱ�������¼֮�����һ����¼��nextpage��˵����һ����¼���ڵ�ǰҳ������һҳ
RC RM_FileHandle::GetNextRecord(PageNum page, SlotNum slot, RM_Record &rec, PF_PageHandle &ph, bool nextPage){
  RC rc = 0;
  char *bitmap;
  struct RM_PageHeader *pageheader;
  int nextRec;
  PageNum nextRecPage = page;
  SlotNum nextRecSlot;

  //Ѱ�ҵ�һ�����ڼ�¼��ҳ��
  if(nextPage){
    while(true){
      //printf("Getting next page\n");
      if((PF_EOF == pfh.GetNextPage(nextRecPage, ph)))
        return (RM_EOF); //ȥ��һ��ҳ�棬��������ļ�άĩβ�����ش�����Ϣ

      // ����ҳ���bitmap��Ϣ
      if((rc = ph.GetPageNum(nextRecPage)))
        return (rc);
      if((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
        return (rc);
      // Ѱ����һ����¼
      if(GetNextOneBit(bitmap, header.numRecordsPerPage, 0, nextRec) != RM_ENDOFPAGE)
        break;
      // ������ҳ���ϲ����ڼ�¼��unpin���ҳ��Ȼ��Ѱ����һ��
      if((rc = pfh.UnpinPage(nextRecPage)))
        return (rc);
    }
  }
  else{
    // �õ����ҳ���bitmap���������ҵ���һ��record��λ��
    if((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
      return (rc);
    if(GetNextOneBit(bitmap, header.numRecordsPerPage, slot + 1, nextRec) == RM_ENDOFPAGE)
      return (RM_EOF);
  }
  // �ҵ������¼��RID�Լ��������ݲ���
  nextRecSlot = nextRec;
  RID rid(nextRecPage, nextRecSlot);
  if((rc = rec.SetRecord(rid, bitmap + (header.bitmapSize) + (nextRecSlot)*(header.recordSize), 
    header.recordSize)))
    return (rc);

  return (0);
}

//�ж��ļ���״̬
bool RM_FileHandle::isValidFH() const{
  if(openedFH == true)
    return true;
  return false;
}

/*
 *�ж��ļ�ͷ�Ƿ���ʱ�Ϸ��Ŀ��õ�
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
 * ��ȡ�ļ��м�¼�ĳ���
 */
int RM_FileHandle::getRecordSize(){
  return this->header.recordSize;
}


//��bitmap����Ϊȫ0
RC RM_FileHandle::ResetBitmap(char *bitmap, int size){
  int char_num = NumBitsToCharSize(size);
  for(int i=0; i < char_num; i++)
    bitmap[i] = bitmap[i] ^ bitmap[i];
  return (0);
}

//���ض�λ����bitmap����Ϊ1
RC RM_FileHandle::SetBit(char *bitmap, int size, int bitnum){
  if (bitnum > size)//�������õ��Ƿ񳬳���Χ
    return (RM_INVALIDBITOPERATION);
  int chunk = bitnum /8;
  int offset = bitnum - chunk*8;
  bitmap[chunk] |= 1 << offset;
  return (0);
}

//ͬ�ϣ�����Ϊ0
RC RM_FileHandle::ResetBit(char *bitmap, int size, int bitnum){
if (bitnum > size)//�ж�slot�Ƿ�������¼����֮��
    return (RM_INVALIDBITOPERATION);
  int mainADD = bitnum / 8;
  int offset = bitnum - mainADD*8;
  bitmap[mainADD] &= ~(1 << offset);
  return (0);
}

//����ض�λ����bitmap�Ƿ�Ϊ1��0
RC RM_FileHandle::CheckBitSet(char *bitmap, int size, int bitnum, bool &set) const{
  if(bitnum > size)
    return (RM_INVALIDBITOPERATION);
  int chunk = bitnum / 8;
  int offset = bitnum - chunk*8;
  //���ǽ�bitmap�����char���ͣ�����ÿһ����¼��bitmap�е�ָʾ�ǰ���
  //bit��ָʾ�ģ���������ͨ����λ����������λ���㱣��ָʾ���record��λ���ж����λ�Ƿ�
  //Ϊ0�����Ϊ0˵�����record��Ч�������=0˵����Ч
  if ((bitmap[chunk] & (1 << offset)) != 0)
    set = true;//��=0Ϊ��Ч
  else
    set = false;//����0��Ч
  return (0);
}

//Ѱ��bitmap��һ��Ϊ0��λ�ã���û�б���Ӧ����
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

//ͬ�ϣ������ҵ�����Ϊ0֮���һ��Ϊ1��
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

//����洢bitmap�����char������
int RM_FileHandle::NumBitsToCharSize(int size){
  int bitmapSize = size/8;
  if(bitmapSize * 8 < size) bitmapSize++;
  return bitmapSize;
}


//����һ��ҳ�������ɵļ�¼������
int RM_FileHandle::CalcNumRecPerPage(int recSize){
  return floor((PF_PAGE_SIZE * 1.0) / (1.0 * recSize + 1.0/8));
}
