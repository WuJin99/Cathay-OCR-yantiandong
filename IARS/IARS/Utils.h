#ifndef UTILS_H
#define UTILS_H
#include <SDKDDKVer.h>
#include <HCNetSDK.h>
#include <plaympeg4.h>

#include <fstream>
#include <thread>                // std::thread
#include <mutex>                // std::mutex, std::unique_lock
#include <condition_variable>    // std::condition_variable
// boost
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/typeof/typeof.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
//
#include "csv.hpp"
#include <opencv2/opencv.hpp>
#include <direct.h>
#include <string>
#include <vector>
#include <string>
#include <map>
#include <atomic>
#include<cstdio>
#include "ocrs.pb.h"

#include "SyncQueue.h"
#include "MySyncQueue.h"

class TaskManager;
class PicSave;
class Event;
#define BOOST_DATE_TIME_SOURCE

using namespace boost::filesystem;
using namespace boost::uuids;
using boost::property_tree::ptree;
using boost::lexical_cast;
using namespace boost::posix_time;
using namespace std;

#define CAM_AXIS_DISCONN_TIME_OUT 20
//业务类型（0:卸船作业、1：装船作业\2:未知）
////卸船
//const string ShipToTruck = "0";
////装船
//const string TruckToShip = "1";
////未知
//const string WorkTypeUnknow = "2";
////发箱
//const string YardToTruck = "3";
////收箱
//const string TruckToYard = "4";
////场内倒箱
//const string YardToYard = "5";
////场内抓箱
//const string LockInYard = "6";

//发箱
const string YardToTruck = "0";
//收箱
const string TruckToYard = "1";
//未知
const string WorkTypeUnknow = "2";

enum
{
	//work_status作业完成状态（0:作业未完成、1：作业完成、2：发送结果、3：寄桥作业中间寄存状态[stay_work为1时使用]）
	WS_UNFINISH = 0,
	WS_FINISH,
	WS_SENDRESULT,
	WS_STAYWORK,
	//carry_cont带箱状态：0=无效，1=带箱，2=不带箱
	CARRY_INVALID = 0,
	CARRY_HEAVY,
	CARRY_EMPTY,
	//position压箱在托架上位置:0：无效，1：F (front) 2：M (middle) 3：A (aft)
	POS_INVALID=0,
	POS_FRONT,
	POS_MID,
	POS_AFT,
};

enum
{
	//相机的安装位置
	CAM_POS_NULL = 0,
	CAM_POS_FRONT,//吊具摄像头安装位置，相对于车头方向
	CAM_POS_BACK, //
};
enum
{
	OCR_COM_FAULT = 1,
	OCR_CAM_FAULT = 2,	// 相机断连，对应上报参数 Disconnection
	OCR_SYS_FAULT = 3,
	OCR_SYS_NORMAL = 4,	// 正常
	OCR_CAM_MULTI_FAULT = 5,	// 多个相机存在多种故障，需要上位机获取所有上报参数
	OCR_CAM_OCCLUSION = 6,	// 相机遮挡，对应上报参数 Occlusion
	OCR_CRS_DISCONNECT = 7,//CRS断联
};
// 常量
const int MAX_JPEG_BUFSIZE = 4090 * 2160 * 3 / 2;
extern std::atomic<bool> g_SpaceIsEnough;//标识磁盘空间是否足够
//-------test-------
extern std::atomic<int> g_testLane;//车道
extern std::atomic<int> g_testCatch;//车道
extern std::mutex g_testTaskIdMtx;
extern std::string g_testTaskId;//车道
//-------test-------


extern std::mutex g_vecTruckNumMutex;
extern SyncQueue <OCRAny> g_WaitPubMsg;//待发送消息
extern std::shared_ptr<PicSave> g_pPicSave;
extern StateInfo g_StateInfo;
extern std::mutex g_mtxStateInfo;
extern int g_StateInfoType;
extern int g_OcrSystemState;
extern std::string g_strDisconnCameraIp;
extern bool g_isOCRCamInitFail;

struct TruckNumHold
{
	char src;
	char dst;

};
extern std::vector<TruckNumHold> g_vecTruckNum;

extern std::map<std::string, std::string> g_mapTruckNum;
struct TruckNumSend
{
	bool isSend = false;
	string truckNum;
};

