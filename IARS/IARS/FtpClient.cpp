#include "FtpClient.h"
#include "Utils.h"
#include <boost/thread/thread.hpp>
#include <fstream>
#include "Console.h"
CFtpClient::CFtpClient()
	: running_(true)
	, error_msg("error")
{
	boost::thread_group tg;
	tg.create_thread(boost::bind(&CFtpClient::UploadThread, this));
	tg.create_thread(boost::bind(&CFtpClient::DownloadThread, this));
}


CFtpClient::~CFtpClient()
{
	running_ = false;
}

void CFtpClient::SetFtpParam(string host, string port, string user, string passwd)
{
	host_ = host;
	port_ = port;
	user_ = user;
	passwd_ = passwd;
}
bool CFtpClient::IsConnect()
{
	try
	{
		boost::asio::ip::tcp::socket cmd_socket(io_service_);
		bool ok = Connect(cmd_socket);
		if (ok)
		{
			cmd_socket.close();
		}
		return ok;
	}
	catch (std::exception& e)
	{
		console->error("FtpClient exception：{}", e.what());
	}
	catch (...)
	{
		console->error("FtpClient exception");
	}
	return false;
}
bool CFtpClient::UploadFile(string src, string dst)
{
	return UploadWork(src, dst);
	pair<string, string> one;
	one.first = src;
	one.second = dst;
	boost::mutex::scoped_lock lock(upload_mutex);
	upload_queue.push_back(one);
	upload_condition.notify_one();
}

bool CFtpClient::DownloadFile(string src, string dst)
{
	return DownloadWork(src, dst);
	pair<string, string> one;
	one.first = src;
	one.second = dst;
	boost::mutex::scoped_lock lock(download_mutex);
	download_queue.push_back(one);
	download_condition.notify_one();
	return true;
}

long CFtpClient::GetFileLength(const string& stringLine)
{
	long ldFileLength = 0;
	size_t nBeginPos = stringLine.find_first_of(" ");
	size_t nEndPos = stringLine.find_first_of("\r\n");
	std::stringstream stream;
	string strLength = stringLine.substr(nBeginPos, nEndPos - nBeginPos + 1);
	stream << strLength;
	stream >> ldFileLength;
	return ldFileLength;
}

int CFtpClient::GetRetCode(const string& stringLine)
{
	if (stringLine.size() < 3)
	{
		return -1;
	}
	size_t nPos = stringLine.find_first_of(" ");
	std::stringstream stream;
	string strCode = stringLine.substr(0, nPos);
	int nRetCode = 0;
	stream << strCode;
	stream >> nRetCode;
	return nRetCode;
}

void CFtpClient::SendCommand(boost::asio::ip::tcp::socket &cmd_socket, string strCommand, string& strResult)
{
	boost::asio::streambuf request;
	boost::asio::streambuf response;
	std::string strLine;
	std::ostream request_stream(&request);
	std::istream response_stream(&response);

	strCommand += ("\r\n");
	request_stream << strCommand;
	boost::asio::write(cmd_socket, request);

	boost::asio::read_until(cmd_socket, response, "\r\n");

	std::getline(response_stream, strResult);
	console->info("[FTP] SendCommand  command:{}   result:{}",strCommand, strResult);
}

bool CFtpClient::ParseHostPort(string pasv, string& host, string& port)
{
	if ((pasv.find('(') == std::string::npos) && (pasv.find(')') == std::string::npos)) {
		return false;
	}
	size_t paramspos = pasv.find('(');
	string params = pasv.substr(paramspos + 1);
	size_t ip1pos = params.find(',');
	string ip1 = params.substr(0, ip1pos);
	size_t ip2pos = params.find(',', ip1pos + 1);
	string ip2 = params.substr(ip1pos + 1, ip2pos - ip1pos - 1);
	size_t ip3pos = params.find(',', ip2pos + 1);
	string ip3 = params.substr(ip2pos + 1, ip3pos - ip2pos - 1);
	size_t ip4pos = params.find(',', ip3pos + 1);
	string ip4 = params.substr(ip3pos + 1, ip4pos - ip3pos - 1);
	size_t port1pos = params.find(',', ip4pos + 1);
	string port1 = params.substr(ip4pos + 1, port1pos - ip4pos - 1);
	size_t port2pos = params.find(')', port1pos + 1);
	string port2 = params.substr(port1pos + 1, port2pos - port1pos - 1);

	host = ip1 + "." + ip2 + "." + ip3 + "." + ip4;
	int portval = atoi(port1.c_str()) * 256 + atoi(port2.c_str());
	port = boost::lexical_cast<std::string>(portval);
	return true;
}

