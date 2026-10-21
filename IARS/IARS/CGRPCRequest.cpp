#include "CGRPCRequest.h"
#include "AppConfig.h"
const char CA_CRT[] = R"(
-----BEGIN CERTIFICATE-----
MIIC6zCCAdMCFDxWARBlkfsjrgiy460cIJSvNlBrMA0GCSqGSIb3DQEBBQUAMDIx
MDAuBgNVBAoMJ1RMUyBQcm9qZWN0IERvZGd5IENlcnRpZmljYXRlIEF1dGhvcml0
eTAeFw0yMTA2MTcwODI0MDlaFw0zNTAyMjQwODI0MDlaMDIxMDAuBgNVBAoMJ1RM
UyBQcm9qZWN0IERvZGd5IENlcnRpZmljYXRlIEF1dGhvcml0eTCCASIwDQYJKoZI
hvcNAQEBBQADggEPADCCAQoCggEBAOSTYoB3YoX+oWUWr6mKEs5K4XCkcjM4NOo7
KmBVjSEfhOBLGJMc/Fv5xHeVKKRinZpUiHiVxmxfiF6legp9bKj/dq0YsAC6FMNl
SbUKKpqR9ia0wId9yYSalu+hQ3ZcD7rGyPB+YEjYXkCPBENfyCoFqY0q0O7q7UYP
a22wMXDyrY0sutc3l73jrpzUn9jBBR/BjM/BYL2PasxC5AhnX41wpu0OSbF7hbS/
1U/DI0zCwGdMU4s5hlNY5mXvR/mtO1PtM+oz/Cu6WG4s2mLB2WVXM32mBPmbArUl
PUye5VXPGNu6nLFXA4LmD9+wG9fLbxKiuGUAfh/FVsGLvI2vKBECAwEAATANBgkq
hkiG9w0BAQUFAAOCAQEA4qyCawTLT015JB5iQMCHW6JBCcTi00J09hdmg1Q2IZKv
fH1T96CXjJU/TJJ4hXY4q4lo7NvvUd6lEYPA+3lAgEuOe+del/mDJOSNrXVLGasi
jbP4MTMZ70NUMNhZvEtRIC+OAaDdI7rseqBsfJv2NG9u705cEQDf4CnAcH3yxPNZ
oCOUR1sLh1ObCTX0TPNqg9lTSPT6RZoY8gPsB9Ngf4BnOAS46gdCMfsg70NOC5+g
XVysKuMRnWpIcRx9KA2HZPdQjnGVDslbr7TSG6Pe4i90FUn4wT3bFM8eOlV3BpwX
Gv1fehmVd6x2VTzaHGzl/TsGdGBIu+XqjUrHguSxnQ==
-----END CERTIFICATE-----
)";
const char CLIENT_CRT[] = R"(
-----BEGIN CERTIFICATE-----
MIIC/jCCAeYCFEY5gLU5dqWZP+IWHW9485ufIsHBMA0GCSqGSIb3DQEBBQUAMDIx
MDAuBgNVBAoMJ1RMUyBQcm9qZWN0IERvZGd5IENlcnRpZmljYXRlIEF1dGhvcml0
eTAeFw0yMTA2MTcwODI0MDlaFw0zNTAyMjQwODI0MDlaMEUxJzAlBgNVBAoMHlRM
UyBQcm9qZWN0IERldmljZSBDZXJ0aWZpY2F0ZTEaMBgGA1UEAwwRd3d3Lm9jcnNl
cnZlci5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCfZqMONOqx
BKBK8SbQlRz61ADxn0h4zuurRr9hX7mrlwLpFAACYtDaECai1vG/kDrMiEJot9jH
pR1HpR3SKz+S0kQaMxkuzWOIQEV+kj87hJcVbluq/F033huH6fB9YV5eC5r20cYB
WLllE9lXVZnuwwz/POso9GT7zG64UHTUlGat2kwLJHSGzjeI6IT60I2R6KCucwOc
cdBn/uScAYRxQKLv6h2bwgdkMwjXS7kkofXOZuuI4ybTVhD5nD6EZMB2ut5vo72i
7mmIE9hA09PnK8MtvQD4KW84jK64wClyhd9xff0CM1kZHqReYYVx922OWimaJ5ut
ZacPlGou8mPFAgMBAAEwDQYJKoZIhvcNAQEFBQADggEBAMFSgtx910QNqlallAqA
9eUDbedVYwb0FeXLI8U9+2pfTGahlk9UGuW9NSDGNb6b+hzhulSYBYgKAx06b5vX
EMYiNz2QSeFvdZt/tN16KTvJNfqigPFMBoPFP/XbD+HlpOVA15ddz1A5VaVjT5Sr
zHTgDXXGRfdo3ZJu1ZgFaFuYIlURxfj4QsCuX+jvT59h/O57iWAGVHtTfjFMYFcV
VMGHeXkD7vMvve5nEhz+QDZBiK2h1IRPsSdO91mLog0Qmh4IEFcoHx4P53QK0fox
zayqxA/Ev6rkcHA92i1V/spcxQW1YzvheCuS6yYFyvo6A99NLi0iMGDGMwAEqrjc
lEE=
-----END CERTIFICATE-----
)";
const char CLIENT_KEY[] = R"(
-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEAn2ajDjTqsQSgSvEm0JUc+tQA8Z9IeM7rq0a/YV+5q5cC6RQA
AmLQ2hAmotbxv5A6zIhCaLfYx6UdR6Ud0is/ktJEGjMZLs1jiEBFfpI/O4SXFW5b
qvxdN94bh+nwfWFeXgua9tHGAVi5ZRPZV1WZ7sMM/zzrKPRk+8xuuFB01JRmrdpM
CyR0hs43iOiE+tCNkeigrnMDnHHQZ/7knAGEcUCi7+odm8IHZDMI10u5JKH1zmbr
iOMm01YQ+Zw+hGTAdrreb6O9ou5piBPYQNPT5yvDLb0A+ClvOIyuuMApcoXfcX39
AjNZGR6kXmGFcfdtjlopmiebrWWnD5RqLvJjxQIDAQABAoIBAFqZueAfT64h1QID
o2428uvWiFrNkCGqauVFJw4vnJbdFHhqN78jb1Yii/NOfMXHMsSCRkzDAmInhUbt
oM+OaWoIPVEu7gdeEk6lSVqxwWMQ79bsjQnYuxtAESHlZB2/tCV8Ly5O+V5WkbnT
2Fr9eqrtDnwcogT7NFKyVdRJVr5kXT+g8hSQD0s/CXlNUlV7TTp+Lg8dacTjk2VA
SKml4RYpJGL6C8SXaJub6EThWy6iQy5JSJj88JO7jGVTIV6eTJ+aM55L6H9xbTX+
zFPRbIYs0BGLnHsSO0/wFt5MUM8QsTqMBTBgD9Q+NZEzEp/PjN0hJnU6OtY/cR42
3/NDOhkCgYEA03cnE7fVu8X6D+xGp29yYzCq7f+CNhMROLyULDIDqfuiy8s9r2nl
8dlbAUqcuyZUahsawGsIkVITRsITngbDQweN/yWRruNKxtDkAuPX9bW4bymOUCsb
xKc7E7nTox+PBSX601P54kqi47+gF5Ftn9v2bt5/kVEmKXw86OQWOB8CgYEAwPiA
tZKrvCAiUUG7OjUbX7Yz5N7/JYQeFB/k2FEKcE6JvdL/fHUpJNjl4qAJ/0WPhddc
czwOAcT3vdOjd1ADeCSk96RZyYGDMWfycjrz+dnNrBPvsoAsQotpnMQn80jg/wbZ
AH94IECfEIYQspiKfHIXLDyCv/upA9a4PbGYd5sCgYBE33zxtiE9vBha0UidxfWK
itJRG7PsKr+kK/9SeaOp3FREbdiUZjhHUdSLEiMD7VL3EAbhfrrWodW3aQR8ppU4
IqDD5heRz2OOZ4Cr1EuFMn90bSN1xuSPYywhm5drfJ7XPWx5IdO0OTzeUo4wvuPm
efzEVWFVEgoM3vLzoz39XwKBgDEKNjNurcQmanI6ASm+u1Esja2ZWkAffkFhBZhP
qBZ4bxrVl2X6pFUf5fQc9c1w0ROTcoQ6e9jJRKhyM3Wr2OXcciwZEVRbXIrevsd2
4e/aASG9A0patmTG5zUWyIxUNhG5nMY6nr2bjpD1cSLiDzenCuqMCjJd80rtc28C
RhwHAoGBALyiAqyQ6B1WfKwNFHwAEkJK50cQ6YWiUqazlaMokRL5ZiIU9SBNHZxg
aE1Bas2Wwhvd1jFlMt+OAfH/drM8CpHWAXJRPIr1vsVX8Q6Tl/4EsxH+2mUenFC+
goSOTyzLhSYsP4DJ3oP5FLRec16Z5U8vjuGHSKuN2kJQjhS4WFe7
-----END RSA PRIVATE KEY-----
)";
CGRPCRequest::CGRPCRequest()
	: m_connectIpStr("")
	, m_connectPort("")
	, m_errorDetail("")
{
	m_exitFlag = false;
	//CRS连接监听线程
	std::thread thGrpc = std::thread
	{
		[=]()
		{
		    // 线程运行后先等待10秒再上报  连接状态（避免 Start() 前误报故障）。识别不依赖此延迟。
			SleepMilliseconds(10000);
			m_nState = 0;
			time_t tCurSec = time(nullptr);
			while (!m_exitFlag)
			{
				int type = 0;
				if (IsConnect())
				{
					type = StateInfo_RUN_STATE_RUN_NORMAL;
					g_OcrSystemState = OCR_SYS_NORMAL;
				}
				else
				{
					ReConnect();
					type = StateInfo_RUN_STATE_RUN_FAULT;
					g_OcrSystemState = OCR_CRS_DISCONNECT;
				}
				if (m_nState != type)
				{
					PackStateInfo(StateInfo_ITEM_TYPE_ITEM_COMPUTER, "CRS", "CRS check",
						StateInfo::RUN_STATE(type), GetConnectIpStr());
					m_nState = type;
				}

				//定时发送状态信息
				if (g_AppConfig.userCfg.stateInfoInterval > 0 &&
					(time(nullptr) - tCurSec) >= (g_AppConfig.userCfg.stateInfoInterval))
				{
					PackStateInfo(StateInfo_ITEM_TYPE_ITEM_COMPUTER, "CRS", "CRS check",
						StateInfo::RUN_STATE(type), GetConnectIpStr());
					tCurSec = time(nullptr);
				}
				SleepMilliseconds(100);
			}
		}
	};
	thGrpc.detach();
}

