
#include <unistd.h>
#include <sys/types.h>
#include "pf.h"
#include "rm_internal.h"/


RM_Manager::RM_Manager(PF_Manager &pfm) : pfm(pfm){
}

RM_Manager::~RM_Manager(){
}

//����һ���ļ����ͼ�¼�Ĵ�С������һ���ļ�����¼�Ĵ�С�����ǿ��õģ��ļ���������Ŀ¼��δ���ڵ��ļ���
RC RM_Manager::CreateFile (const char *fileName, int recordSize) { 
  int rc = 0;
  //���м��
  if(fileName == NULL)//����ļ���δ�գ����ش��󱨸������ļ���
    return (RM_BADFILENAME);
 
  if(recordSize <= 0 || recordSize > PF_PAGE_SIZE)//�����¼�Ĵ�С�����ʣ����ش��󱨸������ļ���С
    return RM_BADRECORDSIZE;


  // �����ļ�
  rc = pfm.CreateFile(fileName);
  //rc������0˵�������˴�����pf�л᷵�ش�����Ϣ���������Ǵ�ʱҲ���ش�����Ϣ
  if(rc!=0) return (rc);

//Ȼ��Ϊ�˽���¼�Ĵ�С��Ϣд�뵽�ļ�ͷ�У��Լ���������������ļ�ͷ��������Ϣ
  //��������PF���ļ����������������ļ�����ҳ�棬Ȼ������ҳ����������÷����ҳ��ҳ���page
  PF_PageHandle PHF; PF_FileHandle FHF;
  struct RM_FileHeader *header;
  rc = pfm.OpenFile(fileName, FHF��
  if(rc!=0) return (rc);
  int page;
  if((rc = FHF.AllocatePage(PHF)) || (rc = PHF.GetPageNum(page)))
    return (rc);
  char *pData;
  rc = PHF.GetData(pData)//ͨ��PHF�������ж����ҳ�������Ƿ�������,������������õ�
   //˵�������ݣ���Ҫ��ϴ���ҳ��
  if(rc == 0)goto cleanup_and_exit;
  
  //�����ļ�ͷ�Ľṹ�壬�����ڽṹ����д���ļ�ͷӦ���е�����
   (struct RM_FileHeader *) header;
  header->recordSize = recordSize;
  header->numRecordsPerPage = (PF_PAGE_SIZE - bitmapSize - bitmapOffset)/recordSize;
  header->bitmapSize = bitmapSize;
  header->bitmapOffset = bitmapOffset;
  header->numPages = 1;
  header->firstFreePage = NO_MORE_FREE_PAGES;
  cleanup_and_exit:
  int rc2;
  //ȷ��mark������unpin�����Լ��رղ�������������ɺ����ȷ���ļ����������ĳɹ�
  if((rc2 = fh.MarkDirty(page)) || (rc2 = fh.UnpinPage(page)) || (rc2 = pfm.CloseFile(fh)))
    return (rc2);
  return (rc); 
}

/*
 * ����һ���ļ�
 */
RC RM_Manager::DestroyFile(const char *fileName) {
  if(fileName == NULL)
    return (RM_BADFILENAME);
  int rc;
  rc = pfm.DestroyFile(fileName)//�����ļ�������ɹ�����0��ʧ�ܷ��ش�����Ϣ
   return (rc);
}


RC RM_Manager::SetUpFH(RM_FileHandle& fileHandle, PF_FileHandle &fh, struct RM_FileHeader* header){
  //����˽�б���
  memcpy(&fileHandle.header, header, sizeof(struct RM_FileHeader));
  fileHandle.pfh = fh;
  fileHandle.header_modified = false;
  fileHandle.openedFH = true;

  // ȷ���ļ�ͷ�ǿ��õ�
  if(! fileHandle.isValidFileHeader()){
    fileHandle.openedFH = false;
    return (RM_INVALIDFILE);
  }
  return (0);
}


//kkkkkkerskkcxxkkfcxt 
RC RM_Manager::OpenFile   (const char *fileName, RM_FileHandle &fileHandle){
  if(fileName == NULL)       //�ļ��������ڷ��ش����ļ���
    return (RM_BADFILENAME);
  // ���filehandle�Ѿ�������һ���ļ����˳���������Ӧ����
  if(fileHandle.openedFH == true)  //
    return (RM_INVALIDFILEHANDLE);

  int rc;
  // ���ļ�
  PF_FileHandle fh;//
  if((rc = pfm.OpenFile(fileName, fh)))
    return (rc);

  //�õ��ڴ����ĵ�һ��ҳ�棬���������������ļ�ͷ
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

  // ����setupFH������ȡ�õ�ͷ�ļ���Ϣ���뵽�ڴ��У����ҽ����handle�������Ϊʹ����
  //�����ٴ򿪵��ļ��ͻᱨ����֤ÿ��ֻ�ܴ�һ��
  rc = SetUpFH(fileHandle, fh, header);
  if (rc != 0) pfm.CloseFile(fh);//���ͷ�ļ���Ϣ����ʧ�ܣ�Ҳ���ش�����Ϣ

  // ����д�����֣��ر����ʵ��
  
  return (rc); //û�д��󣬷���rc
}


RC RM_Manager::CleanUpFH(RM_FileHandle &fileHandle){
  if(fileHandle.openedFH == false)//����Ѿ�Ϊfalse��˵��δ����������Ҫ�����������Ӧ����
    return (RM_INVALIDFILEHANDLE);
  fileHandle.openedFH = false;//��openedFH��Ϊfalse��������filehandle�ѹرգ����Դ���Ϣ��
  return (0);
}


 //�ر��ļ�
RC RM_Manager::CloseFile  (RM_FileHandle &fileHandle) {
  int rc;
  PF_PageHandle ph;
  PageNum page;
  char *pData;

 
  //���ͷ�ļ����޸��ˣ��ѵ�һ��ҳ��ŵ�������У������������ݣ����ұ��Ϊdirty��������д��Ӳ��ʱ��������
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

  //����֮ǰ��CleanUpPH������filehandle�������ã���filehndle��Ϊδ��״̬
  if((rc = CleanUpFH(fileHandle)))
    return (rc);

  return (0);
}
