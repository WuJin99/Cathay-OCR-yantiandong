#pragma once
#include<string>
#include <vector>
#include "Utils.h"
#include "Console.h"
#include "FtpClient.h"
using namespace std;
class PicSave
{
public:
	PicSave();
	~PicSave();
public:

	struct PicText {
		string text;
		cv::Point org;
		int fontFace;
		float fontScale;
		cv::Scalar color;
		double thickness;
	};

	struct PicRec {
		int x;
		int y;
		int w;
		int h;
		int r;
		int g;
		int b;
	};

	bool SavePic(shared_ptr<cv::Mat> pImg, const std::string &path,std::shared_ptr<std::vector<PicText>> textArr=nullptr, 
		std::shared_ptr < std::vector<PicRec>> recArr=nullptr, bool saveToFtp = false, int img_label = -1, const std::string &ftpPath = "");
	bool SavePicToFtp(int img_label, const std::string &path, const std::string &FTPPath);
	void ProcessPic(const GRpc_RepeadResult &result);
	void GetErrorSavePath(const std::string &path);
	void ReSaveToFtp();
	bool Start();
	void Stop();

	std::string GetImdgStr(ImdgResult::ImdgInfo::IMDG_TYPE type);
	std::string GetDamageStr(DamageResult::DamageInfo::DAMAGE_TYPE type);
	std::string GetDamageLevelStr(DamageResult::DamageInfo::DAMAGE_LEVEL level);
	std::string GetDoorStr(int type);
	std::string GetDoorOpenStr(DoorOPENResult::OPEN_TYPE type);
	bool SaveToFtp(string& src, string& ftp);
public:
	bool m_bRun;
	thread m_thFtp;
	boost::mutex m_WaitSaveFtpMutex;
	struct tSaveFtp
	{
		string id;			//每个任务的标识
		string src_path;	//源文件路径
		string ftp_path;	//上传的FTP路径
		int count;			//尝试上传的次数
	};
	struct tSavePic
	{
		bool isSaveToFtp;				//是否上传FTP
		shared_ptr<cv::Mat> pImg;		//图片数据
		int img_label;					//图片标识
		string src_path;				//保存的本地路径
		string ftp_path;				//FTP路径
		std::shared_ptr<std::vector<PicText>> txtArr;	//照片上需要显示的文字
		std::shared_ptr<std::vector<PicRec>> rectArr;	// 照片上需要绘制的ROI
	};
	
	SyncQueue<tSaveFtp> m_SaveFtp;		//上传FTP队列
	deque<tSaveFtp> m_WaitSaveFtp;		//待上传FTP数据的管理队列
	SyncQueue<tSavePic> m_SavePicQue;	//保存本地的

	//ftp path
	std::string m_FtpSaveErrorPath;		//上传FTP失败的FTP信息保存路径
	int m_nFtpFailCount;//FTPFail文件夹中的文件个数
};