//保存本次作业图片路径
struct tPicItem
{
	bool cnFirstSend;//是否优先使用箱号结果
	short P;//某款相机没有ptz，所以自己写到图片上
	short T;
	short Z;
	int img_label;
	int group;
	int lane;
	bool crs_deal;       //crs是否返回处理结果    }
	time_t tCreate;      //记录数据产生时的时间戳 }这2个字段用于从g_arrPicItem删除已经处理或严重超时的tPicItem
	std::string task_id; //记录任务号
	std::string ori_name;//相机拼接的名称，HKGun中SpliceFileName返回值
	std::string ori_img_name;//原始图片名称：ori_name_ORI.jpg
	std::string cvt_path;//处理后图片路径，path\xxx_CVT.jpg，如果不为空，表示用于识别的是cvt_path，否则为ori_path
	std::string cam_ip;
	std::string cam_name; //保存相机的名称
	//用于在图片上标记识别结果
	std::shared_ptr<cv::Mat> pImgOri;//原图
	std::shared_ptr<cv::Mat> pImgCvt;//处理后图片


	struct tPicParam
	{
		ocrs::pb::GRpc_RecogParam param;//识别参数
		float fRetConfidence;           //识别结果置信度
		int truckType;//集卡类型
	};
	std::vector<tPicParam> arrParam;//用于保存识别结果参数，在将识别结果显示到图片上时使用
	TaskManager *pTaskMgr;//设置关联的TaskManager*

	tPicItem()
	{
		P = 0;
		T = 0;
		Z = 0;
		img_label = 0;
		group = 0;
		crs_deal = false;
		tCreate = time(nullptr);
		pTaskMgr = nullptr;
	}
	//获取ROI的xy，只是为了将识别结果显示到图片上，需要跟据设置的ROI做偏移，用识别类型做比较，如果一张照片用于识别多次相同的结果，则会使用第一个ROI，此时不再考虑准确性
	bool GetRoiSrcByRType(int rtype, ocrs::pb::Rect& roi)
	{
		for (auto&t: arrParam)
		{
			if (rtype == t.param.type())
			{
				roi = t.param.roi();
				return true;
			}
		}
		return false;
	}
};
extern std::atomic<int> g_picNum;
extern std::vector<tPicItem> g_arrPicItem;
extern std::mutex g_mtxPicItem;
extern std::mutex g_TruckNumMatchMutex;

extern std::mutex g_IARSTruckPmnMutex;
extern int GetGroupByImgLabel(int img_label);
extern std::string GetCamNameByImgLabel(int img_label);
extern int GetTruckTypeByImgLabel(int img_label);
extern std::mutex g_imgLabelMutex;
extern gui::pb::TruckListReply g_TruckListReply;
extern std::atomic<int> g_nRecvRetLabel; // 接收到识别结果的label序号

struct tCnItem
{
	bool cnFirst;
	std::string cn_str;//cnm+cnn
	std::string cnm;
	std::string cnn;
	std::string iso;
	float cnm_conf;
	float cnn_conf;
	float iso_conf;
	float conf;//综合识别率，单项识别率的平方的和
	bool bCheck;//cn_str是否通过校验
	int nRetCount;//cn_str计数
	int nValidCount;//识别结果有效字符个数
	int64 nowTime;//获取到结果的时间，当第一次尝试抓箱，箱子错误时，会放弃抓箱，在抓第二个箱，此时应发送最新的箱号
	std::set<string> setCamIp;//记录相机IP

	tCnItem()
	{
		Clear();
	}
	std::string to_string()
	{
		std::stringstream ss;
		ss << cnm << " " << cnn << " " << iso << " confidence:" << cnm_conf << "," << cnn_conf << "," << iso_conf << " ";
		ss << "Check:" << bCheck << ",RetCount:" << nRetCount << ",camip:";
		for (auto&cam_ip : setCamIp)
		{
			ss << cam_ip << " ";
		}
		return ss.str();
	}
	void UpdateConf()
	{
		conf = cnm_conf * cnm_conf + cnn_conf * cnn_conf + iso_conf * iso_conf;
	}
	void Clear()
	{
		cn_str = "";
		cnm = "";
		cnn = "";
		iso = "";
		cnm_conf = 0;
		cnn_conf = 0;
		iso_conf = 0;
		conf = 0;
		bCheck = false;
		nRetCount = 0;
		nValidCount = 0;
		setCamIp.clear();
	}
};

extern SyncQueue<OCRAny*> g_queueSubMsg;
extern std::string GetUUID();//获取uuid
/*
*stateInfo 组包
*type 状态项类型：软件、PLC、相机
*name 名称：camer_left、一体机、识别主机
*desc 描述：CPU、内存、存储等占用情况
*runState 运行状态：正常、报警、故障
*ipStr IP地址
*/
extern void PackStateInfo(StateInfo::ITEM_TYPE type, std::string name, std::string desc,
	StateInfo::RUN_STATE runState, std::string ipStr = "", std::string cpuStr = "", std::string memStr = "",
	std::string gpuStr = "");//设备状态组包
struct JPEGPic// 自定义定义数据结构
{
    int width;  // 图片宽
    int height; // 图片高

