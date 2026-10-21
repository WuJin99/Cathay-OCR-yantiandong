#include "Utils.h"
#include "Console.h"
#include "AppConfig.h"

#include "TaskManager.h"
#include "PicSave.h"
// #include "CameraConfig.h"

//-------test-------
std::atomic<int> g_testLane;//车道
std::atomic<int> g_testCatch;//车道
std::mutex g_testTaskIdMtx;
std::string g_testTaskId;//车道
//-------test-------

SyncQueue <OCRAny> g_WaitPubMsg;//待发送消息
SyncQueue<OCRAny*> g_queueSubMsg;
std::atomic<bool> g_SpaceIsEnough;
std::atomic<int> g_picNum = 0;  // c++14中不允许使用拷贝构造，=0属于拷贝构造，可以改成列表初始化{0}
std::vector<tPicItem> g_arrPicItem;
std::mutex g_mtxPicItem;
std::mutex g_imgLabelMutex;
std::shared_ptr<PicSave> g_pPicSave = nullptr;
gui::pb::TruckListReply g_TruckListReply;

std::mutex g_vecTruckNumMutex;
StateInfo g_StateInfo;
int g_StateInfoType;
std::mutex g_mtxStateInfo;

std::vector<TruckNumHold> g_vecTruckNum;

struct Err_Code
{
	int cn_code;
	int truck_code;
	int door_code;
	int open_code;
}Sany_Code;//上报相机识别状态
int g_OcrSystemState;								// 系统状态，枚举
std::string g_strDisconnCameraIp;					// 断连相机 IP，多个 IP 以 ; 分隔
bool g_isOCRCamInitFail = false;
std::atomic<int> g_nRecvRetLabel = 0;
std::mutex g_TruckNumMatchMutex;
std::mutex g_IARSTruckPmnMutex;
std::map<std::string, std::string> g_mapTruckNum;
int GetGroupByImgLabel(int img_label)
{
	std::lock_guard<std::mutex> lck(g_mtxPicItem);
	for (auto&t:g_arrPicItem)
	{
		if (img_label == t.img_label)
			return t.group;
	}
	return 0;
}

int GetTruckTypeByImgLabel(int img_label)
{
	std::lock_guard<std::mutex> lck(g_mtxPicItem);
	for (auto&t : g_arrPicItem)
	{
		if (img_label == t.img_label)
		{
			if (t.arrParam.size() > 0)
			{

				return t.arrParam[0].truckType;
			}
			else
			{
				return 0;
			}
		}
	}
	return 0;
}
std::string GetCamNameByImgLabel(int img_label)
{
	std::lock_guard<std::mutex> lck(g_mtxPicItem);
	for (auto&t : g_arrPicItem)
	{
		if (img_label == t.img_label)
			return t.cam_name;
	}
	return "";
}

//获取uuid
std::string GetUUID()
{
	uuid a_uuid = random_generator()();
	return boost::uuids::to_string(a_uuid);
}

bool isEqual(const std::string& str1, const std::string& str2)
{
	return !strcmp(str1.c_str(), str2.c_str());
}
std::string GetAppPath()
{
    std::string fullpath = boost::filesystem::initial_path<boost::filesystem::path>().string();
    return fullpath;
}