CGRPCRequest::~CGRPCRequest()
{
	m_exitFlag = true;
}

CGRPCRequest * CGRPCRequest::GetInstance()
{
	static CGRPCRequest _inst;
	return &_inst;
}

std::string CGRPCRequest::GetConnectIpStr()
{
	return m_connectIpStr;
}

std::string CGRPCRequest::GetErrorDetail()
{
	return m_errorDetail;
}

bool CGRPCRequest::ConnectCRS(string ipaddr, string port)
{
	try
	{
		std::shared_ptr<Channel> channel = grpc::CreateChannel(ipaddr + ":" + port, grpc::InsecureChannelCredentials());
		m_connectIpStr = ipaddr;
		m_connectPort = port;
		/*gpr_timespec tmout{20, 0, GPR_TIMESPAN};
		console->info("Connect CRS {}:{}", ipaddr, port);
		if (channel->WaitForConnected<gpr_timespec>(tmout))
			console->info("Connect CRS {}:{} ok", ipaddr, port);
		else
		{
			console->error("Connect CRS {}:{} fail", ipaddr, port);
			return false;
		}*/
		if (channel)
		{
			stub_ = ocrs::pb::main::NewStub(channel);
			if (stub_)
			{
				return Identity();
			}
		}
	}
	catch (std::exception&e)
	{
		console->error("Connect CRS {}:{} error:{}", ipaddr, port, e.what());
	}
	catch (...)
	{
		console->error("Connect CRS {}:{} error", ipaddr, port);
	}
	return false;
}