    BYTE *buf;
    LONG size;
    JPEGPic()
        :buf(nullptr)
        , size(0)
    {
        ;
    }
};

struct LicPlateResult
{
	int lane;                // 相机所属的车道
	int nBelieve;                // 整个车牌的置信度
	std::string sLicPlate;       // 车牌识别结果
	std::string sLane;			 // 相机所属的车道
	std::string sCamIp;          // 相机IP
	std::string sFileName;       // 图片名称

	unsigned int uiRecong;		//识别时间
	std::string strRecTime;     //识别结果时间戳
	JPEGPic vehPic;				// 车辆图
	LicPlateResult& operator=(const LicPlateResult& r)
	{
		if (this != &r)
		{
			nBelieve = r.nBelieve;
			sLicPlate = r.sLicPlate;
			sLane = r.sLane;
			sCamIp = r.sCamIp;
			uiRecong = r.uiRecong;
			strRecTime = r.strRecTime;
			sFileName = r.sFileName;

			if (vehPic.buf)
			{
				delete vehPic.buf;
				vehPic.buf = nullptr;
			}
			if (r.vehPic.buf && r.vehPic.size > 0)
			{
				vehPic.size = r.vehPic.size;
				vehPic.buf = new BYTE[vehPic.size];
				memcpy(vehPic.buf, r.vehPic.buf, vehPic.size);
				vehPic.height = r.vehPic.height;
				vehPic.width = r.vehPic.width;
			}
		}
		return *this;
	}
};
typedef std::vector<std::shared_ptr<ocrs::pb::GRpc_RImage>> RRIMAGELIST;

// 照片文件读写函数
extern bool ReadImage(const char*pszImg, BYTE*& pBuf, LONG& nLen);
extern void WriteImage(std::string path, const BYTE *buf, const LONG size);


// 将字符串转换为区域
extern cv::Rect str2rect(const char *str);
// 将区域存储为字符串
extern std::string rect2str(const cv::Rect& rect);
// 将字符串格式的变换矩阵转化为Mat格式
extern cv::Mat str2mat(const char *str);
// 将Mat格式的变换矩阵转化为字符串
extern std::string mat2str(const cv::Mat& mat);

// 画矩形框
extern  void DrawRectAreas(cv::Mat &img, int x, int y, int w, int h, int r=255, int g=255, int b=0);

extern void UnevenLightCompensate(cv::Mat &image, int blockSize = 12);

class MyRect        // 自定义区域类
{
public:
    cv::Rect r;
public:
    MyRect(const char *str) { r = str2rect(str); }
    MyRect(){}
    operator cv::Rect&(){ return r; }
    operator std::string() const
    {
        return rect2str(r);
    }
};

class MyMat         // 自定义矩阵类
{
public:
    cv::Mat m;
public:
    MyMat(){}
    MyMat(const char *str) { m = str2mat(str); }
    operator cv::Mat&() { return m; }
    operator std::string() const { return mat2str(m); }
    void StrToMat(const char* str){ m = str2mat(str); }
};


struct tCapImage
{
	std::shared_ptr<JPEGPic> pJPEG;
	short P;
	short T;
	short Z;
	Event *event;
	std::string eventName;
	std::shared_ptr<cv::Mat> pMat;
};

extern string BufToHex(unsigned char *pBuf, int nLen);

// 进制转换函数
extern WORD DecToHex(WORD wDec);
extern WORD HexToDec(WORD wHex);
extern bool isLetter(char c);

extern bool isNumber(char c);

//获取应用程序根目录
extern  std::string GetAppPath();
extern  bool MakeDirs(std::string filePath);
extern std::string GetSysTimeString();
// CRC 校验
unsigned short CRC16(const unsigned char *buf, int size);
// 返回毫米级时间戳
extern void GetCurrentStamp64(int64_t &t);
int64_t GetCurrentStamp64();
// sleep时间单位毫秒级
extern void SleepMilliseconds(int64_t ms);
//相邻2个字节交换位置（高低位相反）
extern void SwapByte(unsigned char *pBuf, int nLen);
extern int get_file_names(const std::string& dir, const std::string& type, std::vector<std::string>& filenames);
extern int get_last_dirs(const std::string& dir, std::vector<std::string>& dirnames);
//type是旋转方式，默认-1为按照原点旋转，0绕x轴旋转，1绕y轴旋转
extern void set_rotate_angle(const cv::Mat& src, cv::Mat& dst, double angle);
extern std::string GetDateString();
extern std::string GetTimeString();
extern std::string GetHttpTimeString();
extern std::string GetLocalIp();
//分割字符串
int SepString(const std::string& str, std::vector<std::string>& arr, char ch, bool remove_space = true);
/**
* \brief 转换八进制字符串到中文
* \param str 源字符串
* \return 转换后的字符串
*/
extern std::string ConvertChs(const std::string& str);
/**
* \brief 转换protobuf中DebugString的八进制字符串到中文
* \param pszSrc 源字符串
* \param nLen 源字符串长度
* \return 转换后的字符串
*/
std::string ConvertChs(const char *pszSrc, int nLen);

