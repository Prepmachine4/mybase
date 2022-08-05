

#include <unistd.h>
#include <sys/types.h>
#include "pf.h"
#include "rm_internal.h"
#include <stdlib.h>

//构造函数
RM_FileScan::RM_FileScan(){
  openScan = false; 
  value = NULL;
  initializedValue = false;
  hasPagePinned = false;
  scanEnded = true;
}
//析构函数
RM_FileScan::~RM_FileScan(){
  if(scanEnded == false && hasPagePinned == true && openScan == true){
    fileHandle->pfh.UnpinPage(scanPage);
  }
  if (initializedValue == true){ //释放value
    free(value);
    initializedValue = false;
  }
}

//两个对象比较相等函数，相等返回1。必须接受属性值和属性长度，因为这样才能比较
bool equal(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 == *(float*)value2);
    case INT: return (*(int *)value1 == *(int *)value2) ;
    default:
      return (strncmp((char *) value1, (char *) value2, attrLength) == 0); 
  }
}
//对象1小于对象2，返回1
bool less_than(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 < *(float*)value2);
    case INT: return (*(int *)value1 < *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) < 0);
  }
}
//大于返回1
bool greater_than(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 > *(float*)value2);
    case INT: return (*(int *)value1 > *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) > 0);
  }
}
//小于等于返回1
bool less_than_or_eq_to(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 <= *(float*)value2);
    case INT: return (*(int *)value1 <= *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) <= 0);
  }
}
//大于等于返回1
bool greater_than_or_eq_to(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 >= *(float*)value2);
    case INT: return (*(int *)value1 >= *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) >= 0);
  }
}
//不等返回1
bool not_equal(void * value1, void * value2, AttrType attrtype, int attrLength){
  switch(attrtype){
    case FLOAT: return (*(float *)value1 != *(float*)value2);
    case INT: return (*(int *)value1 != *(int *)value2) ;
    default: 
      return (strncmp((char *) value1, (char *) value2, attrLength) != 0);
  }
}


//设置OpenScan函数的参数，根Filehandle是朋友类
RC RM_FileScan::OpenScan (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint) {
  
 //如果filehandle已经联系一个scan了，即正在进行搜索，那么报错后返回
  if (openScan == true)
    return (RM_INVALIDSCAN);

  //检查filehandle是否可用，通过头文件
  if(fileHandle.isValidFileHeader())
    this->fileHandle = const_cast<RM_FileHandle*>(&fileHandle);
  else
    return (RM_INVALIDFILE);

  this->value = NULL;
  //通过合适的函数进行相对应的比较功能
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

  int recSize = (this->fileHandle)->getRecordSize();//调用filehandle的方法获取记录长度
  // 如果存在比较即compOP不等于NO_OP，更新一些参数
  if(this->compOp != NO_OP){
   //检查给定的属性偏移和长度是否合法
    if((attrOffset + attrLength) > recSize || attrOffset < 0 || attrOffset > MAXSTRINGLEN)
      return (RM_INVALIDSCAN);
    this->attrOffset = attrOffset;
    this->attrLength = attrLength;

    //分配适当的内存来存储被比较的值
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

  // 打开搜索
  openScan = true;
  scanEnded = false;

  // 设置搜索参数
  numRecOnPage = 0;
  numSeenOnPage = 0;
  useNextPage = true;
  scanPage = 0;
  scanSlot = BEGIN_SCAN;
  numSeenOnPage = 0;
  hasPagePinned = false;
  return (0);
} 


//检索给定页面的记录数量，并把它存放在numRecods中
RC RM_FileScan::GetNumRecOnPage(PF_PageHandle &ph, int &numRecords){
  RC rc;
  char *bitmap;
  struct RM_PageHeader *pageheader;
  if((rc = (this->fileHandle)->GetPageDataAndBitmap(ph, bitmap, pageheader)))
      return (rc);
  numRecords = pageheader->numRecords;
  return (0);
}


//找到下一个满足检索条件的记录
RC RM_FileScan::GetNextRec(RM_Record &rec) {
 //如果检索结束或者不可用了。立刻结束
  if(scanEnded == true)
    return (RM_EOF);
  if(openScan == false)
    return (RM_INVALIDSCAN);
  hasPagePinned = true;
  
  RC rc;
  while(true){
    // 遍历所有记录
    RM_Record temprec;
    if((rc=fileHandle->GetNextRecord(scanPage, scanSlot, temprec, currentPH, useNextPage))){
      if(rc == RM_EOF){
        hasPagePinned = false;
        scanEnded = true;
      }
      return (rc);
    }
    hasPagePinned = true;
   
    //如果检索的记录在下一页面，重置numRecOnrecord。并且如果下一个页面的记录数量不为0，更新currentPH
    if(useNextPage){
      GetNumRecOnPage(currentPH, numRecOnPage);
      useNextPage = false;
      numSeenOnPage = 0;
      if(numRecOnPage == 1)
        currentPH.GetPageNum(scanPage);
    }
    numSeenOnPage++; 

 
    //页面检索完后们，进行unpin
    if(numRecOnPage == numSeenOnPage){
      useNextPage = true;
      
      if(rc = fileHandle->pfh.UnpinPage(scanPage)){
        return (rc);
      }
      hasPagePinned = false;
    }
   
    //检索删除扫描以更新扫描的进度
    RID rid;
    temprec.GetRid(rid);
    rid.GetPageNum(scanPage);
    rid.GetSlotNum(scanSlot);

   //查是否满足搜索比较条件，如果满足，则退出函数，返回记录。
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
      rec = temprec; // 如果不满足，返回这个记录
      break;
    }
  }
  return (0);
}



//关闭搜索后，释放存放比较值内存，并且把我们pin过的页面全部unpin
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