void CGRPCRequest::SetTimeSpec(ClientContext& context)
{
	//console->info("set CRS_GRPC_WaitTime :{}", g_AppConfig.userCfg.CRS_GRPC_WaitTime);
	gpr_timespec timespec;
	timespec.tv_sec = g_AppConfig.userCfg.CRS_GRPC_WaitTime;//通过app配置的userconfig中，CRS_GRPC_WaitTime设置阻塞时间，默认4秒
	timespec.tv_nsec = 0;
	timespec.clock_type = GPR_TIMESPAN;
	context.set_deadline(timespec);
	context.set_wait_for_ready(false);
}

bool CGRPCRequest::Start()
{
	//---------密钥连接--------------start


		//安全验证
	//grpc::SslCredentialsOptions ssl_options;

	//ssl_options.pem_root_certs = CA_CRT;
	//ssl_options.pem_private_key = CLIENT_KEY;
	//ssl_options.pem_cert_chain = CLIENT_CRT;
	////// Create a default SSL ChannelCredentials object.
	//auto channel_creds = grpc::SslCredentials(ssl_options);
	//grpc::ChannelArguments cargs;
	//cargs.SetMaxReceiveMessageSize(100 * 1024 * 1024);
	//cargs.SetMaxSendMessageSize(100 * 1024 * 1024);
	//cargs.SetSslTargetNameOverride("ocrservice");
	//////cargs.SetSslTargetNameOverride("gamemail.ztgame.com");  // 如果加了 DNS 就不用这个了

	////auto call_creds = grpc::MetadataCredentialsFromPlugin(
	////	std::unique_ptr<grpc::MetadataCredentialsPlugin>(new TokenAuthenticator("token")));

	////auto compsited_creds = grpc::CompositeChannelCredentials(channel_creds, call_creds);

	////// Create a channel using the credentials created in the previous step.

	//auto channel = grpc::CreateCustomChannel(masterIP + ":" + masterPort, channel_creds, cargs);
	//if (!channel)
	//{
	//	channel = grpc::CreateCustomChannel(slaveIP + ":" + slavePort, channel_creds, cargs);
	//	if (!channel)
	//	{
	//		return false;
	//	}
	//}



	//---------密钥连接--------------end

	//无安全验证
	for (auto&crs : g_AppConfig.arrCrsCfg)
	{
		console->info("[CRS] Connect [{} {}:{}]", crs.crs_name, crs.ipaddr, crs.port);
		if (ConnectCRS(crs.ipaddr, crs.port))
		{
			console->info("[CRS] Connect [{}/{}:{}] ok", crs.crs_name, crs.ipaddr, crs.port);
			return true;
		}
		console->error("[CRS] Connect [{}/{}:{}] fail {}", crs.crs_name, crs.ipaddr, crs.port, m_errorDetail);

	}
	return false;

}
bool CGRPCRequest::GetOcrResult(const std::vector<shared_ptr<GRpc_RImage>>& imgs, GRpc_RepeadResult &result)
{
	m_lastDeadlineExceeded = false;
	if (imgs.empty())
	{
		console->error("[CRS] 待识别图片队列为空");
		return false;
	}
	return RecordRoute(imgs, result);
}

