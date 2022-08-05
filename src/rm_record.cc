

#include <unistd.h>
#include <sys/types.h>
#include "pf.h"
#include "rm_internal.h"

//���캯��
RM_Record::RM_Record(){
  size = INVALID_RECORD_SIZE; // ��ʼ����¼������
  data = NULL;
}
//��������
RM_Record::~RM_Record(){
  if(data != NULL) // ɾ����¼����������
    delete [] data;
}

//����¼��Ӳ�̸㵽�ڴ�����
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


//����ָ���¼���ݵ�ָ��
RC RM_Record::GetData(char *&pData) const {
  if(data == NULL || size == INVALID_RECORD_SIZE)
    return (RM_INVALIDRECORD);
  pData = data;
  return (0);
}


//����RID
RC RM_Record::GetRid (RID &rid) const {
  RC rc;
  if((rc = (this->rid).isValidRID()))
    return rc;
  rid = this->rid;
  return (0);
}


//ͨ��������RID�Լ��ض�������������������¼
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


