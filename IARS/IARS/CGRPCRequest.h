#pragma once
#include <string>
#include <vector>
#include "Console.h"
#include "ocrs.pb.h"
#include "Utils.h"
#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
// #include "PLCHttpComm.h"

using namespace ocrs::pb;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
class CGRPCRequest
{
private:
	CGRPCRequest();
public:
	~CGRPCRequest();
	static CGRPCRequest *GetInstance();
	public:
		bool Start();
		bool GetOcrResult(const std::vector<shared_ptr<GRpc_RImage>> &imgs, GRpc_RepeadResult &result);
		//获取连接的IP,连接失败则为空
		std::string GetConnectIpStr();
		//错误详细信息
		std::string GetErrorDetail();
		// 最近一次 gRPC 调用是否因 deadline（CRS_GRPC_WaitTime）超时失败；用于 StatusCode=2
		bool IsLastErrorDeadlineExceeded() const {
		    return m_lastDeadlineExceeded; }
		bool IsStateOk() { return m_nState == StateInfo_RUN_STATE_RUN_NORMAL; }
	private:
		// 函数调用    一次发送一次返回
		bool GetFeature(const GRpc_RImage& img, GRpc_RepeadResult &result);
		// 应答流式  一张图片发送单个多个任务类型  多次返回每个任务类型的结果
		bool ListFeatures(const GRpc_RImage& img, std::vector<GRpcResult> &results);
		// 请求流式 返回识别结果信息  多次发送图片的信息 一次返回所有数据
		bool RecordRoute(const std::vector<shared_ptr<GRpc_RImage>>& imgs, GRpc_RepeadResult &result);
		// 双向流式 一直发送图片信息  一直返回图片结果
		bool RouteChat(const std::vector<shared_ptr<GRpc_RImage>> &imgs, std::vector<GRpc_RepeadResult> &results);
		// 身份验证
		bool Identity();
		//是否请求结果成功
		bool RequestStatus(const Status &status);
		//连接CRS
		bool ConnectCRS(string ipaddr, string port);
		//设置GRPC通信超时时间
		void SetTimeSpec(ClientContext& context);
		//是否连接
		bool IsConnect();
		//重连 先尝试连接主识别服务器，如果失败在尝试连接备设别服务器
		bool ReConnect();
	private:
		std::unique_ptr<ocrs::pb::main::Stub> stub_;
		std::string m_connectIpStr;//当前连接的IP
		std::string m_connectPort;//当前连接端口
		std::string m_errorDetail;
		// 最近一次 RequestStatus 失败是否为 DEADLINE_EXCEEDED（发图后超时无回包）
		bool m_lastDeadlineExceeded = false;

		// 状态 ：0 无 / 1 正常 / 2 报警 / 3 故障
		std::atomic<int> m_nState;
		std::atomic<bool> m_exitFlag;
};

class TokenAuthenticator : public grpc::MetadataCredentialsPlugin
{
public:

	TokenAuthenticator(const std::string& token) : token_(token) {}

	grpc::Status GetMetadata(
		grpc::string_ref service_url
		, grpc::string_ref method_name
		, const grpc::AuthContext& channel_auth_context
		, std::multimap<std::string, std::string> * metadata) override
	{
		metadata->insert(std::make_pair("authorization-token", token_));
		return grpc::Status::OK;
	}

private:

	std::string token_;
};