void CFtpClient::UploadThread()
{
	while (running_)
	{
		boost::mutex::scoped_lock lock(upload_mutex);
		upload_condition.wait(lock);
		while (!upload_queue.empty())
		{
			pair<string, string> one = upload_queue.front();
			upload_queue.pop_front();
			UploadWork(one.first, one.second);
		}
	}
}

bool CFtpClient::UploadWork(string src, string dst)
{
	try
	{
		boost::asio::ip::tcp::socket cmd_socket(io_service_);
		if (!Logon(cmd_socket))
		{
			console->error("[FTP] Logon Ftp false ");
			return false;
		}
		string strResult;
		//send PASV
		SendCommand(cmd_socket, "PASV", strResult);
		if (GetRetCode(strResult) >= 500)
		{
			cmd_socket.close();
			stringstream ss;
			ss << "FTP PASV失败:" << strResult;
			console->error(ss.str());
			return false;
		}
		string host, port;
		if (!ParseHostPort(strResult, host, port))
		{
			cmd_socket.close();
			stringstream ss;
			ss << "FTP connect new port 失败:" << strResult;
			console->error(ss.str());
			return false;
		}
		boost::asio::ip::tcp::socket data_socket(io_service_);
		boost::asio::ip::tcp::resolver::query data_query(host, port);
		boost::asio::ip::tcp::resolver resolver(io_service_);

		boost::asio::ip::tcp::resolver::iterator data_endpoint_iterator = resolver.resolve(data_query);
		boost::asio::ip::tcp::resolver::iterator end;
		boost::system::error_code error = boost::asio::error::host_not_found;
		if (error && data_endpoint_iterator != end)
		{
			data_socket.close();
			data_socket.connect(*data_endpoint_iterator++, error);
		}
		if (!data_socket.is_open())
		{
			console->error("[FTP] data_socket.is_open() flase");
			cmd_socket.close();
			stringstream ss;
			ss << "FTP send data connect 失败:" << strResult;
			console->error(ss.str());
			return false;
		}
		vector<string> dst_paths;
		GetPaths(dst, dst_paths);
		string dst_name = GetName(dst);
		string cmd;
		cmd = "TYPE I";
		SendCommand(cmd_socket, cmd, strResult);
		for (const auto &dst_path : dst_paths)
		{
			cmd = "MKD ";
			cmd += dst_path;
			SendCommand(cmd_socket, cmd, strResult);
			cmd = "CWD ";
			cmd += dst_path;
			SendCommand(cmd_socket, cmd, strResult);
		}

		cmd = "STOR ";
		cmd += dst_name;
		SendCommand(cmd_socket, cmd, strResult);
		if (GetRetCode(strResult) >= 500)
		{
			cout << strResult << endl;
			data_socket.close();
			cmd_socket.close();
			stringstream ss;
			ss << "FTP STOR失败:" << strResult;
			console->error(ss.str());
			return false;
		}
		boost::asio::streambuf sbuf;
		std::ifstream file;
		file.open(src, ios_base::binary);

		// Read until EOF, writing data to output as we go.
		while (!file.eof())
		{
			file >> &sbuf;
			boost::asio::write(data_socket, sbuf, boost::asio::transfer_at_least(sbuf.size()), error);
		}
		file.close();
		
		////关闭ftp
		//cmd = "QUIT";
		//SendCommand(cmd_socket, cmd, strResult);
		data_socket.close();
		cmd_socket.close();
		if (error)
		{
			error_msg = error.message();
		}
		return !error.failed();
	}
	catch (system_error& e)
	{
		stringstream ss;
		ss << "文件上传失败:" << e.what();
		console->error(ss.str());
	}
	catch (exception& e)
	{
		stringstream ss;
		ss << "文件上传失败:" << e.what();
		console->error(ss.str());
	}
	return false;
}

