#pragma once
/*
FTP命令


命令 	            描述
ABOR	            中断数据连接程序
ACCT <account>	    系统特权帐号
ALLO <bytes> 	    为服务器上的文件存储器分配字节
APPE <filename>	    添加文件到服务器同名文件
CDUP <dir path>	    改变服务器上的父目录
CWD <dir path>	    改变服务器上的工作目录
DELE <filename>	    删除服务器上的指定文件
HELP <command>	    返回指定命令信息
LIST <name>	        如果是文件名列出文件信息，如果是目录则列出文件列表
MODE <mode>	        传输模式（S=流模式，B=块模式，C=压缩模式）
MKD <directory>	    在服务器上建立指定目录
NLST <directory>	列出指定目录内容
NOOP	            无动作，除了来自服务器上的承认
PASS <password>	    系统登录密码
PASV	            请求服务器等待数据连接
PORT <address>	    IP 地址和两字节的端口 ID
PWD	                显示当前工作目录
QUIT	            从 FTP 服务器上退出登录
REIN	            重新初始化登录状态连接
REST <offset>	    由特定偏移量重启文件传递
RETR <filename>	    从服务器上找回（复制）文件
RMD <directory>	    在服务器上删除指定目录
RNFR <old path>	    对旧路径重命名
RNTO <new path>	    对新路径重命名
SITE <params>	    由服务器提供的站点特殊参数
SMNT <pathname>	    挂载指定文件结构
STAT <directory>	在当前程序或目录上返回信息
STOR <filename>	    储存（复制）文件到服务器上
STOU <filename>	    储存文件到服务器名称上
STRU <type>	        数据结构（F=文件，R=记录，P=页面）
SYST	            返回服务器使用的操作系统
TYPE <data type>	数据类型（A=ASCII，E=EBCDIC，I=binary）
USER <username> 	系统登录的用户名


FTP响应码

响应代码 	解释说明
110	新文件指示器上的重启标记
120	服务器准备就绪的时间（分钟数）
125	打开数据连接，开始传输
150	打开连接
200	成功
202	命令没有执行
211	系统状态回复
212	目录状态回复
213	文件状态回复
214	帮助信息回复
215	系统类型回复
220	服务就绪
221	退出网络
225	打开数据连接
226	结束数据连接
227	进入被动模式（IP 地址、ID 端口）
230	登录因特网
250	文件行为完成
257	路径名建立
331	要求密码
332	要求帐号
350	文件行为暂停
421	服务关闭
425	无法打开数据连接
426	结束连接
450	文件不可用
451	遇到本地错误
452	磁盘空间不足
500	无效命令
501	错误参数
502	命令没有执行
503	错误指令序列
504	无效命令参数
530	未登录网络
532	存储文件需要帐号
550	文件不可用
551	不知道的页类型
552	超过存储分配
553	文件名不允许
*/
#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
using namespace std;

class CFtpClient : public boost::noncopyable
{
public:
	CFtpClient();
	~CFtpClient();

	static CFtpClient & getInstence()
	{
		static CFtpClient ftp;
		return ftp;
	}

	void SetFtpParam(string host, string port, string user, string passwd);
	bool UploadFile(string src, string dst);
	bool UploadData(const char*pszData, size_t bySize, string dst);
	bool DownloadFile(string src, string dst);
	string GetErrorMsg() {
		//boost::mutex::scoped_lock lock(iomutex);
		return error_msg;
	}

	string GetHost() {
		//boost::mutex::scoped_lock lock(iomutex);
		return host_;
	}
	bool IsConnect();
private:
	//得到文件长度
	long GetFileLength(const string& stringLine);
	//得到ftp返回命令的数值
	int GetRetCode(const string& stringLine);
	// 发送命令到FTP服务器
	void SendCommand(boost::asio::ip::tcp::socket &cmd_socket, string strCommand, string& strResult);
	bool ParseHostPort(string s, string& host, string& port);

	void UploadThread();
	bool UploadWork(string src, string dst);
	void DownloadThread();
	bool DownloadWork(string src, string dst);

	//逐层创建多级目录
	bool MakeMultiDir(boost::asio::ip::tcp::socket &cmd_socket, string s);
	bool IsLogon(boost::asio::ip::tcp::socket &cmd_socket);
	bool Connect(boost::asio::ip::tcp::socket &cmd_socket);
	bool Logon(boost::asio::ip::tcp::socket &cmd_socket);
	string GetPath(string s);
	string GetName(string s);
	void GetPaths(string s, vector<string> &v);
private:
	boost::asio::io_service io_service_;
	boost::mutex  connect_mutex;
	boost::mutex  download_mutex;
	boost::condition download_condition;
	std::deque<pair<string, string>> download_queue;

	boost::mutex  upload_mutex;
	boost::condition upload_condition;
	std::deque<pair<string, string>> upload_queue;

	bool running_;
	std::string user_;
	std::string passwd_;
	std::string host_;
	std::string port_;
	string error_msg;
};

