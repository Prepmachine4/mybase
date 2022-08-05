
#include "rm_rid.h"
#include "rm_internal.h"

RID::RID(){
  page = INVALID_PAGE; // 初始化指向不可用页或slot的RID
  slot = INVALID_SLOT;
}

RID::RID(PageNum pageNum, SlotNum slotNum) {
  page = pageNum;
  slot = slotNum;
}

RID::~RID(){}

/*
 * 将RID的内容从硬盘搞到内存
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
 *返回RID的页码
 */
RC RID::GetPageNum(PageNum &pageNum) const {
  //if(page == INVALID_PAGE) return RM_INVALIDRID;
  pageNum = page;
  return 0;
}

/*
 * 返回RID的slot
 */
RC RID::GetSlotNum(SlotNum &slotNum) const {
  //if(slot == INVALID_SLOT) return RM_INVALIDRID;
  slotNum = slot;
  return 0;
}

/*
 * 检验RID是否可用
 */
RC RID::isValidRID() const{
  if(page > 0 && slot >= 0)
    return 0;
  else
    return RM_INVALIDRID;
}