extern bool isEqual(const std::string& str1, const std::string& str2);
//写文件 path:路径
//extern void WriteToLocalFile(std::string path, const OCRAny &any);
//extern void WriteToLocalFile(std::string path, const char *buffer, int size);
////读文件
//extern void ReadFromLocalFile(std::string path, string &buffer);

extern void GetFilesNames(const std::string& rootPath,std::vector<string> &name);
//删除文件，适用没有后缀的
extern bool RemoveLocalFile(const string& rootPath);
//获取磁盘存储 单位：G
extern bool GetDiskSpace(std::string path, int& nTotalG, int&nFreeG);
//写proto到本地文件
extern void WriteProtoToLocalFile(const string &path, const TaskInfo &oa);
//读本地文件到proto
extern void ReadLocalFileToProto(const string &path, TaskInfo &oa);
//写proto到本地文件
extern void WriteProtoToLocalFile(const string &path, const FtpPath &oa);
//读本地文件到proto
extern bool ReadLocalFileToProto(const string &path, FtpPath &oa);
//写proto到本地文件
extern void WriteProtoToLocalFile(const string &path, const OCRAny &oa);

//写string到本地txt文件
extern void WriteStringToTxt(const string &path, const string &src);
//读本地文件到proto
extern void ReadLocalFileToProto(const string &path, OCRAny &oa);

//拼接文件名称：时间_相机位置_相机IP_evtName
extern std::string SpliceFileName(std::string strCamIp, std::string evtName);
/**
* \brief 判断是否是八进制字符串
* \param pszSrc 源字符串
* \param nLen 长度
* \return 八进制字符串返回true
*/
extern bool IsOctStr(const char* pszSrc, int nLen);
/**
* \brief 八进制字符串到BYTE
* \param pszOct 源字符串
* \param nLen 长度
* \return 转换后的值
*/
extern unsigned char Oct2Byte(const char* pszOct, int nLen);
/**
* \brief 从配置的车号映射列表中找到对应的车号
* \param pmn 车号
* \return 是否找到映射的车号
*/
extern bool GetTruckNumFromMap(string &pmn);
/**
* \brief 转换protobuf中DebugString的八进制字符串到中文
* \param pszSrc 源字符串
* \param nLen 源字符串长度
* \return 转换后的字符串
*/
extern std::string ConvertChs(const char *pszSrc, int nLen);
/**
* \brief 转换八进制字符串到中文
* \param str 源字符串
* \return 转换后的字符串
*/
extern std::string ConvertChs(const std::string& str);

/**
* 路径下文件数量
*/
extern size_t recursive_dir(const string& dir, int level, set<string>*pset);
/**
* \brief 清理磁盘空间
*/
extern void MakeEnoughSpace();
extern bool IsAllNumber(string& str);
extern void GetTruckNo(string& truck_no);
//从映射列表中获取对应的结果
extern bool GetPlateNumByTruckNo(string& truck_no, string& plate_num);
extern bool GetTruckNoByPlateNum(string& plate_num, string& truck_no);

extern void AddMainLog(const string& info);

extern double calc_image_pixel_avg(cv::Mat& image);

struct TruckNumInfo
{
	int count = 0;
	float conf = 0.0;
	int64_t time = GetCurrentStamp64();
	void Update(float conf_)
	{
		count++;
		if (conf_ > conf)
		{
			conf = conf_;
		}
		time = GetCurrentStamp64();
	}
};

extern string g_SavePath;
struct tMainLog
{
	string time;
	string path;
	string info;
	tMainLog()
	{
		SYSTEMTIME sysTime;
		GetLocalTime(&sysTime);
		char szTime[100] = {};
		sprintf_s(szTime, "[%d-%02d-%02d %02d:%02d:%02d.%03d] ",
			sysTime.wYear, sysTime.wMonth, sysTime.wDay,
			sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
		time = szTime;
		//存储到作业数据文件夹，自动管理磁盘空间
		sprintf_s(szTime, "%s\\%04d%02d%02d\\MainLog_%d%02d%02d.txt",
			g_SavePath.c_str(),
			sysTime.wYear, sysTime.wMonth, sysTime.wDay,
			sysTime.wYear, sysTime.wMonth, sysTime.wDay);
		path = szTime;
	}
};
extern SyncQueue<shared_ptr<tMainLog>> g_queueLog;

#endif
