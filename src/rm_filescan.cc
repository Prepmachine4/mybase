

#include <unistd.h>
#include <sys/types.h>
#include "pf.h"
#include "rm_internal.h"
#include <stdlib.h>

//���캯��
RM_FileScan::RM_FileScan(){
  openScan = false; 
  value = NULL;
  initializedValue = false;
  hasPagePinned = false;
  scanEnded = true;
}
//��������
RM_FileScan::~RM_FileScan(){
  if(scanEnded == false && hasPagePinned == true && openScan == true){
    fileHandle->pfh.UnpinPage(scanPage);
  }
  if (initializedValue == true){ //�ͷ�value
    free(value);
    initializedValue = false;
  }
}

//��������Ƚ���Ⱥ�������ȷ���1�������������ֵ�����Գ��ȣ���Ϊ�������ܱȽ�
bool equal(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 == *(float*)value2);
    case INT: return (*(int *)value1 == *(int *)value2) ;
    default:
      return (strncmp((char *) value1, (char *) value2, attrLength) == 0); 
  }
}
//����1С�ڶ���2������1
bool less_than(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 < *(float*)value2);
    case INT: return (*(int *)value1 < *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) < 0);
  }
}
//���ڷ���1
bool greater_than(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 > *(float*)value2);
    case INT: return (*(int *)value1 > *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) > 0);
  }
}
//С�ڵ��ڷ���1
bool less_than_or_eq_to(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 <= *(float*)value2);
    case INT: return (*(int *)value1 <= *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) <= 0);
  }
}
//���ڵ��ڷ���1
bool greater_than_or_eq_to(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 >= *(float*)value2);
    case INT: return (*(int *)value1 >= *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) >= 0);
  }
}
//���ȷ���1
bool not_equal(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 != *(float*)value2);
    case INT: return (*(int *)value1 != *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) != 0);
  }
}


//����OpenScan�����Ĳ�������Filehandle��������
RC RM_FileScan::OpenScan (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint) {
  
 //���filehandle�Ѿ���ϵһ��scan�ˣ������ڽ�����������ô����󷵻�
  if (openScan == true)
    return (RM_INVALIDSCAN);

  //���filehandle�Ƿ���ã�ͨ��ͷ�ļ�
  if(fileHandle.isValidFileHeader())
    this->fileHandle = const_cast<RM_FileHandle*>(&fileHandle);
  else
    return (RM_INVALIDFILE);

  this->value = NULL;
  //ͨ�����ʵĺ����������Ӧ�ıȽϹ���
  this->compOp = compOp;
  switch(compOp){
    case EQ_OP : comparator = &equal; break;
    case LT_OP : comparator = &less_than; break;
    case GT_OP : comparator = &greater_than; break;
    case LE_OP : comparator = &less_than_or_eq_to; break;
    case GE_OP : comparator = &greater_than_or_eq_to; break;
    case NE_OP : comparator = &not_equal; break;
    case NO_OP : comparator = NULL; break;
    default: return (RM_INVALIDSCAN);
  }

  int recSize = (this->fileHandle)->getRecordSize();//����filehandle�ķ�����ȡ��¼����
  // ������ڱȽϼ�compOP������NO_OP������һЩ����
  if(this->compOp != NO_OP){
   //������������ƫ�ƺͳ����Ƿ�Ϸ�
    if((attrOffset + attrLength) > recSize || attrOffset < 0 || attrOffset > MAXSTRINGLEN)
      return (RM_INVALIDSCAN);
    this->attrOffset = attrOffset;
    this->attrLength = attrLength;

    //�����ʵ����ڴ����洢���Ƚϵ�ֵ
    if(attrType == FLOAT || attrType == INT){
      if(attrLength != 4)
        return (RM_INVALIDSCAN);
      this->value = (void *) malloc(4);
      memcpy(this->value, value, 4);
      initializedValue = true;
    }
    else if(attrType == STRING){
      this->value = (void *) malloc(attrLength);
      memcpy(this->value, value, attrLength);
      initializedValue = true;
    }
    else{
      return (RM_INVALIDSCAN);
    }
    this->attrType = attrType;
  }

  // ������
  openScan = true;
  scanEnded = false;

  // ������������
  numRecOnPage = 0;
  numSeenOnPage = 0;
  useNextPage = true;
  scanPage = 0;
  scanSlot = BEGIN_SCAN;
  numSeenOnPage = 0;
  hasPagePinned = false;
  return (0);
} 


//��������ҳ��ļ�¼�����������������numRecods��
RC RM_FileScan::GetNumRecOnPage(PF_PageHandle &ph, int &numRecords){
  RC rc;
  char *bitmap;
  struct RM_PageHeader *pageheader;
  if((rc = (this->fileHandle)->GetPageDataAndBitmap(ph, bitmap, pageheader)))
      return (rc);
  numRecords = pageheader->numRecords;
  return (0);
}


//�ҵ���һ��������������ļ�¼
RC RM_FileScan::GetNextRec(RM_Record &rec) {
 //��������������߲������ˡ����̽���
  if(scanEnded == true)
    return (RM_EOF);
  if(openScan == false)
    return (RM_INVALIDSCAN);
  hasPagePinned = true;
  
  RC rc;
  while(true){
    // �������м�¼
    RM_Record temprec;
    if((rc=fileHandle->GetNextRecord(scanPage, scanSlot, temprec, currentPH, useNextPage))){
      if(rc == RM_EOF){
        hasPagePinned = false;
        scanEnded = true;
      }
      return (rc);
    }
    hasPagePinned = true;
   
    //��������ļ�¼����һҳ�棬����numRecOnrecord�����������һ��ҳ��ļ�¼������Ϊ0������currentPH
    if(useNextPage){
      GetNumRecOnPage(currentPH, numRecOnPage);
      useNextPage = false;
      numSeenOnPage = 0;
      if(numRecOnPage == 1)
        currentPH.GetPageNum(scanPage);
    }
    numSeenOnPage++; 

 
    //ҳ���������ǣ�����unpin
    if(numRecOnPage == numSeenOnPage){
      useNextPage = true;
      
      if(rc = fileHandle->pfh.UnpinPage(scanPage)){
        return (rc);
      }
      hasPagePinned = false;
    }
   
    //����ɾ��ɨ���Ը���ɨ��Ľ���
    RID rid;
    temprec.GetRid(rid);
    rid.GetPageNum(scanPage);
    rid.GetSlotNum(scanSlot);

   //���Ƿ����������Ƚ�������������㣬���˳����������ؼ�¼��
    char *pData;
    if((rc = temprec.GetData(pData))){
      return (rc);
    }
    if(compOp != NO_OP){
      bool satisfies = (* comparator)(pData + attrOffset, this->value, attrType, attrLength);
      if(satisfies){
        rec = temprec;
        break;
      }
    }
    else{
      rec = temprec; // ��������㣬���������¼
      break;
    }
  }
  return (0);
}



//�ر��������ͷŴ�űȽ�ֵ�ڴ棬���Ұ�����pin����ҳ��ȫ��unpin
RC RM_FileScan::CloseScan () {
  RC rc;
  if(openScan == false){
    return (RM_INVALIDSCAN);
  }
  if(hasPagePinned == true){
  
    if((rc = fileHandle->pfh.UnpinPage(scanPage)))
      return (rc);
  }
  if(initializedValue == true){
    free(this->value);
    initializedValue = false;
  }
  openScan = false;
  return (0);
}