//bool CGRPCRequest::Identity()
//{
//	Identityverify a;
//	Identityreturn *b = new Identityreturn;
//	ClientContext context;
//	return RequestStatus(stub_->Identity(&context, a, b));
//}
bool CGRPCRequest::Identity()
{
	if (!stub_)
	{
		return false;
	}
	Identityverify verify;
	verify.set_ip(m_connectIpStr);
	verify.set_port(m_connectPort);
	Identityreturn ret;

	ClientContext context;
	SetTimeSpec(context);
	return RequestStatus(stub_->Identity(&context, verify, &ret));
}

bool CGRPCRequest::IsConnect()
{
	return Identity();
}

bool CGRPCRequest::ReConnect()
{
	return Start();
}

bool CGRPCRequest::GetFeature(const GRpc_RImage& img, GRpc_RepeadResult &result)
{
	ClientContext context;
	SetTimeSpec(context);
	return RequestStatus(stub_->Function_call(&context, img, &result));
}

bool CGRPCRequest::ListFeatures(const GRpc_RImage& img, std::vector<GRpcResult> &results)
{
	if (!stub_)
	{
		return false;
	}
	ClientContext context;
	SetTimeSpec(context);
	GRpcResult rpcr;
	std::unique_ptr<ClientReader<GRpcResult> > reader(
		stub_->Reply_stream(&context, img));
	while (reader->Read(&rpcr))
	{
		GRpcResult result;
		result.CopyFrom(rpcr);
		results.push_back(result);
	}
	return RequestStatus(reader->Finish());
}