void CFtpClient::DownloadThread()
{
	while (running_)
	{
		boost::mutex::scoped_lock lock(download_mutex);
		download_condition.wait(lock);
		while (!download_queue.empty())
		{
			pair<string, string> one = download_queue.front();
			download_queue.pop_front();
			DownloadWork(one.first, one.second);
		}
	}
}
bool CFtpClient::DownloadWork(string src, string dst)
{
	try
	{
		boost::asio::ip::tcp::socket cmd_socket(io_service_);
		if (!Logon(cmd_socket))
		{
			cmd_socket.close();
			return false;
		}
		string strResult;
		string dst_path = GetPath(dst);
		string dst_name = GetName(dst);
		string cmd;
		cmd = "SIZE ";
		cmd += src;
		SendCommand(cmd_socket, cmd, strResult);
		if (GetRetCode(strResult) >= 500)
		{
			cmd_socket.close();
			cout << strResult << endl;
			return false;
		}
		long lSize = GetFileLength(strResult);
		//send PASV
		SendCommand(cmd_socket, "PASV", strResult);
		if (GetRetCode(strResult) >= 500)
		{
			cmd_socket.close();
			cout << strResult << endl;
			return false;
		}

		string host, port;
		if (!ParseHostPort(strResult, host, port))
		{
			cmd_socket.close();
			return false;
		}

		boost::asio::ip::tcp::socket data_socket(io_service_);
		boost::asio::ip::tcp::resolver::query data_query(host, port);
		boost::asio::ip::tcp::resolver resolver(io_service_);

		boost::asio::ip::tcp::resolver::iterator data_endpoint_iterator = resolver.resolve(data_query);
		boost::asio::ip::tcp::resolver::iterator end;
		boost::system::error_code error = boost::asio::error::host_not_found;

		if (error && data_endpoint_iterator != end)
		{
			data_socket.close();
			data_socket.connect(*data_endpoint_iterator++, error);
		}
		if (!data_socket.is_open())
		{
			cout << "Connect " << host << ":" << port << " Failed:" << error.message() << endl;
			cmd_socket.close();
			return false;
		}

		cmd = "TYPE I";
		SendCommand(cmd_socket, cmd, strResult);
		cmd = "RETR ";
		cmd += src;
		SendCommand(cmd_socket, cmd, strResult);
		if (GetRetCode(strResult) >= 500)
		{
			cout << strResult << endl;
			data_socket.close();
			cmd_socket.close();
			return false;
		}
		std::ofstream file;
		file.open(dst, ios_base::binary);
		boost::asio::streambuf sbuf;
		// Read until EOF, writing data to output as we go.
		while (boost::asio::read(data_socket, sbuf,
			boost::asio::transfer_at_least(1024000), error) &&
			(lSize -= sbuf.size()) >= 0)
		{
			file << &sbuf;
		}
		file.close();
		data_socket.close();
		cmd_socket.close();
		cout << "Download " << src << " To " << dst << endl;
		if (error)
		{
			error_msg = error.message();
		}
		return !error.failed();
	}
	catch (system_error& e)
	{
		cout << e.what() << endl;
	}
	catch (exception& e)
	{
		cout << e.what() << endl;
	}
	return false;
}

bool CFtpClient::Connect(boost::asio::ip::tcp::socket& cmd_socket)
{
	boost::mutex::scoped_lock lock(connect_mutex);
	// 我们需要把在命令行参数中指定的服务器转换为TCP上的节点.
	// 完成这项工作需要boost::asio::ip::tcp::resolver对象 
	boost::asio::ip::tcp::resolver resolver(io_service_);

	//一个resolver对象查询一个参数,并将其转换为TCP上节点的列表.
	boost::asio::ip::tcp::resolver::query query(host_, port_);

	//节点列表可以用 boost::asio::ip::tcp::resolver::iterator 来进行迭代. 
	// iterator默认的构造函数生成一个end iterator
	boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	boost::asio::ip::tcp::resolver::iterator end;

	//现在我们建立一个连接的sockert,由于获得节点既有IPv4也有IPv6的.   
	// 所以,我们需要依次尝试他们直到找到一个可以正常工作的.
	// 这步使得我们的程序独立于IP版本
	boost::system::error_code error = boost::asio::error::host_not_found;
	try
	{
		while (error && endpoint_iterator != end)
		{
			cmd_socket.close();
			cmd_socket.connect(*endpoint_iterator++, error);
		}
		//bool bConn = cmd_socket.is_open();
		bool bConn = error.failed();
		if (bConn)
		{
			//MsgInfo msg;
			//msg.set_msg_type(MsgInfo::FTP_ERROR);
			//msg.set_id(g_strModuleId);
			//msg.set_name(g_AppConfig.rrisCfg.moduleName);
			stringstream ss;
			ss << "FTP连接失败:IP=" << host_;
			if (error)
			{
				error_msg = error.message();
				ss << ",error=" << error.message();
			}
			//msg.set_msg_data(ss.str());
			//g_zcPubMsg.PubMsg(msg);
			//console->error(ss.str());
		}
		return !bConn;
	}
	catch (system_error& e)
	{
		//MsgInfo msg;
		//msg.set_msg_type(MsgInfo::FTP_ERROR);
		//msg.set_id(g_strModuleId);
		//msg.set_name(g_AppConfig.rrisCfg.moduleName);
		stringstream ss;
		ss << "FTP连接异常:" << e.what();
		//msg.set_msg_data(ss.str());
		//g_zcPubMsg.PubMsg(msg);
		console->error(ss.str());
		return false;
	}
	catch (exception& e)
	{
		//MsgInfo msg;
		//msg.set_msg_type(MsgInfo::FTP_ERROR);
		//msg.set_id(g_strModuleId);
		//msg.set_name(g_AppConfig.rrisCfg.moduleName);
		stringstream ss;
		ss << "FTP连接异常:" << e.what();
		//msg.set_msg_data(ss.str());
		//g_zcPubMsg.PubMsg(msg);
		console->error(ss.str());
		return false;
	}
	return false;
}

