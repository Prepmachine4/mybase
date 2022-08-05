
#include <unistd.h>
#include <sys/types.h>
#include "pf.h"
#include "rm_internal.h"/


RM_Manager::RM_Manager(PF_Manager &pfm) : pfm(pfm){
}

RM_Manager::~RM_Manager(){
}

//给定一个文件名和记录的大小来创建一个文件，记录的大小必须是可用的，文件名必须是目录中未存在的文件。
RC RM_Manager::CreateFile (const char *fileName, int recordSize) { 
  int rc = 0;
  //进行检查
  if(fileName == NULL)//如果文件名未空，返回错误报告错误的文件名
    return (RM_BADFILENAME);
 
  if(recordSize <= 0 || recordSize > PF_PAGE_SIZE)//如果记录的大小不合适，返回错误报告错误的文件大小
    return RM_BADRECORDSIZE;


  // 创建文件
  rc = pfm.CreateFile(fileName);
  //rc不等于0说明出现了错误，在pf中会返回错误信息，所以我们此时也返回错误信息
  if(rc!=0) return (rc);

//然后为了将记录的大小信息写入到文件头中，以及根据这来计算出文件头的其他信息
  //我们利用PF的文件处理函数给创建的文件分配页面，然后利用页处理函数来获得分配的页的页码给page
  PF_PageHandle PHF; PF_FileHandle FHF;
  struct RM_FileHeader *header;
  rc = pfm.OpenFile(fileName, FHF）
  if(rc!=0) return (rc);
  int page;
  if((rc = FHF.AllocatePage(PHF)) || (rc = PHF.GetPageNum(page)))
    return (rc);
  char *pData;
  rc = PHF.GetData(pData)//通过PHF对象来判断这个页面里面是否有数据,如果可以正常得到
   //说明有数据，需要清洗这个页面
  if(rc == 0)goto cleanup_and_exit;
  
  //创建文件头的结构体，并且在结构体中写入文件头应该有的数据
   (struct RM_FileHeader *) header;
  header->recordSize = recordSize;
  header->numRecordsPerPage = (PF_PAGE_SIZE - bitmapSize - bitmapOffset)/recordSize;
  header->bitmapSize = bitmapSize;
  header->bitmapOffset = bitmapOffset;
  header->numPages = 1;
  header->firstFreePage = NO_MORE_FREE_PAGES;
  cleanup_and_exit:
  int rc2;
  //确保mark操作，unpin操作以及关闭操作都能正常完成后才能确保文件创建真正的成功
  if((rc2 = fh.MarkDirty(page)) || (rc2 = fh.UnpinPage(page)) || (rc2 = pfm.CloseFile(fh)))
    return (rc2);
  return (rc); 
}

/*
 * 销毁一个文件
 */
RC RM_Manager::DestroyFile(const char *fileName) {
  if(fileName == NULL)
    return (RM_BADFILENAME);
  int rc;
  rc = pfm.DestroyFile(fileName)//销毁文件，如果成功返回0，失败返回错误信息
   return (rc);
}


RC RM_Manager::SetUpFH(RM_FileHandle& fileHandle, PF_FileHandle &fh, struct RM_FileHeader* header){
  //设置私有变量
  memcpy(&fileHandle.header, header, sizeof(struct RM_FileHeader));
  fileHandle.pfh = fh;
  fileHandle.header_modified = false;
  fileHandle.openedFH = true;

  // 确保文件头是可用的
  if(! fileHandle.isValidFileHeader()){
    fileHandle.openedFH = false;
    return (RM_INVALIDFILE);
  }
  return (0);
}


//kkkkkkerskkcxxkkfcxt 
RC RM_Manager::OpenFile   (const char *fileName, RM_FileHandle &fileHandle){
  if(fileName == NULL)       //文件名不存在返回错误文件名
    return (RM_BADFILENAME);
  // 如果filehandle已经打开了另一个文件，退出并返回相应报错
  if(fileHandle.openedFH == true)  //
    return (RM_INVALIDFILEHANDLE);

  int rc;
  // 打开文件
  PF_FileHandle fh;//
  if((rc = pfm.OpenFile(fileName, fh)))
    return (rc);

  //得到内存池里的第一个页面，并且用它来设置文件头
  PF_PageHandle ph;
  PageNum page;
  if((rc = fh.GetFirstPage(ph)) || (ph.GetPageNum(page))){
    fh.UnpinPage(page);
    pfm.CloseFile(fh);
    return (rc);
  }
  char *pData;
  ph.GetData(pData);
  struct RM_FileHeader * header = (struct RM_FileHeader *) pData;

  // 利用setupFH函数将取得的头文件信息放入到内存中，并且将这个handle方法标记为使用中
  //这样再打开的文件就会报错，保证每次只能打开一个
  rc = SetUpFH(fileHandle, fh, header);
  if (rc != 0) pfm.CloseFile(fh);//如果头文件信息更新失败，也返回错误信息

  // 如果有错误出现，关闭这个实例
  
  return (rc); //没有错误，返回rc
}


RC RM_Manager::CleanUpFH(RM_FileHandle &fileHandle){
  if(fileHandle.openedFH == false)//如果已经为false，说明未带来，不需要清楚，返回相应错误
    return (RM_INVALIDFILEHANDLE);
  fileHandle.openedFH = false;//将openedFH置为false，即表述filehandle已关闭，可以打开信息的
  return (0);
}


 //关闭文件
RC RM_Manager::CloseFile  (RM_FileHandle &fileHandle) {
  int rc;
  PF_PageHandle ph;
  PageNum page;
  char *pData;

 
  //如果头文件被修改了，把第一个页面放到缓冲池中，更新它的内容，并且标记为dirty，这样在写回硬盘时会对其更新
  if(fileHandle.header_modified != false){
      rc = (fileHandle.pfh.GetFirstPage(ph)||ph.GetPageNum(page))
      if(rc!=0) return (rc);
      rc = ph.GetData(pData)
    if((rc!=0){
      int rc2;
      rc2 = fileHandle.pfh.UnpinPage(page)
      if((rc2 != 0)
        return (rc2);
      return (rc);
    }
   rc = (fileHandle.pfh.MarkDirty(page)||fileHandle.pfh.UnpinPage(page))
      if(rc!=0) return (rc);

  }
  
  if((rc = pfm.CloseFile(fileHandle.pfh)))
    return (rc);

  //利用之前的CleanUpPH方法对filehandle参数重置，将filehndle置为未打开状态
  if((rc = CleanUpFH(fileHandle)))
    return (rc);

  return (0);
}