bool CGRPCRequest::RecordRoute(const std::vector<shared_ptr<GRpc_RImage>>& imgs, GRpc_RepeadResult &result)
{
	if (!stub_)
	{
		m_lastDeadlineExceeded = false;
		m_errorDetail = "CRS stub 未连接";
		return false;
	}
	console->info("[CRS] 识别开始");
	bool re = false;
	ClientContext context;
	SetTimeSpec(context);
	std::unique_ptr<ClientWriter<GRpc_RImage> > writer(
		stub_->Request_stream(&context, &result));
	for (int i = 0; i < imgs.size(); i++)
	{
		if (!writer->Write(*imgs[i]))
		{
			// Broken stream.
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	writer->WritesDone();
	console->info("[CRS] 识别结束");
	bool isFinish = RequestStatus(writer->Finish());
	console->info("[CRS] 识别返回状态 {}", m_errorDetail);
	return isFinish;
}

bool CGRPCRequest::RequestStatus(const Status & status)
{
	m_lastDeadlineExceeded = false;
	if (status.ok())
	{
		m_errorDetail = "成功";
		return true;
	}
	else
	{
		// DEADLINE_EXCEEDED：CRS 在线但在 CRS_GRPC_WaitTime 内未完成回包 → StatusCode=2
		m_lastDeadlineExceeded = (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED);
		m_errorDetail = status.error_message();
		return false;
	}
}
bool CGRPCRequest::RouteChat(const std::vector<shared_ptr<GRpc_RImage>> &imgs, std::vector<GRpc_RepeadResult> &results)
{
	if (!stub_)
	{
		return false;
	}
	ClientContext context;
	SetTimeSpec(context);
	std::shared_ptr<ClientReaderWriter<GRpc_RImage, GRpc_RepeadResult> > stream(
		stub_->Bidirectional_stream(&context));

	std::thread writer([stream, &imgs]() {
		for (const auto& img : imgs) {
			stream->Write(*img);
		}
		stream->WritesDone();
	});

	GRpc_RepeadResult result;
	while (stream->Read(&result))
	{
		GRpc_RepeadResult resultTmp;
		resultTmp.CopyFrom(result);
		results.push_back(resultTmp);
	}
	writer.join();
	return RequestStatus(stream->Finish());

}
