#pragma once
#include <string>
#include "Utils.h"
#include "CDataIntegration.h"
class ComposeResult
{
public:
	ComposeResult(TaskManager *pMgr);
	~ComposeResult();
public:
	void ComposeResults(const GRpc_RepeadResult &result);
	void ComposeCnRet(vector<tCnItem>& arrGroupCn, tCnItem& cn_ret);
	bool ISO6346Check(const string& cc);

private:
	//结果校验并返回
	bool ComposeCRSResult(const GRpcResult& gr);

	//校验箱号 如果校验并使用返回true，否则false
	bool ComposeCNR(int group, CnResult* cnr);
	void CheckOwner(CnResult* cnr);
	//配置文件中配置了船主的映射
	void OwnerMapping(std::string &cnm);
	void CheckCnn(CnResult* cnr);
	//配置文件中配置了Cnn的映射
	void CnnMapping(std::string &iso);
	void CheckIso(CnResult* cnr);
	//配置文件中配置了ISO的映射
	void IsoMapping(std::string &iso);
	// 判断箱号 cc 是否通过 ISO6346 标准校验
	int GetCnValidCount(const string &pszCn);
	int GetIsoValidCount(const string &pszIso);
	int GetCnmValidCount(const string &pszCnm);
	int GetCnnValidCount(const string &pszCnn);
	int GetISO6346CheckDigit(const string& cc);
	int GetLaneByImgLabel(int imgLabel);
	//校验铅封
	bool ComposeSEALR(int group, SealResult* sealr);
	//校验危标
	bool ComposeImdgr(int group, ImdgResult* imdgr);
	//校验箱门
	bool ComposeDR(const string &camInstallName, int group, DoorResult* dr);
	//校验箱门打开
	bool ComposeDOR(int group, DoorOPENResult* dor);
	//校验残损
	bool ComposeDAMAGER(int group, DamageResult* damage_ret);
	//校验车号
	bool ComposePMR(int lane,const string &camInstallName, int group, PMResult* pmr,int truckType);
	bool ComposeQR(int lane, const string &camInstallName, int group, QRResult& ret);
	//车号规则
	void PMRRules(PMResult &pmr);
	//箱主号预配
	int CnmFile(std::string path);
	//箱型号预配
	int IsoFile(std::string path);
private:
	TaskManager *m_pTaskMgr;
};
//箱门第一次复合标志位
extern	int m_DRisFri;//侧面球机识别结果
extern	int m_DRisFriAxis;//吊具相机第一次复合结果
