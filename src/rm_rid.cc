
#include "rm_rid.h"
#include "rm_internal.h"

RID::RID(){
  page = INVALID_PAGE; // ��ʼ��ָ�򲻿���ҳ��slot��RID
  slot = INVALID_SLOT;
}

RID::RID(PageNum pageNum, SlotNum slotNum) {
  page = pageNum;
  slot = slotNum;
}

RID::~RID(){}

/*
 * ��RID�����ݴ�Ӳ�̸㵽�ڴ�
 */
RID& RID::operator= (const RID &rid){
  if (this != &rid){
    this->page = rid.page;
    this->slot = rid.slot;
  }
  return (*this);
}

bool RID::operator== (const RID &rid) const{
  return (this->page == rid.page && this->slot == rid.slot);
}

/*
 *����RID��ҳ��
 */
RC RID::GetPageNum(PageNum &pageNum) const {
  //if(page == INVALID_PAGE) return RM_INVALIDRID;
  pageNum = page;
  return 0;
}

/*
 * ����RID��slot
 */
RC RID::GetSlotNum(SlotNum &slotNum) const {
  //if(slot == INVALID_SLOT) return RM_INVALIDRID;
  slotNum = slot;
  return 0;
}

/*
 * ����RID�Ƿ����
 */
RC RID::isValidRID() const{
  if(page > 0 && slot >= 0)
    return 0;
  else
    return RM_INVALIDRID;
}