bool MakeDirs(std::string filePath)
{
    // 文件夹不存在-创建
	try
	{
		bool bRet = boost::filesystem::create_directories(filePath);
		return bRet;
	}
	catch (std::exception& e)
	{
		printf("MakeDirs %s fail:%s", filePath.c_str(), e.what());
	}
	catch (...)
	{
		printf("MakeDirs %s fail.", filePath.c_str());
	}
	return false;
}
std::string GetTimeString()
{
	boost::posix_time::ptime pt = boost::posix_time::microsec_clock::local_time();
	char szTime[32] = {};
	sprintf_s(szTime, "%d-%02d-%02d %02d:%02d:%02d.%03d",
		(unsigned short)pt.date().year(),
		pt.date().month().as_number(),
		pt.date().day().as_number(),
		pt.time_of_day().hours(),
		pt.time_of_day().minutes(),
		pt.time_of_day().seconds(),
		pt.time_of_day().total_milliseconds() % 1000);
	return szTime;
}
std::string GetHttpTimeString()
{
	auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	struct tm* ptm = localtime(&tt);

	char date[60] = { 0 };

	sprintf(date, "%04d/%02d/%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday, (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);

	return std::string(date);
}
std::string GetLocalIp()
{
	return g_AppConfig.userCfg.localIp;
}

std::string GetSysTimeString()
{
	auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	struct tm* ptm = localtime(&tt);

	char date[60] = { 0 };

	sprintf(date, "%04d%02d%02d%02d%02d%02d", (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday, (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);

	return std::string(date);
}

extern void SleepMilliseconds(int64_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
std::mutex stamp_mutex;

extern void GetCurrentStamp64(int64_t &t)
{
    try
    {
        stamp_mutex.lock();

        // boost::posix_time::ptime epoch(boost::gregorian::date(2000, boost::gregorian::Jan, 1));

        // boost::posix_time::time_duration local_time_from_epoch;

        //local_time_from_epoch = boost::posix_time::microsec_clock::local_time() - epoch;

        // t = local_time_from_epoch.ticks() / 1000;

        auto time_now = std::chrono::system_clock::now();

        auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());

        t = duration_in_ms.count();

        stamp_mutex.unlock();

    }
    catch (...)
    {
        console->info("GetCurrentStamp64:failed");
    }
}

int64_t GetCurrentStamp64()
{
        auto time_now = std::chrono::system_clock::now();

        auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());

        return duration_in_ms.count();

}

void WriteImage(std::string path, const BYTE *buf, const LONG size)
{
    std::string save_pth = path;

    boost::filesystem::ofstream ofs;

    ofs.open(save_pth, std::ios::out | std::ios::binary);

    const char *p = (const char *)buf;

    ofs.write(p, size);

    ofs.close();

}

bool ReadImage(const char*pszImg, BYTE*& pBuf, LONG& nLen)
{
    nLen = 0;
    FILE *pFile = nullptr;
    fopen_s(&pFile, pszImg, "rb");
    if (!pFile)
    {
        return false;
    }
    fseek(pFile, 0, SEEK_END);
    long lSize = ftell(pFile);
	// 创建buf
	if (pBuf == NULL)
	{
		pBuf = new BYTE[lSize];
	}
    fseek(pFile, 0, SEEK_SET);

    fread(pBuf, 1, lSize, pFile);
    fclose(pFile);
    nLen = lSize;
    return true;
}

int get_file_names(const std::string& dir, const std::string& type, std::vector<std::string>& filenames)
{
    filenames.clear();
    path path(dir);
    if (!exists(path))
    {
        return -1;
    }
    directory_iterator end_iter;
    for (directory_iterator iter(path); iter != end_iter; ++iter)
    {
        if (is_regular_file(iter->status()))
        {
            int pos = iter->path().string().find_last_of(".");

            std::string str = iter->path().string().substr(pos + 1, type.length());

            if (type == str)
            {
                filenames.push_back(iter->path().string());
            }
        }

        if (is_directory(iter->status()))
        {
            get_file_names(iter->path().string(), type, filenames);
        }
    }

    return filenames.size();
}

int get_last_dirs(const std::string& dir, std::vector<std::string>& dirnames)
{
    dirnames.clear();
    path path(dir);
    if (!exists(path))
    {
        return -1;
    }

    directory_iterator end_iter;
    for (directory_iterator iter(path); iter != end_iter; ++iter)
    {
        if (is_directory(iter->status()))
        {
            dirnames.push_back(iter->path().string());
        }
    }
    return dirnames.size();
}

string BufToHex(unsigned char *pBuf, int nLen)
{
	char szHex[4] = {};
	string strHex;
	for (int nIdx = 0; nIdx < nLen; nIdx++)
	{
		sprintf_s(szHex, "%02X ", pBuf[nIdx]);
		strHex += szHex;
	}
	return strHex;
}

WORD DecToHex(WORD wDec)
{
    return (wDec / 1000) * 4096 + ((wDec % 1000) / 100) * 256 + ((wDec % 100) / 10) * 16 + (wDec % 10);
}

WORD HexToDec(WORD wHex)
{
    return (wHex / 4096) * 1000 + ((wHex % 4096) / 256) * 100 + ((wHex % 256) / 16) * 10 + (wHex % 16);
}

cv::Rect str2rect(const char *str)
{
    std::stringstream ss(str);
    cv::Rect rect;

    CSVRow row;
    ss >> row;
    ss.clear();
    ss.str(row[0]);
    ss >> rect.x;
    ss.clear();
    ss.str(row[1]);
    ss >> rect.y;
    ss.clear();
    ss.str(row[2]);
    ss >> rect.width;
    ss.clear();
    ss.str(row[3]);
    ss >> rect.height;
    return rect;
}

std::string rect2str(const cv::Rect& rect)
{
    std::stringstream ss;
    ss << rect.x << "," << rect.y << "," << rect.width << "," << rect.height;
    return ss.str();
}

cv::Mat str2mat(const char *str)
{
    if (str == NULL || str[0] == '\0')
        return cv::Mat();
    std::stringstream ss(str);
    CSVRow row;
    ss >> row;
    double e[9] = {
        1., 0., 0.,
        0., 1., 0.,
        0., 0., 1.
    };
    for (int i = 0; i < 9 && i < row.size(); i++)
    {
        ss.clear();
        ss.str(row[i]);
        ss >> e[i];
    }
    return (cv::Mat_<double>(3, 3) << e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8]);
}

std::string mat2str(const cv::Mat& mat)
{
    if (mat.empty())
        return "";
    CV_Assert(mat.type() == CV_64FC1 && mat.rows == 3 && mat.cols == 3);
    const double *p = mat.ptr<double>();
    std::stringstream ss;
    for (int i = 0; i < 8; i++)
    {
        ss << *p++ << ",";
    }
    ss << *p;
    return ss.str();
}

unsigned short CRC16(const unsigned char *buf, int size)
{
    unsigned short crc = 0xffff;
    for (int i = 0; i < size; i++)
    {
        crc = crc ^ buf[i];
        for (int j = 0; j < 8; j++)
        {
            unsigned char chk;
            chk = crc & 1;
            crc = crc >> 1;
            crc = crc & 0x7fff;
            if (chk == 1)
                crc = crc ^ 0xa001;
            crc = crc & 0xffff;
        }
    }
    return crc;
}
void set_rotate_angle(const cv::Mat& src, cv::Mat& dst, double angle)
{

		//cv::Size src_sz = src.size();

		//cv::Size dst_sz(src_sz.width, src_sz.height);

		//cv::Point center = cv::Point(src.cols / 2, src.rows / 2);//旋转中心

		//cv::Mat rot_mat = cv::getRotationMatrix2D(center, angle, 1.0);//获得仿射变换矩阵

		//cv::warpAffine(src, dst, rot_mat, dst_sz);
	int maxBorder = (int)(std::max(src.cols, src.rows)* 1.414); // 1.414 = sqrt(2)

	int dx = (maxBorder - src.cols) / 2;
	int dy = (maxBorder - src.rows) / 2;
	cv::copyMakeBorder(src, dst, dy, dy, dx, dx, cv::BORDER_CONSTANT);

	cv::Point2f center(dst.cols / 2, dst.rows / 2);
	cv::Mat rot_mat = cv::getRotationMatrix2D(center, angle, 1.0);
	cv::warpAffine(dst, dst, rot_mat, dst.size());

	float radian = angle / 180.0 * CV_PI;
	float sinVal = abs(sin(radian));
	float cosVal = abs(cos(radian));
	cv::Size dst_sz((int)(src.cols * cosVal + src.rows * sinVal),
		(int)(src.cols * sinVal + src.rows * cosVal));

	int x = (dst.cols - dst_sz.width) / 2;
	int y = (dst.rows - dst_sz.height) / 2;
	cv::Rect rect(x, y, dst_sz.width, dst_sz.height);
	dst = cv::Mat(dst, rect);
}

std::string GetDateString()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    struct tm* ptm = localtime(&tt);

    char date[60] = { 0 };

	sprintf(date, "%04d%02d%02d", (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday );

    return std::string(date);
}

void DrawRectAreas(cv::Mat &img, int x, int y, int w, int h, int r, int g, int b)
{
	try
	{
		cv::Rect rect(x, y, w, h);
		cv::rectangle(img, rect, cv::Scalar(r, g, b), 2.5, cv::LINE_8, 0);
	}
	catch (...)
	{
		console->error("DrawRectAreas error");
	}
}
int SepString(const std::string& str, std::vector<std::string>& arr, char ch, bool remove_space)
{
	int nStart = 0;
	int nEnd = 0;
	int nLen = str.length();
	for (; nStart < nLen;)
	{
		nEnd = str.find(ch, nStart);
		if (nEnd - nStart >= 0)
		{
			std::string one = str.substr(nStart, nEnd - nStart);
			//删除空格
			if (remove_space)
				one.erase(std::remove(one.begin(), one.end(), ' '), one.end());
			nStart = nEnd + 1;
			arr.push_back(one);
		}
		else
		{
			std::string one = str.substr(nStart, nLen - nStart);
			//删除空格
			if (remove_space)
				one.erase(std::remove(one.begin(), one.end(), ' '), one.end());
			nStart = nEnd + 1;
			arr.push_back(one);
			break;
		}
	}
	return arr.size();
}

void UnevenLightCompensate(cv::Mat &image, int blockSize /*= 12*/)
{
    if (image.channels() == 3) cvtColor(image, image, 7);
    double average = mean(image)[0];
    int rows_new = ceil(double(image.rows) / double(blockSize));
    int cols_new = ceil(double(image.cols) / double(blockSize));
    cv::Mat blockImage;
    blockImage = cv::Mat::zeros(rows_new, cols_new, CV_32FC1);
    for (int i = 0; i < rows_new; i++)
    {
        for (int j = 0; j < cols_new; j++)
        {
            int rowmin = i*blockSize;
            int rowmax = (i + 1)*blockSize;
            if (rowmax > image.rows) rowmax = image.rows;
            int colmin = j*blockSize;
            int colmax = (j + 1)*blockSize;
            if (colmax > image.cols) colmax = image.cols;
            cv::Mat imageROI = image(cv::Range(rowmin, rowmax), cv::Range(colmin, colmax));
            double temaver = mean(imageROI)[0];
            blockImage.at<float>(i, j) = temaver;
        }
    }
    blockImage = blockImage - average;
    cv::Mat blockImage2;
    resize(blockImage, blockImage2, image.size(), (0, 0), (0, 0), cv::INTER_CUBIC);
    cv::Mat image2;
    image.convertTo(image2, CV_32FC1);
    cv::Mat dst = image2 - blockImage2;
    dst.convertTo(image, CV_8UC1);
}


/**
* \brief 判断是否是八进制字符串
* \param pszSrc 源字符串
* \param nLen 长度
* \return 八进制字符串返回true
*/
bool IsOctStr(const char* pszSrc, int nLen)
{
	//至少4个字符，并且满足第0个字符为\，连续后三个字符为[0,7]
	if (nLen < 4)
		return false;
	return (pszSrc[0] == '\\' &&
		(pszSrc[1] >= '0' && pszSrc[1] <= '7') &&
		(pszSrc[2] >= '0' && pszSrc[2] <= '7') &&
		(pszSrc[3] >= '0' && pszSrc[3] <= '7'));
}

bool isLetter(char c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

bool isNumber(char c)
{
	return (c >= '0'&&c <= '9');
}

void PackStateInfo(StateInfo::ITEM_TYPE type, std::string name, std::string desc,
	StateInfo::RUN_STATE runState, std::string ipStr, std::string cpuStr, std::string memStr,
	std::string gpuStr)
{
	//stateInfoInterval为0时不上传状态到后台
	if (g_AppConfig.userCfg.stateInfoInterval <= 0)
		return;
	std::lock_guard<std::mutex> lck(g_mtxStateInfo);
	if (type != StateInfo_RUN_STATE_RUN_NORMAL)
		g_StateInfoType = type;
	int nItemSize = g_StateInfo.items_size();
	for (int nItemIdx = 0; nItemIdx < nItemSize; nItemIdx++)
	{
		g_StateInfo.items(nItemIdx);
		StateInfo::Item *item = g_StateInfo.mutable_items(nItemIdx);
		if (item->name() == name)
		{
			item->set_type(type);
			item->set_name(name);
			item->set_desc(desc);
			item->set_state(runState);
			item->set_ipaddr(ipStr);
			item->set_cpu(cpuStr);
			item->set_memory(memStr);
			item->set_gpu(gpuStr);
			return;
		}
	}
	StateInfo::Item *item = g_StateInfo.add_items();
	item->set_type(type);
	item->set_name(name);
	item->set_desc(desc);
	item->set_state(runState);
	item->set_ipaddr(ipStr);
	item->set_cpu(cpuStr);
	item->set_memory(memStr);
	item->set_gpu(gpuStr);
}
////写文件
//void WriteToLocalFile(std::string path, const char *buffer, int size)
//{
//	std::ofstream ofs{path.c_str(), std::ios_base::binary | std::ios_base::trunc };
//
//	if (ofs)
//	{
//		ofs.write(buffer, size);
//		ofs.close();
//
//	}
//}
////读文件
//void ReadFromLocalFile(std::string path, string &buffer)
//{
//	std::ifstream ifs;
//	ifs.open(path.c_str(), std::ios_base::binary | std::ios_base::in);
//	ifs.seekg(0, ifs.end);
//	int length = ifs.tellg();
//	ifs.seekg(0, ifs.beg);
//	char *ss = new char[length];
//	if (ifs.is_open())
//	{
//		while (!ifs.eof())
//		{
//			ifs.read(ss, length);
//		}
//		ifs.close();
//	}
//	string tmp(ss);
//	buffer = tmp;
//	delete[] ss;
//	ss = nullptr;
//}
/*
*获取路径下所有文件夹和文件名字
*ret 文件夹名字
*name 文件名字
*/
void GetFilesNames(const std::string& rootPath, std::vector<string> &name)
{
	namespace fs = boost::filesystem;
	fs::path fullpath(rootPath);
	fs::recursive_directory_iterator end_iter;
	for (fs::recursive_directory_iterator iter(fullpath);iter != end_iter;iter++)
	{
		try
		{
			if (fs::is_directory(*iter))
			{
				//ret.push_back(iter->path().string());
			}
			else
			{
				string file = iter->path().string();
				//ret.push_back(iter->path().string());
				fs::path filePath(file);
				name.push_back(filePath.stem().string());
			}
		}
		catch (const std::exception & ex)
		{
			console->error("获取文件名称失败：{}",ex.what());
			continue;
		}
	}
}

bool RemoveLocalFile(const string& rootPath)
{
	if (rootPath.find_last_of("\\")==rootPath.length()-1)
	{
		console->error("删除 没有文件名：{}",rootPath);
		return false;
	}
	if (boost::filesystem::exists(rootPath))
	{
		try
		{
			bool result = remove(rootPath);
			return result;
		}
		catch (std::exception &e)
		{
			console->error("删除文件错误 msg：{}", e.what());
			return false;
		}
		catch (...)
		{
			console->error("删除文件错误");
			return false;
		}
	}
	else
	{
		console->error("删除 找不到文件：{}", rootPath);
		return true;
	}
}

bool GetDiskSpace(std::string path, int& nTotalG, int&nFreeG)
{
	nTotalG = 0;
	nFreeG = 0;
	try
	{
		boost::filesystem::space_info si = boost::filesystem::space(path);
		nTotalG = si.capacity / 1024 / 1024 / 1024;
		nFreeG = si.free / 1024 / 1024 / 1024;
		return true;
	}
	catch (std::exception& e)
	{
		console->error("GetDiskSpace {} error:{}", path, e.what());
	}
	catch (...)
	{
		console->error("GetDiskSpace {} error", path);
	}
	return false;
}

void WriteProtoToLocalFile(const string &path,const TaskInfo &oa)
{
	string str_pb = path;
	FILE *pOut = nullptr;
	errno_t er = fopen_s(&pOut, str_pb.c_str(), "wb+");
	if (pOut)
	{
		//fseek(pOut, 0, SEEK_END);
		//先写4个字节的长度
		int nByteSize = oa.ByteSizeLong();
		//fwrite(&nByteSize, 1, sizeof(int), pOut);
		//再写数据
		char *pszBuf = new char[nByteSize];
		oa.SerializePartialToArray(pszBuf, nByteSize);
		fwrite(pszBuf, 1, nByteSize, pOut);
		fclose(pOut);
		pOut = nullptr;

		delete pszBuf;
		pszBuf = nullptr;
	}
	else
	{
		console->error("写入文件失败 {} err={}", str_pb, er);
	}
}

void ReadLocalFileToProto(const string &path, TaskInfo &oa)
{
	string str_pb = path;
	FILE *fp;
	errno_t er = fopen_s(&fp, str_pb.c_str(), "rb");
	if (fp)
	{
		//求得文件的大小
		fseek(fp, 0, SEEK_END);
		int size = ftell(fp);
		rewind(fp);

		//申请一块能装下整个文件的空间
		char *ar = new char[size];
		//读文件，每次读一个，共读size次
		fread(ar, 1, size, fp);

		oa.ParseFromArray(ar, size);
		fclose(fp);
		fp = nullptr;
		delete ar;
		ar = nullptr;
	}
	else
	{
		console->error("读取文件失败 {} err={}", str_pb, er);
	}


}

std::string SpliceFileName(std::string strCamIp, std::string evtName)
{
	return GetSysTimeString() + "_" +
		strCamIp + "_" +
		evtName;
}

void WriteProtoToLocalFile(const string &path, const FtpPath &oa)
{
	string str_pb = path;
	FILE *pOut = nullptr;
	errno_t er = fopen_s(&pOut, str_pb.c_str(), "wb+");
	if (pOut)
	{
		//fseek(pOut, 0, SEEK_END);
		//先写4个字节的长度
		int nByteSize = oa.ByteSizeLong();
		//fwrite(&nByteSize, 1, sizeof(int), pOut);
		//再写数据
		char *pszBuf = new char[nByteSize];
		oa.SerializePartialToArray(pszBuf, nByteSize);
		fwrite(pszBuf, 1, nByteSize, pOut);
		fclose(pOut);
		pOut = nullptr;

		delete pszBuf;
		pszBuf = nullptr;
	}
	else
	{
		console->error("写入文件失败 {} err={}", str_pb, er);
	}
}

bool ReadLocalFileToProto(const string &path, FtpPath &oa)
{
	string str_pb = path;
	FILE *fp;
	errno_t er = fopen_s(&fp, str_pb.c_str(), "rb");
	if (fp)
	{
		//求得文件的大小
		fseek(fp, 0, SEEK_END);
		int size = ftell(fp);
		rewind(fp);

		//申请一块能装下整个文件的空间
		char *ar = new char[size];
		//读文件，每次读一个，共读size次
		fread(ar, 1, size, fp);

		oa.ParseFromArray(ar, size);
		fclose(fp);
		fp = nullptr;
		delete ar;
		ar = nullptr;
		return true;
	}
	else
	{
		console->error("读取文件失败 {} err={}", str_pb, er);
	}
	return false;
}

void WriteProtoToLocalFile(const string &path, const OCRAny &oa)
{
	string str_pb = path;
	FILE *pOut = nullptr;
	errno_t er = fopen_s(&pOut, str_pb.c_str(), "wb+");
	if (pOut)
	{
		//fseek(pOut, 0, SEEK_END);
		//先写4个字节的长度
		int nByteSize = oa.ByteSizeLong();
		//fwrite(&nByteSize, 1, sizeof(int), pOut);
		//再写数据
		char *pszBuf = new char[nByteSize];
		oa.SerializePartialToArray(pszBuf, nByteSize);
		fwrite(pszBuf, 1, nByteSize, pOut);
		fclose(pOut);
		pOut = nullptr;

		delete pszBuf;
		pszBuf = nullptr;
	}
	else
	{
		console->error("写入proto文件失败 {} err={}", str_pb, er);
	}

}

void ReadLocalFileToProto(const string &path, OCRAny &oa)
{
	string str_pb = path;
	FILE *fp;
	errno_t er = fopen_s(&fp, str_pb.c_str(), "rb");
	if (fp)
	{
		//求得文件的大小
		fseek(fp, 0, SEEK_END);
		int size = ftell(fp);
		rewind(fp);

		//申请一块能装下整个文件的空间
		char *ar = new char[size];
		//读文件，每次读一个，共读size次
		fread(ar, 1, size, fp);

		oa.ParseFromArray(ar, size);
		fclose(fp);
		fp = nullptr;
		delete ar;
		ar = nullptr;
	}
	else
	{
		console->error("读取proto文件失败 {} err={}", str_pb, er);
	}


}

/**
* \brief 八进制字符串到BYTE
* \param pszOct 源字符串
* \param nLen 长度
* \return 转换后的值
*/
unsigned char Oct2Byte(const char* pszOct, int nLen)
{
	unsigned char bVal = 0;
	for (int i = 0; i < nLen; ++i)
	{
		bVal = bVal * 8 + pszOct[i] - '0';
	}
	return bVal;
}

bool GetTruckNumFromMap(string &pmn)
{
	/*if (g_mapTruckNum.find(pmn) != g_mapTruckNum.end())
	{
		pmn = g_mapTruckNum.at(pmn);
		return true;
	}
	return false;*/
	std::lock_guard<std::mutex> lck(g_vecTruckNumMutex);
	for (auto &c : g_vecTruckNum)
	{
		if (pmn.length() > 0)
		{
			if (pmn[0] == c.src)
			{
				pmn[0] = c.dst;
			}
		}
		if (pmn.length() > 1)
		{
			if (pmn[1] == c.src)
			{
				pmn[1] = c.dst;
			}
		}
	}
	return true;
}
/**
* \brief 转换protobuf中DebugString的八进制字符串到中文
* \param pszSrc 源字符串
* \param nLen 源字符串长度
* \return 转换后的字符串
*/
std::string ConvertChs(const char *pszSrc, int nLen)
{
	char *pszDst = new char[nLen + 1];
	memset(pszDst, 0, nLen + 1);
	int nDstPos = 0;
	for (int nSrcPos = 0; nSrcPos < nLen;)
	{
		if (nSrcPos + 8 <= nLen &&
			IsOctStr(pszSrc + nSrcPos, 4) &&
			IsOctStr(pszSrc + nSrcPos + 4, 4))
		{
			BYTE bVal = Oct2Byte(pszSrc + nSrcPos + 1, 3);
			pszDst[nDstPos++] = bVal;
			nSrcPos += 4;
			bVal = Oct2Byte(pszSrc + nSrcPos + 1, 3);
			pszDst[nDstPos++] = bVal;
			nSrcPos += 4;
		}
		else
		{
			pszDst[nDstPos++] = pszSrc[nSrcPos++];
		}
	}
	std::string ret(pszDst);
	delete[]pszDst;
	return ret;
}
/**
* \brief 转换八进制字符串到中文
* \param str 源字符串
* \return 转换后的字符串
*/
std::string ConvertChs(const std::string& str)
{
	return ConvertChs(str.c_str(), str.length());
}

void WriteStringToTxt(const string &path, const string &src)
{
	string str_pb = path;
	FILE *pOut = nullptr;
	errno_t er = fopen_s(&pOut, str_pb.c_str(), "at+");
	if (pOut)
	{
		//fseek(pOut, 0, SEEK_END);
		//先写4个字节的长度
		int nByteSize = src.length();
		//fwrite(&nByteSize, 1, sizeof(int), pOut);
		//再写数据
		fwrite(src.c_str(), 1, nByteSize, pOut);
		fclose(pOut);
		pOut = nullptr;

	}
	else
	{
		console->error("写入txt文件失败 {} err={}", str_pb, er);
	}
}

size_t recursive_dir(const string& dir, int level, set<string>*pset)
{
	size_t nCount = 0;
	boost::filesystem::directory_iterator end;
	for (boost::filesystem::directory_iterator pos(dir); pos != end; ++pos)
	{
		boost::filesystem::path cur = *pos;
		if (boost::filesystem::is_directory(*pos))
		{
			pset->insert(pos->path().string());
			nCount++;
		}
	}
	return nCount;
}
void MakeEnoughSpace()
{
	std::thread t = std::thread{
		[]()
		{
			static bool bRun = false;
			if (bRun)
			{
				return;
			}
			bRun = true;
			try
			{
				int nMinFreeRate = g_AppConfig.monitorCfg.diskCfg;
				string save_path = g_AppConfig.userCfg.savePath;
				console->debug("MakeEnoughSpace path:[{}]", save_path);
				while (true)
				{
					boost::filesystem::space_info si = boost::filesystem::space(save_path);
					//磁盘可用空间
					int nAvailableM = si.available / 1024 / 1024;
					int nMinFreeM = si.capacity * nMinFreeRate / 100 / 1024 / 1024;
					if (nAvailableM <= nMinFreeM)
					{
						//磁盘空间不足，删除最久远日期的文件
						set<string> setDir;
						size_t st_dir = recursive_dir(save_path, 0, &setDir);
						if (st_dir <= 1)
						{
							//没有可操作的文件夹
							stringstream ss;
							ss << "disk space(available="
								<< nAvailableM << " M,Min="
								<< nMinFreeM
								<< " M)";
							console->info("{}", ss.str());
							g_SpaceIsEnough = false;
							break;
						}
						//取出最时间最久远的文件夹
						string del_path = *(setDir.begin());
						if (boost::filesystem::exists(del_path))
						{
							//删除该目录下所有文件(夹)
							long long freeSize_t = boost::filesystem::remove_all(del_path) / 1024 / 1024;
							console->warn("REMOVE DIR:{}  space:{}", del_path, freeSize_t);
							bool isEnough = freeSize_t >= nMinFreeM - nAvailableM;
							g_SpaceIsEnough = isEnough;
							if (isEnough)
								break;
						}
						else
						{
							//磁盘空间不足，并且无可删除的文件(夹)
							stringstream ss;
							ss << "disk space(available="
								<< nAvailableM << " M,Min="
								<< nMinFreeM
								<< " M)),no file(dir) to delete";
							console->info("{}", ss.str());
							g_SpaceIsEnough = false;
							break;
						}
					}
					else
					{
						console->debug("disk available {} > {}({}%)", nAvailableM, nMinFreeM, nMinFreeRate);
						g_SpaceIsEnough = true;
						break;
					}
				}
			}
			catch (exception& e)
			{
				console->error("delete disk file exception:{}", e.what());
				g_SpaceIsEnough = false;
			}
			catch (...)
			{
				console->error("delete disk file exception:...");
				g_SpaceIsEnough = false;
			}
			bRun = false;
		}
	};
	t.detach();
}

//相邻2个字节交换位置（高低位相反）
void SwapByte(unsigned char *pBuf, int nLen)
{
	unsigned char ucTemp;
	for (int nPos = 0; nPos < nLen;)
	{
		ucTemp = pBuf[nPos];
		pBuf[nPos] = pBuf[nPos + 1];
		pBuf[nPos + 1] = ucTemp;
		nPos += 2;
	}
}
bool IsAllNumber(string& str )
{
	bool bAllNumber = true;
	for (auto c : str)
	{
		if (!isNumber(c))
		{
			bAllNumber = false;
			break;
		}
	}
	return bAllNumber;
}
void GetTruckNo(string& truck_no)
{
	//根据内集卡车顶号查找车牌号
	if (IsAllNumber(truck_no))
	{
		//全部是数字，认为是内集卡车顶号，并且补齐为T+3位数字
		if (truck_no.length() <= 3)
		{
			char szTruckNo[5] = { "T000" };
			memcpy(szTruckNo + 4 - truck_no.length(), truck_no.c_str(), truck_no.length());
			truck_no = szTruckNo;
		}
		else
			truck_no = "T" + truck_no;
	}
	else
	{
		//外集卡车顶号，补齐为3位
		if (truck_no.length() <= 3)
		{
			char szTruckNo[4] = { "000" };
			memcpy(szTruckNo + 3 - truck_no.length(), truck_no.c_str(), truck_no.length());
			truck_no = szTruckNo;
		}
	}
}
bool GetPlateNumByTruckNo(string& truck_no, string& plate_num)
{
	//根据内集卡车顶号查找车牌号
	bool bAllNumber = true;
	for (auto c:truck_no)
	{
		if (!isNumber(c))
		{
			bAllNumber = false;
			break;
		}
	}
	if (bAllNumber)
	{
		//全部是数字，认为是内集卡车顶号，并且补齐为T+3位数字
		if (truck_no.length() <= 3)
		{
			char szTruckNo[5] = { "T000" };
			memcpy(szTruckNo + 4 - truck_no.length(), truck_no.c_str(), truck_no.length());
			truck_no = szTruckNo;
		}
		else
			truck_no = "T" + truck_no;
	}
	else
	{
		//外集卡车顶号，补齐为3位
		if (truck_no.length() <= 3)
		{
			char szTruckNo[4] = { "000" };
			memcpy(szTruckNo + 3 - truck_no.length(), truck_no.c_str(), truck_no.length());
			truck_no = szTruckNo;
		}
	}
	for (auto&plate : g_AppConfig.arrPlateMatch)
	{
		if (plate.truck_num == truck_no)
		{
			plate_num = plate.plate_num;
			return true;
		}
	}
	return false;
}

bool GetTruckNoByPlateNum(string& plate_num, string& truck_no)
{
	string strRight5 = plate_num.substr(plate_num.length() - 5, 5);//车牌中后5位
	for (auto&plate : g_AppConfig.arrPlateMatch)
	{
		if (plate.plate_num.find(strRight5) != -1)
		{
			truck_no = plate.truck_num;
			plate_num = plate.plate_num;
			return true;
		}
	}
	return false;
}

string g_SavePath;
SyncQueue<shared_ptr<tMainLog>> g_queueLog;
void AddMainLog(const string& info)
{
	shared_ptr<tMainLog> pLog = std::make_shared<tMainLog>();
	pLog->info = info;
	g_queueLog.add(pLog);
}

double calc_image_pixel_avg(cv::Mat& image)
{
	int rows = image.rows;
	int cols = image.cols;
	int channels = image.channels();

	int sumPixel = 0;
	int totalPixels = rows * cols * channels;			// 总像素数

	for (int r = 0; r < rows; ++r) {
		const uchar* ptr = image.ptr<uchar>(r);			// 当前行

		for (int c = 0; c < cols * channels; c += channels) {
			for (int ch = 0; ch < channels; ++ch) {
				sumPixel += (int)ptr[c + ch];
			}
		}
	}

	return sumPixel * 1.0 / totalPixels;
}