bool CFtpClient::Logon(boost::asio::ip::tcp::socket& cmd_socket)
{
	if (!Connect(cmd_socket))
	{
		console->info("Connect Ftp false ");
		return false;
	}
	string strResult;
	//读取连接后ftp的反馈信息
	boost::asio::streambuf response;
	std::istream response_stream(&response);
	boost::asio::read_until(cmd_socket, response, "\r\n");
	size_t st = response.size();
	/// send USER
	string snd_cmd;
	snd_cmd = "USER ";
	snd_cmd += user_;
	SendCommand(cmd_socket, snd_cmd, strResult);
	if (GetRetCode(strResult) >= 500)
	{
		//MsgInfo msg;
		//msg.set_msg_type(MsgInfo::FTP_ERROR);
		//msg.set_id(g_strModuleId);
		//msg.set_name(g_AppConfig.rrisCfg.moduleName);
		stringstream ss;
		ss << "FTP USER 失败:" << strResult;
		//msg.set_msg_data(ss.str());
		//g_zcPubMsg.PubMsg(msg);
		console->error(ss.str());
		return false;
	}
	/// send PASSWORD
	snd_cmd = "PASS ";
	snd_cmd += passwd_;
	SendCommand(cmd_socket, snd_cmd, strResult);
	if (GetRetCode(strResult) >= 500)
	{
		//MsgInfo msg;
		//msg.set_msg_type(MsgInfo::FTP_ERROR);
		//msg.set_id(g_strModuleId);
		//msg.set_name(g_AppConfig.rrisCfg.moduleName);
		stringstream ss;
		ss << "FTP PASS失败:" << strResult;
		//msg.set_msg_data(ss.str());
		//g_zcPubMsg.PubMsg(msg);
		console->error(ss.str());
		return false;
	}
	//send TYPE I(BINARY)
	SendCommand(cmd_socket, "TYPE I", strResult);
	if (GetRetCode(strResult) >= 500)
	{
		//MsgInfo msg;
		//msg.set_msg_type(MsgInfo::FTP_ERROR);
		//msg.set_id(g_strModuleId);
		//msg.set_name(g_AppConfig.rrisCfg.moduleName);
		stringstream ss;
		ss << "FTP TYPE I失败:" << strResult;
		//msg.set_msg_data(ss.str());
		//g_zcPubMsg.PubMsg(msg);
		console->error(ss.str());
		return false;
	}
	return true;
}

string CFtpClient::GetPath(string s)
{
	size_t pos = string::npos;
	if ((pos = s.find_last_of('/')) != string::npos)
	{
		return s.substr(0, pos + 1);
	}
	else if ((pos = s.find_last_of('\\')) != string::npos)
	{
		return s.substr(0, pos + 1);
	}
	else
	{
		return s;
	}
}

void CFtpClient::GetPaths(string s, vector<string> &v)
{
	size_t pos = string::npos;
	if ((pos = s.find_last_of('/')) != string::npos)
	{
		SepString(s.substr(0, pos + 1), v, '/');
	}
	else if ((pos = s.find_last_of('\\')) != string::npos)
	{
		SepString(s.substr(0, pos + 1), v, '\\');
	}
	else
	{
		v.push_back(s);
	}
}

string CFtpClient::GetName(string s)
{
	size_t pos = string::npos;
	if ((pos = s.find_last_of('/')) != string::npos)
	{
		return s.substr(pos + 1);
	}
	else if ((pos = s.find_last_of('\\')) != string::npos)
	{
		return s.substr(pos + 1);
	}
	else
		return s;
}
