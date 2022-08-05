

#include <unistd.h>
#include <sys/types.h>
#include "pf.h"
#include "rm_internal.h"

//构造函数
RM_Record::RM_Record(){
  size = INVALID_RECORD_SIZE; // 初始化记录的内容
  data = NULL;
}
//析构函数
RM_Record::~RM_Record(){
  if(data != NULL) // 删除记录的所有数据
    delete [] data;
}

//将记录从硬盘搞到内存里面
RM_Record& RM_Record::operator= (const RM_Record &record){
  if (this != &record){
    if(this->data != NULL)  // make sure the memory allocation is for the
      delete [] data;       // correct size of the record
    this->size = record.size;
    this->data = new char[size];
    memcpy(this->data, record.data, record.size);
    this->rid = record.rid;
  }
  return (*this);
}


//检索指向记录数据的指针
RC RM_Record::GetData(char *&pData) const {
  if(data == NULL || size == INVALID_RECORD_SIZE)
    return (RM_INVALIDRECORD);
  pData = data;
  return (0);
}


//检索RID
RC RM_Record::GetRid (RID &rid) const {
  RC rc;
  if((rc = (this->rid).isValidRID()))
    return rc;
  rid = this->rid;
  return (0);
}


//通过给定的RID以及特定的数据来设置这条记录
RC RM_Record::SetRecord(RID rec_rid, char *recData, int rec_size){
  RC rc;
  if((rc = rec_rid.isValidRID()))
    return RM_INVALIDRID;
  if(rec_size <= 0 )
    return RM_BADRECORDSIZE;
  rid = rec_rid;

  if(recData == NULL)
    return RM_INVALIDRECORD;
  size = rec_size;
  if (data != NULL)
    delete [] data;
  data = new char[rec_size];
  memcpy(data, recData, size);
  return (0);
}


