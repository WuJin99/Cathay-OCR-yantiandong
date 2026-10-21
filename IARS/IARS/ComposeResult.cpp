#include "ComposeResult.h"
#include "Console.h"
#include "AppConfig.h"
#include "TaskManager.h"
int m_DRisFri;//侧面球机识别结果
int m_DRisFriAxis;//吊具相机第一次复合结果
ComposeResult::ComposeResult(TaskManager *pMgr)
{
	m_pTaskMgr = pMgr;
}

ComposeResult::~ComposeResult()
{
}
void ComposeResult::ComposeResults(const GRpc_RepeadResult & result)
{
	if (result.result_size() <= 0)
	{
		console->warn("[ComposeResult] 待复核结果为空");
		return ;
	}
	for (auto& ret:result.result())
	{
		ComposeCRSResult(ret);
	}
	m_pTaskMgr->AddTaskLog(console->info("[ComposeResult] 复核结果：{}", ConvertChs(m_pTaskMgr->m_TaskInfo.DebugString())));
}

void UpdateRetConfidence(int img_label, int rtype, float conf)
{
	g_mtxPicItem.lock();
	for (auto&item : g_arrPicItem)
	{
		if (item.img_label == img_label)
		{
			for (auto&param:item.arrParam)
			{
				if (param.param.type() == rtype)
				{
					param.fRetConfidence = conf;
					break;
				}
			}
			break;
		}
	}
	g_mtxPicItem.unlock();
}
int ComposeResult::GetLaneByImgLabel(int imgLabel)
{
	std::lock_guard<std::mutex> lck(g_mtxPicItem);
	for (auto&t : g_arrPicItem)
	{
		if (imgLabel == t.img_label)
		{
			for (auto &cfg:m_pTaskMgr->m_pDevMgr->gunCfgList)
			{
				if (cfg.first == t.cam_ip)
				{
					return cfg.second->lane;
				}
			}
		}
	}
	return 0;
}
//将str按nLen个数放入到arr中
void GetSubString(vector<string>& arr, const string& str, int nLen)
{
	if (str.length() <= nLen)
	{
		arr.push_back(str);
		return;
	}
	char szBuf[10] = {};
	for (size_t i = 0; i <= str.length()-nLen; i++)
	{
		strncpy(szBuf, str.c_str() + i, nLen);
		arr.push_back(szBuf);
	}
}

bool ComposeResult::ComposeCRSResult(const GRpcResult& ret)
{
	g_imgLabelMutex.lock();
	g_nRecvRetLabel = ret.img_label();
	g_imgLabelMutex.unlock();

	switch (ret.rtype())
	{
	case GRpcResult_RESULT_TYPE_T_ZERO:
	{
		console->error("[ComposeResult] 识别结果类型为：初始化 没有结果");
		return false;
	}
	case GRpcResult_RESULT_TYPE_T_CNR://箱号识别结果复核
	{
		CnResult cnr;
		if (ret.recogresult().UnpackTo(&cnr))
		{
			//存在cnm、cnn、iso识别结果中字符个数不是4/7/4情况
			CnResult cnTemp = cnr;
			vector<string> arrCnm;
			vector<string> arrCnn;
			vector<string> arrIso;
			GetSubString(arrCnm, cnr.cn().cnm(), 4);
			GetSubString(arrCnn, cnr.cn().cnn(), 7);
			GetSubString(arrIso, cnr.cn().iso(), 4);
			for (auto&cnm : arrCnm)
			{
				for (auto&cnn : arrCnn)
				{
					for (auto&iso : arrIso)
					{
						cnTemp.mutable_cn()->set_cnm(cnm);
						cnTemp.mutable_cn()->set_cnn(cnn);
						cnTemp.mutable_cn()->set_iso(iso);
						//箱号识别结果置信度，使用cnm/cnn的平方和，以便在CDataIntegration::DataIntegration中根据置信度筛选上传到ECS的图片
						float fConfidence = cnr.box().rect_cnm().confidence() * cnr.box().rect_cnm().confidence() +
							cnr.box().rect_cnn().confidence() * cnr.box().rect_cnn().confidence();
						if (ISO6346Check(cnTemp.cn().cnm() + cnTemp.cn().cnn()))
						{
							//箱号通过校验，置信度加1
							fConfidence += 1;
						}
						UpdateRetConfidence(ret.img_label(), ret.rtype(), fConfidence);
						ComposeCNR(ret.img_label(), &cnTemp);
					}
				}
			}

			return true;
		}
	}
	case GRpcResult_RESULT_TYPE_T_SEAL://铅封识别结果复核
	{
		SealResult sr;
		if (ret.recogresult().UnpackTo(&sr))
		{
			//以铅封结果个数进行判断
			UpdateRetConfidence(ret.img_label(), ret.rtype(), sr.seal_arr_size());
			return ComposeSEALR(GetGroupByImgLabel(ret.img_label()), &sr);
		}
	}
	case GRpcResult_RESULT_TYPE_T_IMDG://危标识别结果复核
	{
		ImdgResult ir;
		if (ret.recogresult().UnpackTo(&ir))
		{
			//以危标结果个数进行判断
			UpdateRetConfidence(ret.img_label(), ret.rtype(), ir.imdg_arr_size());
			return ComposeImdgr(GetGroupByImgLabel(ret.img_label()), &ir);
		}
	}
	case GRpcResult_RESULT_TYPE_T_DOOR://箱门识别结果复核
	{
		DoorResult dr;
		if (ret.recogresult().UnpackTo(&dr))
		{
			m_pTaskMgr->SetDoorDirRet(ret.img_label(), dr);
			UpdateRetConfidence(ret.img_label(), ret.rtype(), dr.confidence());
			return ComposeDR(GetCamNameByImgLabel(ret.img_label()), GetGroupByImgLabel(ret.img_label()), &dr);
		}
	}
	case GRpcResult_RESULT_TYPE_T_DOOR_OPEN://箱门打开识别结果复核
	{
		DoorOPENResult dor;
		if (ret.recogresult().UnpackTo(&dor))
		{
			m_pTaskMgr->SetDoorOpenRet(ret.img_label(), dor);
			m_pTaskMgr->m_bDoorOpenReNeed = true;
			UpdateRetConfidence(ret.img_label(), ret.rtype(), dor.confidence());

			return ComposeDOR(GetGroupByImgLabel(ret.img_label()), &dor);
		}
	}
	case GRpcResult_RESULT_TYPE_T_DAMAGE://验残识别结果
	{
		DamageResult damage_ret;
		if (ret.recogresult().UnpackTo(&damage_ret))
		{
			//以残损结果个数进行判断
			UpdateRetConfidence(ret.img_label(), ret.rtype(), damage_ret.dmg_arr_size());
			return ComposeDAMAGER(GetGroupByImgLabel(ret.img_label()), &damage_ret);
		}
	}
	case GRpcResult_RESULT_TYPE_T_PMN://
	{
		PMResult pmr;
		if (ret.recogresult().UnpackTo(&pmr))
		{
			//处理车号结果置信度
			float fConf = pmr.rect_pmn().confidence();
			for (auto&c:pmr.pmn())
			{
				if (!isNumber(c))
				{
					//结果中包含非数字，置信度减半
					fConf *= 0.5;
					break;
				}
			}
			if (pmr.pmn().empty())
				fConf = 0.01;//防止无结果但置信度高
			UpdateRetConfidence(ret.img_label(), ret.rtype(), fConf);
			return ComposePMR(GetLaneByImgLabel(ret.img_label()),GetCamNameByImgLabel(ret.img_label()), GetGroupByImgLabel(ret.img_label()), &pmr,GetTruckTypeByImgLabel(ret.img_label()));
		}
	}
	case GRpcResult_RESULT_TYPE_T_PMN_QR://车号识别结果复核
	{
		QRResult qr;
		if (ret.recogresult().UnpackTo(&qr))
		{
			//二维码不为空，置信度认为100，否则为0
			UpdateRetConfidence(ret.img_label(), ret.rtype(), qr.qr().length() ? 100.0 : 0.0);
			return ComposeQR(GetLaneByImgLabel(ret.img_label()), GetCamNameByImgLabel(ret.img_label()), GetGroupByImgLabel(ret.img_label()), qr);
		}
	}
	default:
		break;
	}
	return false;
}

// 立即箱号复核 - 拿到识别结果后，立即对当期识别结果做处理
bool ComposeResult::ComposeCNR(int img_label, CnResult * cnr)
{
	int laneTmp = 0;
	string strCamIp = "";
	string strCamName = "";
	bool isCnFirst;
	g_mtxPicItem.lock();
	for (auto&t : g_arrPicItem)
	{
		if (img_label == t.img_label)
		{
			strCamIp = t.cam_ip;
			strCamName = t.cam_name;
			isCnFirst = t.cnFirstSend;
			laneTmp = t.lane;
			break;
		}
	}
	g_mtxPicItem.unlock();

	string cnm = cnr->cn().cnm();
	if (cnm.length() > 4)
	{
		cnm = cnm.substr(0,4);
	}
	string cnn = cnr->cn().cnn();
	if (cnn.length() > 7)
	{
		cnn = cnn.substr(0, 7);
	}
	string iso = cnr->cn().iso();
	if (iso.length() > 4)
	{
		iso = iso.substr(0, 4);
	}
	string cn_str = cnm + cnn;
	auto it = m_pTaskMgr->m_pDevMgr->AxisCfgList.find(strCamName);
	if (it != m_pTaskMgr->m_pDevMgr->AxisCfgList.end())//判断识别结果是否来自吊具相机
	{
		if (cnn.length() == 6 && cn_str.length()==10)
		{
			int cd = GetISO6346CheckDigit(cn_str);
			cn_str +=static_cast<char>(cd);
			m_pTaskMgr->AddTaskLog(console->info("[Compose] 箱号复核 此结果校验位缺失，自动补齐后为 :{}", cn_str));
		}
	}
	if (laneTmp <= 0)
	{
		m_pTaskMgr->AddTaskLog(console->info("[Compose] 箱号复核 此结果相机没有车道 ip:{}", strCamIp));
		return false;
	}

	std::lock_guard<std::mutex> lck_cn(m_pTaskMgr->m_mtxCnItem);
	auto&arrGroupCn = m_pTaskMgr->m_arrCnItem;
	int nCnIdx = -1;
	bool isExist = false;
	for (auto &item : arrGroupCn)
	{
		if (item.first == laneTmp)
		{
			for (int nIdx=0; nIdx<item.second.size(); ++nIdx)
			{
				if (item.second[nIdx].cnm == cnm &&
					item.second[nIdx].cnn == cnn)
				{
					nCnIdx = nIdx;
					break;
				}
			}
		}
	}
	//更新arrGroupCn中的箱号结果数据项
	if (nCnIdx == -1)
	{
		//没有找到
		tCnItem item;
		item.cnFirst = isCnFirst;
		item.cn_str = cn_str;
		item.cnm = cnm;
		item.cnn = cnn;
		item.iso = iso;
		item.cnm_conf = cnr->box().rect_cnm().confidence();
		item.cnn_conf = cnr->box().rect_cnn().confidence();
		item.iso_conf = cnr->box().rect_iso().confidence();
		item.bCheck = ISO6346Check(item.cn_str);
		item.nRetCount = 1;
		item.nowTime = GetCurrentStamp64();
		item.setCamIp.insert(strCamIp);//记录相机IPs
		item.UpdateConf();
		arrGroupCn[laneTmp].push_back(item);
		m_pTaskMgr->AddTaskLog(console->info("[Compose] 新增箱号 {}", item.to_string()));
	}
	else
	{
		auto& item = arrGroupCn[laneTmp][nCnIdx];
		item.nRetCount++;
		//更新时间
		item.nowTime = GetCurrentStamp64();
		if (isCnFirst)
		{
			item.cnFirst = isCnFirst;
		}
		item.setCamIp.insert(strCamIp);
		if (cnr->box().rect_cnm().confidence() > item.cnm_conf)
		{
			item.cnm_conf = cnr->box().rect_cnm().confidence();
		}
		if (cnr->box().rect_cnn().confidence() > item.cnn_conf)
		{
			item.cnn_conf = cnr->box().rect_cnn().confidence();
		}
		if (cnr->box().rect_iso().confidence() > item.iso_conf)
		{
			item.iso_conf = cnr->box().rect_iso().confidence();
			item.iso = iso;
		}
		item.UpdateConf();
		m_pTaskMgr->AddTaskLog(console->info("[Compose] 更新箱号 {}", arrGroupCn[laneTmp][nCnIdx].to_string()));
	}
	m_pTaskMgr->AddTaskLog(console->info("[Compose] 立即箱号复核================}"));
	for (auto & item: arrGroupCn)
	{
		for (auto &i:item.second)
		{
			m_pTaskMgr->AddTaskLog(console->info("[Compose] 立即箱号复核 lane[{}]:[{}]",item.first, i.cn_str));
		}
	}
	m_pTaskMgr->AddTaskLog(console->info("[Compose] 立即箱号复核================}"));
	return true;
}

namespace {
	// 整号/组合主分 = cnm^2 + cnn^2（不含 iso、不含校验加分）
	float CnSegScore(const tCnItem& item)
	{
		return item.cnm_conf * item.cnm_conf + item.cnn_conf * item.cnn_conf;
	}

	// score 相同时：多相机 > 次数 > cnFirst > iso_conf
	bool IsBetterFullCandidate(const tCnItem& cand, const tCnItem& cur)
	{
		const float candScore = CnSegScore(cand);
		const float curScore = CnSegScore(cur);
		if (candScore != curScore)
			return candScore > curScore;
		if (cand.setCamIp.size() != cur.setCamIp.size())
			return cand.setCamIp.size() > cur.setCamIp.size();
		if (cand.nRetCount != cur.nRetCount)
			return cand.nRetCount > cur.nRetCount;
		if (cand.cnFirst != cur.cnFirst)
			return cand.cnFirst;
		return cand.iso_conf > cur.iso_conf;
	}

	bool IsBetterComboCandidate(const tCnItem& cand, const tCnItem& cur)
	{
		const float candScore = CnSegScore(cand);
		const float curScore = CnSegScore(cur);
		if (candScore != curScore)
			return candScore > curScore;
		if (cand.nRetCount != cur.nRetCount)
			return cand.nRetCount > cur.nRetCount;
		return cand.nValidCount > cur.nValidCount;
	}

	void AttachBestIso(const vector<tCnItem>& arrGroupCn, tCnItem& cn_ret)
	{
		float fMaxIso = 0;
		for (const auto& item : arrGroupCn)
		{
			if (item.iso_conf > fMaxIso)
			{
				fMaxIso = item.iso_conf;
				cn_ret.iso_conf = item.iso_conf;
				cn_ret.iso = item.iso;
			}
		}
	}
}

// 二次箱号复核 - 先选校验通过的完整记录(cnm^2+cnn^2)，门限 0.9；不够再跨结果组合
void ComposeResult::ComposeCnRet(vector<tCnItem>& arrGroupCn, tCnItem& cn_ret)
{
	static const float kFullScoreGate = 0.9f; // 同记录 cnm^2+cnn^2，一侧偏低时进入组合
	static const float kComboDelta = 0.03f;   // 组合须明显优于整号才替换

	m_pTaskMgr->AddTaskLog(console->debug("[Compose] 箱号复核：************************************"));
	for (auto& item : arrGroupCn)
	{
		m_pTaskMgr->AddTaskLog(console->debug("[Compose] 箱号复核:箱号数据{}[{}] segScore={}",
			item.nowTime, item.to_string(), CnSegScore(item)));
	}
	m_pTaskMgr->AddTaskLog(console->debug("[Compose] 箱号复核：************************************"));

	// 1. 校验通过的完整记录中按 segScore 选 best_full（平局用相机数/次数等）
	bool hasBestFull = false;
	tCnItem bestFull;
	for (auto& item : arrGroupCn)
	{
		if (!item.bCheck)
			continue;
		if (!hasBestFull || IsBetterFullCandidate(item, bestFull))
		{
			bestFull = item;
			hasBestFull = true;
		}
	}

	const float scoreFull = hasBestFull ? CnSegScore(bestFull) : 0.f;
	const bool needCombo = !hasBestFull || scoreFull < kFullScoreGate;

	if (hasBestFull && !needCombo)
	{
		cn_ret = bestFull;
		m_pTaskMgr->AddTaskLog(console->info(
			"[Compose] 箱号复核：整号通过校验且 segScore={}>={}，[{}]",
			scoreFull, kFullScoreGate, cn_ret.to_string()));
		return;
	}

	if (hasBestFull)
	{
		m_pTaskMgr->AddTaskLog(console->info(
			"[Compose] 箱号复核：整号候选 segScore={}<{}，进入组合判断，[{}]",
			scoreFull, kFullScoreGate, bestFull.to_string()));
	}
	else
	{
		m_pTaskMgr->AddTaskLog(console->info("[Compose] 箱号复核：无校验通过整号，进入组合判断"));
	}

	// 2. 跨记录组合 cnm × cnn
	vector<tCnItem> arrCombination;
	for (auto& item_cnm : arrGroupCn)
	{
	    // 先跳过空的cnm和cnn
		if (item_cnm.cnm.empty())
			continue;
		for (auto& item_cnn : arrGroupCn)
		{
			if (item_cnn.cnn.empty())
				continue;
			bool bFind = false;
			string cn_str = item_cnm.cnm + item_cnn.cnn;
			// 在已有组合里找有没有同 cn_str（去重/累加）
			for (auto& item : arrCombination)
			{
				if (item.cn_str == cn_str)
				{
					item.nRetCount++;
					if (item.cnm_conf < item_cnm.cnm_conf)
						item.cnm_conf = item_cnm.cnm_conf;
					if (item.cnn_conf < item_cnn.cnn_conf)
						item.cnn_conf = item_cnn.cnn_conf;
					bFind = true;
					break;
				}
			}
			// 没有重复的就加入arrCombination
			if (!bFind)
			{
				tCnItem item;
				item.nRetCount = 1;
				item.nValidCount = GetCnValidCount(cn_str.c_str());
				item.bCheck = ISO6346Check(cn_str);
				item.cn_str = cn_str;
				item.cnm = item_cnm.cnm;
				item.cnm_conf = item_cnm.cnm_conf;
				item.cnn = item_cnn.cnn;
				item.cnn_conf = item_cnn.cnn_conf;
				arrCombination.push_back(item);
			}
		}
	}

	bool hasBestCombo = false;
	tCnItem bestCombo;
	for (auto& item : arrCombination)
	{
		if (!item.bCheck)
			continue;
		if (!hasBestCombo || IsBetterComboCandidate(item, bestCombo))
		{
			bestCombo = item;
			hasBestCombo = true;
		}
	}
	if (hasBestCombo)
	{
		m_pTaskMgr->AddTaskLog(console->debug("[Compose] 箱号复核：组合候选共{}条，最优校验通过[{}] segScore={}",
			arrCombination.size(), bestCombo.to_string(), CnSegScore(bestCombo)));
	}
	else
	{
		m_pTaskMgr->AddTaskLog(console->debug("[Compose] 箱号复核：组合候选共{}条，无校验通过组合",
			arrCombination.size()));
	}

	// 3. 组合通过校验时，与整号比较（δ）；无整号则直接用组合
	if (hasBestCombo)
	{
		const float scoreCombo = CnSegScore(bestCombo);
		const bool takeCombo = !hasBestFull || (scoreCombo + 1e-6f >= scoreFull + kComboDelta);
		if (takeCombo)
		{
			cn_ret = bestCombo;
			AttachBestIso(arrGroupCn, cn_ret);
			cn_ret.UpdateConf();
			m_pTaskMgr->AddTaskLog(console->info(
				"[Compose] 箱号复核：[4]组合通过校验 segScore={}（整号={} δ={}），[{}]",
				scoreCombo, scoreFull, kComboDelta, cn_ret.to_string()));
			return;
		}
		m_pTaskMgr->AddTaskLog(console->info(
			"[Compose] 箱号复核：组合 segScore={} 未优于整号+δ，保留整号",
			scoreCombo));
	}

	// 4. 组合无更优结果：仍用校验通过的整号（即使低于门限）
	if (hasBestFull)
	{
		cn_ret = bestFull;
		m_pTaskMgr->AddTaskLog(console->info(
			"[Compose] 箱号复核：[3]保留整号（segScore={}），[{}]",
			scoreFull, cn_ret.to_string()));
		return;
	}

	// 5. 组合未过校验：找次数最多且有效字符>=9
	int nMaxCount = 0;
	for (auto& item : arrCombination)
	{
		if (item.nRetCount > nMaxCount && item.nValidCount >= 9)
		{
			nMaxCount = item.nRetCount;
			cn_ret = item;
		}
	}
	if (nMaxCount >= 2)
	{
		AttachBestIso(arrGroupCn, cn_ret);
		cn_ret.UpdateConf();
		m_pTaskMgr->AddTaskLog(console->info("[Compose] 箱号复核：[5]箱号未通过校验，{}次识别结果，[{}]", nMaxCount, cn_ret.to_string()));
		return;
	}

	// 6. 最坏：各段取置信度最高
	cn_ret.cnm = "???U";
	cn_ret.cnn = "???????";
	float fMaxConfidence = 0;
	for (auto& item : arrGroupCn)
	{
		if (item.cnm_conf > fMaxConfidence)
		{
			fMaxConfidence = item.cnm_conf;
			cn_ret.cnm_conf = item.cnm_conf;
			cn_ret.cnm = item.cnm;
		}
	}
	fMaxConfidence = 0;
	for (auto& item : arrGroupCn)
	{
		if (item.cnn_conf > fMaxConfidence)
		{
			fMaxConfidence = item.cnn_conf;
			cn_ret.cnn_conf = item.cnn_conf;
			cn_ret.cnn = item.cnn;
		}
	}
	AttachBestIso(arrGroupCn, cn_ret);
	cn_ret.cn_str = cn_ret.cnm + cn_ret.cnn;
	cn_ret.UpdateConf();
	m_pTaskMgr->AddTaskLog(console->info("[Compose] 箱号复核：[6]箱号未通过校验，取置信度最高结果，[{}]", cn_ret.to_string()));
}

/*
字符有效个数，基本规则：
1、无？
2、第一位为2、4、L
3、第二位为2、5
4、第三位为GVURTP
5、第四位为1
*/
int ComposeResult::GetIsoValidCount(const string &pszIso)
{
	if (pszIso.length() < 4)
	{
		return 0;
	}
	int nCount = 0;
	if (pszIso[0] == '2' ||
		pszIso[0] == '4' ||
		pszIso[0] == 'L')
	{
		nCount++;
	}
	if (pszIso[1] == '2' ||
		pszIso[1] == '5')
	{
		nCount++;
	}
	if (pszIso[2] == 'G' ||
		pszIso[2] == 'V' ||
		pszIso[2] == 'U' ||
		pszIso[2] == 'R' ||
		pszIso[2] == 'T' ||
		pszIso[2] == 'P')
	{
		nCount++;
	}
	if (pszIso[3] == '1')
	{
		nCount++;
	}
	return nCount;
}
//前4个为字母，后7个为数字
int ComposeResult::GetCnValidCount(const string &pszCn)
{
	if (pszCn.length() < 11)
	{
		return 0;
	}
	int nCount = 0;
	for (int i = 0; i < 4; ++i)
	{
		if (pszCn[i] >= 'A' && pszCn[i] <= 'Z')
		{
			nCount++;
		}
	}
	for (int i = 4; i < 11; ++i)
	{
		if (pszCn[i] >= '0' && pszCn[i] <= '9')
		{
			nCount++;
		}
	}
	return nCount;
}
int ComposeResult::GetCnmValidCount(const string &pszCnm)
{
	if (pszCnm.length() != 4)
	{
		return 0;
	}
	int nCount = 0;
	for (int i = 0; i < 4; ++i)
	{
		if (pszCnm[i] >= 'A' && pszCnm[i] <= 'Z')
		{
			nCount++;
		}
	}
	return nCount;
}
int ComposeResult::GetCnnValidCount(const string &pszCnn)
{
	if (pszCnn.length() != 7)
	{
		return 0;
	}
	int nCount = 0;
	for (int i = 0; i < 7; ++i)
	{
		if (pszCnn[i] >= '0' && pszCnn[i] <= '9')
		{
			nCount++;
		}
	}
	return nCount;
}
void ComposeResult::CheckOwner(CnResult * cnr)
{
	std::string cnm = cnr->cn().cnm();
	if (cnm.length() > 4)
	{
		cnm = cnm.substr(0, 4);
	}
	OwnerMapping(cnm);
	cnr->mutable_cn()->set_cnm(cnm);
}

void ComposeResult::OwnerMapping(std::string & cnm)
{

}

void ComposeResult::CheckCnn(CnResult * cnr)
{
	std::string cnn = cnr->cn().cnn();
	if (cnn.length() > 7)
	{
		cnn = cnn.substr(0, 7);
	}
	CnnMapping(cnn);
	cnr->mutable_cn()->set_cnn(cnn);
}

void ComposeResult::CnnMapping(std::string & iso)
{
}

void ComposeResult::CheckIso(CnResult * cnr)
{
	std::string iso = cnr->cn().iso();
	if (iso.length() > 4)
	{
		iso = iso.substr(0, 4);
	}
	IsoMapping(iso);
	cnr->mutable_cn()->set_iso(iso);
}

void ComposeResult::IsoMapping(std::string & iso)
{
	for (const auto &a : g_AppConfig.mapIsoNum)
	{
		if (a.first == iso)
		{
			console->warn("[ComposeResult] 配置文件中配置了此ISO需要替换。 配置中：srciso:{} dstiso:{} 此箱子ISO：{}", a.first, a.second, iso);
			iso = a.second;
			break;
		}
	}
}
// 判断箱号 cc 是否通过 ISO6346 标准校验
bool ComposeResult::ISO6346Check(const string& cc)
{
	if (cc.length() < 11)
	{
		return false;
	}
	for (int i = 0; i < 11; i++)
	{
		if (!isNumber(cc[i]) && !isLetter(cc[i]))
		{
			return false;
		}
	}
	int cd = GetISO6346CheckDigit(cc);
	if (cd < 0)
	{
		return false;
	}
	return cd == cc[10];
}

int ComposeResult::GetISO6346CheckDigit(const string& cc)
{
	const int char2num[] = {
		10, // A
		12, 13, 14, 15, 16, 17, 18, 19, 20, 21, // B-K
		23, 24, 25, 26, 27, 28, 29, 30, 31, 32, // L-U
		34, 35, 36, 37, 38 // V-Z
	};

	int sum = 0;
	int pow = 1;
	for (int i = 0; i < 4; ++i)
	{
		if (cc[i] < 'A' || cc[i] > 'Z')
		{
			return -1;
		}
		sum += char2num[cc[i] - 'A'] * pow;
		pow *= 2;
	}
	for (int i = 4; i < 10; ++i)
	{
		if (cc[i] < '0' || cc[i] > '9')
		{
			return -1;
		}
		sum += (cc[i] - '0') * pow;
		pow *= 2;
	}

	int cd = (sum % 11) % 10;
	// 非标准
	if (memcmp(cc.c_str(), "HLCU", 4) == 0)
	{
		cd = cd == 0 ? 9 : cd == 1 ? 8 : cd - 2;
	}
	return cd + '0';
}
//

//铅封复核
bool ComposeResult::ComposeSEALR(int group, SealResult * sealr)
{
	bool br = false;
	bool findGroup = false;
	for (int i = 0; i < m_pTaskMgr->m_TaskInfo.ocr().sealr_arr().size(); i++)
	{
		if (m_pTaskMgr->m_TaskInfo.ocr().sealr_arr(i).group() == group)
		{
			findGroup = true;

			SealObject *a = m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_sealr_arr(i);

			if (sealr->seal())
			{
				if (!a->sealr().seal())
				{
					a->mutable_sealr()->CopyFrom(*sealr);
					br = true;
				}
				else
				{
					if (a->sealr().seal_arr_size() < sealr->seal_arr_size())
					{
						a->mutable_sealr()->CopyFrom(*sealr);
						br = true;
					}
				}
			}
		}
	}

	if (!findGroup)//复核结果里面没有结果直接插入
	{
		SealObject *sealo = m_pTaskMgr->m_TaskInfo.mutable_ocr()->add_sealr_arr();
		sealo->set_group(group);
		sealo->mutable_sealr()->CopyFrom(*sealr);
		br = true;
	}
	return br;
}
//危标
bool ComposeResult::ComposeImdgr(int group, ImdgResult * imdgr)
{
	bool br = false;

	bool findGroup = false;

	for (int i = 0; i < m_pTaskMgr->m_TaskInfo.ocr().imdgr_arr().size(); i++)
	{
		if (m_pTaskMgr->m_TaskInfo.ocr().imdgr_arr(i).group() == group)
		{
			findGroup = true;

			ImdgObject *a = m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_imdgr_arr(i);
			if (imdgr->imdg())
			{
				if (!a->imdgr().imdg())
				{
					a->mutable_imdgr()->CopyFrom(*imdgr);
					br = true;
				}
				else
				{
					if (a->imdgr().imdg_arr_size() < imdgr->imdg_arr_size())
					{
						a->mutable_imdgr()->CopyFrom(*imdgr);
						br = true;
					}
				}
			}
		}
	}
	if (!findGroup)//复核结果里面没有结果直接插入
	{
		ImdgObject *imdgro = m_pTaskMgr->m_TaskInfo.mutable_ocr()->add_imdgr_arr();
		imdgro->set_group(group);
		imdgro->mutable_imdgr()->CopyFrom(*imdgr);
		br = true;
	}

	return br;
}
//箱门
bool ComposeResult::ComposeDR(const string &camInstallName, int group, DoorResult * dr)
{
	bool br = false;
	bool findGroup = false;

	for (int i = 0; i < m_pTaskMgr->m_TaskInfo.ocr().dr_arr().size(); i++)
	{
		if (m_pTaskMgr->m_TaskInfo.ocr().dr_arr(i).group() == group)
		{
			findGroup = true;
			//新结果置信度大于老结果置信度，替换
			if (m_pTaskMgr->m_TaskInfo.ocr().dr_arr(i).dr().confidence() < dr->confidence())
			{
				m_pTaskMgr->AddTaskLog(console->info("[doorcompose] new conf:{} > old conf:{} update dr:{}", dr->confidence(), m_pTaskMgr->m_TaskInfo.ocr().dr_arr(i).dr().confidence(),dr->DebugString()));
				m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_dr_arr(i)->mutable_dr()->CopyFrom(*dr);
				//从配置文件中读取相机的安装位置，然后确定箱门是前还是后
				if (g_PositionCfg.m.find(camInstallName) != g_PositionCfg.m.end())
				{
					if (g_PositionCfg.m.at(camInstallName) == CAM_POS_FRONT)
					{
						if (dr->type() == ocrs::pb::DoorResult::BACK_DOOR)//后箱门
						{
							m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_dr_arr(i)->set_dr_dir("FRONT");
							m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} CAM_POS_FRONT DoorResult:[BACK] DOOR_DIR:FRONT", camInstallName));
						}
						else if (dr->type() == ocrs::pb::DoorResult::FRONT_DOOR)
						{
							m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_dr_arr(i)->set_dr_dir("BACK");
							m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} CAM_POS_FRONT DoorResult:[FRONT] DOOR_DIR:BACK", camInstallName));
						}
						else
						{
							m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_dr_arr(i)->set_dr_dir("Unknow");
							m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} CAM_POS_FRONT DoorResult:[{}] DOOR_DIR:Unknow", camInstallName, dr->type()));
						}
					}
					else if (g_PositionCfg.m.at(camInstallName) == CAM_POS_BACK)
					{
						if (dr->type() == ocrs::pb::DoorResult::BACK_DOOR)//后箱门
						{
							m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_dr_arr(i)->set_dr_dir("BACK");
							m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} CAM_POS_BACK DoorResult:[BACK] DOOR_DIR:BACK", camInstallName));
						}
						else if (dr->type() == ocrs::pb::DoorResult::FRONT_DOOR)
						{
							m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_dr_arr(i)->set_dr_dir("FRONT");
							m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} CAM_POS_BACK DoorResult:[FRONT] DOOR_DIR:FRONT", camInstallName));
						}
						else
						{
							m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_dr_arr(i)->set_dr_dir("Unknow");
							m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} CAM_POS_BACK DoorResult:[{}] DOOR_DIR:Unknow", camInstallName, dr->type()));
						}
					}
					else
					{
						m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} Unknow pos {}", camInstallName, g_PositionCfg.m.at(camInstallName)));
					}
				}
				else
				{
					m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_dr_arr(i)->set_dr_dir("Unknow");
					m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} DoorResult:[Unknow] DOOR_DIR:Unknow", camInstallName));
				}
				br = true;
			}
		}
	}

	if (!findGroup)//复核结果里面没有结果直接插入
	{
		DoorObject *dro = m_pTaskMgr->m_TaskInfo.mutable_ocr()->add_dr_arr();
		dro->set_group(group);
		dro->mutable_dr()->CopyFrom(*dr);

		if (g_PositionCfg.m.find(camInstallName) != g_PositionCfg.m.end())
		{
			if (g_PositionCfg.m.at(camInstallName) == CAM_POS_FRONT)
			{
				if (dr->type() == ocrs::pb::DoorResult::BACK_DOOR)//后箱门
				{
					dro->set_dr_dir("FRONT");
					m_pTaskMgr->AddTaskLog(console->info("[doorcompose] first cam:{} CAM_POS_FRONT DoorResult:[BACK] DOOR_DIR:FRONT", camInstallName));
				}
				else if (dr->type() == ocrs::pb::DoorResult::FRONT_DOOR)
				{
					dro->set_dr_dir("BACK");
					m_pTaskMgr->AddTaskLog(console->info("[doorcompose] first cam:{} CAM_POS_FRONT DoorResult:[FRONT] DOOR_DIR:BACK", camInstallName));
				}
				else
				{
					dro->set_dr_dir("Unknow");
					m_pTaskMgr->AddTaskLog(console->info("[doorcompose] first cam:{} CAM_POS_FRONT DoorResult:[{}] DOOR_DIR:Unknow", camInstallName, dr->type()));
				}
			}
			else if (g_PositionCfg.m.at(camInstallName) == CAM_POS_BACK)
			{
				if (dr->type() == ocrs::pb::DoorResult::BACK_DOOR)//后箱门
				{
					dro->set_dr_dir("BACK");
					m_pTaskMgr->AddTaskLog(console->info("[doorcompose] first cam:{} CAM_POS_BACK DoorResult:[BACK] DOOR_DIR:BACK", camInstallName));
				}
				else if (dr->type() == ocrs::pb::DoorResult::FRONT_DOOR)
				{
					dro->set_dr_dir("FRONT");
					m_pTaskMgr->AddTaskLog(console->info("[doorcompose] first cam:{} CAM_POS_BACK DoorResult:[FRONT] DOOR_DIR:FRONT", camInstallName));
				}
				else
				{
					dro->set_dr_dir("Unknow");
					m_pTaskMgr->AddTaskLog(console->info("[doorcompose] first cam:{} CAM_POS_BACK DoorResult:[{}] DOOR_DIR:Unknow", camInstallName, dr->type()));
				}
			}
			else
			{
				m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} Unknow pos {}", camInstallName, g_PositionCfg.m.at(camInstallName)));
			}
		}
		else
		{
			dro->set_dr_dir("Unknow");
			m_pTaskMgr->AddTaskLog(console->info("[doorcompose] cam:{} DoorResult:[Unknow] DOOR_DIR:Unknow", camInstallName));
		}
		br = true;
	}
	int nSpreader;
	nSpreader = m_pTaskMgr->m_plcinfo.spreaderMode;//吊具类型0=无效;1=20ft;2=40ft;3=45ft;4=双箱
		int nSprSet = 0;
		if (nSpreader == 1)
		{
			nSprSet = 1;
		}
		else
		{
			nSprSet = 2;
		}
		auto it = m_pTaskMgr->m_pDevMgr->AxisCfgList.find(camInstallName);
		if (it != m_pTaskMgr->m_pDevMgr->AxisCfgList.end())//判断识别结果是否来自吊具相机
		{
			m_DRisFriAxis += 1;
			if (m_DRisFriAxis == 2)//发送第一次吊具识别结果；
			{
				m_pTaskMgr->m_bDoorOpenReNeed = true;
				if (m_pTaskMgr->m_plcinfo.lockStatus != 2)
				{
					m_pTaskMgr->m_bDoorDirReNeed = true;
				}
			}
		}
		else
		{
			m_DRisFri += 1;
			if (m_DRisFri == nSprSet&& m_pTaskMgr->m_plcinfo.lockStatus != 2)//非闭锁状态下发送第一次球机识别结果；
			{
				m_pTaskMgr->m_bDoorDirReNeed = true;
			}
		}

	m_pTaskMgr->AddTaskLog(console->info
	("[doorcompose] cam:{} m_DRisFri:[{}], m_DRisFriAxis:[{}],nSpreader:[{}] ,lockStatus:[{}],m_bDoorDirReNeed:[{}]，m_bDoorOpenNeed:[{}]",
	            camInstallName, m_DRisFri, m_DRisFriAxis, nSpreader, m_pTaskMgr->m_plcinfo.lockStatus,m_pTaskMgr->m_bDoorDirReNeed, m_pTaskMgr->m_bDoorOpenReNeed));
	return br;
}

bool ComposeResult::ComposeDOR(int group, DoorOPENResult * dor)
{

	bool br = false;
	bool findGroup = false;

	for (int i = 0; i < m_pTaskMgr->m_TaskInfo.ocr().dor_arr().size(); i++)
	{
		if (m_pTaskMgr->m_TaskInfo.ocr().dor_arr(i).group() == group)
		{
			findGroup = true;
			//新结果置信度大于老结果置信度，替换
			if (m_pTaskMgr->m_TaskInfo.ocr().dor_arr(i).dor().confidence() < dor->confidence())
			{
				m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_dor_arr(i)->mutable_dor()->CopyFrom(*dor);
				br = true;
			}
		}
	}

	if (!findGroup)//复核结果里面没有结果直接插入
	{
		DoorOPENObject *doro = m_pTaskMgr->m_TaskInfo.mutable_ocr()->add_dor_arr();
		doro->set_group(group);
		doro->mutable_dor()->CopyFrom(*dor);
		br = true;
	}

	return br;
}

bool ComposeResult::ComposeDAMAGER(int group, DamageResult * damager)
{
	bool br = false;

	bool findGroup = false;

	for (int i = 0; i < m_pTaskMgr->m_TaskInfo.ocr().damager_arr().size(); i++)
	{
		if (m_pTaskMgr->m_TaskInfo.ocr().damager_arr(i).group() == group)
		{
			findGroup = true;

			if (damager->damage())
			{
				DamageObject *a = m_pTaskMgr->m_TaskInfo.mutable_ocr()->mutable_damager_arr(i);
				if (!a->damager().damage())
				{
					a->mutable_damager()->CopyFrom(*damager);
					br = true;
				}
				else
				{
					if (a->damager().dmg_arr_size() < damager->dmg_arr_size())
					{
						a->mutable_damager()->CopyFrom(*damager);
						br = true;
					}
				}
			}
		}
	}
	if (!findGroup)//复核结果里面没有结果直接插入
	{
		DamageObject *damagero = m_pTaskMgr->m_TaskInfo.mutable_ocr()->add_damager_arr();
		damagero->set_group(group);
		damagero->mutable_damager()->CopyFrom(*damager);
		br = true;
	}

	return br;
}

bool ComposeResult::ComposePMR(int lane, const string &camInstallName, int group, PMResult * pmr, int truckType)
{
	if (lane <= 0)
	{
		m_pTaskMgr->AddTaskLog(console->info("[iars][pmn] 相机找不到车道:[{}] ", camInstallName));
		return false;
	}
	if (pmr->pmn().empty())
	{
		m_pTaskMgr->AddTaskLog(console->info("[iars][pmn] pos:[{}] 车号结果为空", camInstallName));
		return false;
	}

	string pmn = pmr->pmn();
	//车号结果映射
	GetTruckNumFromMap(pmn);
	if (pmn.find("?") != string::npos)
	{
		m_pTaskMgr->AddTaskLog(console->info("[iars][pmn] 结果无效[{}] ", pmn));
		return false;
	}
	/*if (pmn.length() < g_AppConfig.userCfg.pmn_minsize)
	{
		m_pTaskMgr->AddTaskLog(console->info("[iars][pmn] 结果短于正常位数无效[{}] ", pmn));
		return false;
	}*/

	float conf = pmr->rect_pmn().confidence();
	if (conf < 0.8)
	{
		m_pTaskMgr->AddTaskLog(console->info("[iars][pmn] 结果无效[{}] 置信度太低[{}]", pmn, conf));
		return false;
	}

	// CAP 抓拍车顶号：每条有效结果都追加进 m_arrPmnItem（同号也新开一条，不在入池时 conf+=/nCount++）
	// 选号在 GetPmnResult 里按 PIIS 请求瞬间 gantry 窗口再聚合；车牌仍走 m_arrPmnItem2
	m_pTaskMgr->m_mtxPmnItem.lock();
	m_pTaskMgr->AddPmnRet(lane, pmn, conf);
	m_pTaskMgr->m_mtxPmnItem.unlock();
	//

	//g_TruckNumMatchMutex.lock();

	if (pmn == m_pTaskMgr->m_planTruckNum)
	{
		m_pTaskMgr->m_planTruckMatch = true;
	}
	if (truckType == PLCInfo::TRUCK_TYPE_INNER)
	{
		m_pTaskMgr->m_IARSInnerPmn.pmn = pmn;
		m_pTaskMgr->m_IARSInnerPmn.conf = conf;
		m_pTaskMgr->AddTaskLog(console->info("[iars][pmn]  车顶号识别结果pmn[{}] conf:{} 集卡类型:{} (1:内 2:外)", pmn, conf, truckType));
	}
	else if (truckType == PLCInfo::TRUCK_TYPE_OUTER)
	{
		m_pTaskMgr->m_IARSOuterPmn.pmn = pmn;
		m_pTaskMgr->m_IARSOuterPmn.conf = conf;
		m_pTaskMgr->AddTaskLog(console->info("[iars][pmn]  车顶号识别结果pmn[{}] conf:{} 集卡类型:{} (1:内 2:外)", pmn, conf, truckType));
	}
	else
	{
		m_pTaskMgr->AddTaskLog(console->info("[iars][pmn]  车顶号识别结果pmn[{}] conf:{} 集卡类型:{} (1:内 2:外)", pmn, conf, truckType));
	}
	//要结果后，有结果才有结果
	if (m_pTaskMgr->m_TruckNumResultNeed)
	{
		m_pTaskMgr->m_IARSPmn = true;
	}


	string sendPmn = "";
	//要车号结果且没发送过结果,是否跟计划车号一致，一致直接发送
	if (m_pTaskMgr->m_TruckNumResultNeed && !m_pTaskMgr->m_TruckResultSend)
	{
		//如果内集卡
		if (m_pTaskMgr->m_truckType == PLCInfo::TRUCK_TYPE_INNER)
		{
			m_pTaskMgr->AddTaskLog(console->info("[iars][pmn]  开始匹配此次结果 内集卡作业 plan[{}]", m_pTaskMgr->m_planTruckNum));
			//与计划车号一样，发送计划车号
			if (!m_pTaskMgr->m_planTruckNum.empty())
			{
				string pmnin_crs = m_pTaskMgr->m_IARSInnerPmn.pmn;
				string pmnout_crs = m_pTaskMgr->m_IARSOuterPmn.pmn;
				GetTruckNo(pmnin_crs);
				GetTruckNo(pmnout_crs);
				if (m_pTaskMgr->m_planTruckNum == pmnin_crs)
				{
					m_pTaskMgr->m_TruckResultSend = true;
					sendPmn = m_pTaskMgr->m_IARSInnerPmn.pmn;
					m_pTaskMgr->AddTaskLog(console->info("[iars][pmn] 内集卡作业 iars内集卡结果[{}] 与计划车号[{}] 匹配", sendPmn, m_pTaskMgr->m_planTruckNum));
				}
				else if (m_pTaskMgr->m_planTruckNum == pmnout_crs)
				{
					m_pTaskMgr->m_TruckResultSend = true;
					sendPmn = m_pTaskMgr->m_IARSOuterPmn.pmn;
					m_pTaskMgr->AddTaskLog(console->info("[iars][pmn] 内集卡作业 iars外集卡结果[{}] 与计划车号[{}] 匹配", sendPmn, m_pTaskMgr->m_planTruckNum));
				}
			}
		}
		else if (m_pTaskMgr->m_truckType == PLCInfo::TRUCK_TYPE_OUTER)//外集卡
		{
			m_pTaskMgr->AddTaskLog(console->info("[iars][pmn]  开始匹配此次结果 外集卡作业 plan[{}]", m_pTaskMgr->m_planTruckNum));
			if (!m_pTaskMgr->m_planTruckNum.empty())
			{
				if (m_pTaskMgr->m_planTruckNum == m_pTaskMgr->m_IARSInnerPmn.pmn || m_pTaskMgr->m_planTruckNum == m_pTaskMgr->m_IARSOuterPmn.pmn)
				{
					m_pTaskMgr->m_TruckResultSend = true;
					sendPmn = m_pTaskMgr->m_planTruckNum;
					m_pTaskMgr->AddTaskLog(console->info("[iars][pmn]  外集卡作业 内集卡结果[{}] 或者外集卡结果[{}] 与计划车号[{}] 匹配 保留车号结果pmn[{}]",
						m_pTaskMgr->m_IARSInnerPmn.pmn, m_pTaskMgr->m_IARSOuterPmn.pmn, m_pTaskMgr->m_planTruckNum, sendPmn));
				}
			}
		}

		//有结果
		if (m_pTaskMgr->m_TruckResultSend && !sendPmn.empty())//使用结果不为空，是因为 流识别结果也会触发 m_TruckResultSend = true,从而导致这发送结果，把正确结果冲掉
		{
			GetTruckNo(sendPmn);
			m_pTaskMgr->AddTaskLog(console->info("[iars][pmn] 车号结果 [{}] 发送", sendPmn));

			g_mtxOCRToPlc.lock();
			g_OCRToPlc.strTruckNo = sendPmn;
			//保存本次作业结果及吊具类型
			m_pTaskMgr->m_prePmn.pmn = sendPmn;
			m_pTaskMgr->m_prePmn.spreaderMode = m_pTaskMgr->m_spreaderMode;

			if (m_pTaskMgr->m_CNResultSend)
			{
				g_OCRToPlc.resultType = OCRToPlc::RESULT_TYPE_ALL;
			}
			else
			{
				g_OCRToPlc.resultType = OCRToPlc::RESULT_TYPE_PMN;
			}
			unsigned char szWriteBuf[100] = {};

			string strWritePlc = g_OCRToPlc.to_string();
			int nWriteLen = g_OCRToPlc.SeriralToBuffer(szWriteBuf);
			g_mtxOCRToPlc.unlock();

			string strLog = console->info("[iars][pmn] UpdateRetToPlc {} {}", strWritePlc, BufToHex(szWriteBuf, nWriteLen));
			m_pTaskMgr->AddTaskLog(strLog);
			//WriteStringToTxt(m_pTaskMgr->m_strJobPath, strLog);
			AddMainLog(strLog);
			AddMainLog("---------------------------------------\n");
			WriteStringToTxt(m_pTaskMgr->m_TaskInfo.local_snapshot_location() + GetSysTimeString() + "_SendToPlc.txt", strLog);
		}

	}

	/*g_TruckNumMatchMutex.lock();
	TruckNumSend ts;
	ts.truckNum = pmn;
	ts.isSend = false;
	m_pTaskMgr->m_truckNumDeque.push_back(ts);
	m_pTaskMgr->AddTaskLog(console->info("[Compose][pmn] iars车顶号识别结果pmn[{}]放入队列 队列size:{}", pmn, m_pTaskMgr->m_truckNumDeque.size()));
	if (m_pTaskMgr->m_truckNumDeque.size() > 8)
	{
		for (auto iter = m_pTaskMgr->m_truckNumDeque.begin(); iter != m_pTaskMgr->m_truckNumDeque.end();)
		{
			iter = m_pTaskMgr->m_truckNumDeque.erase(iter);
			if (m_pTaskMgr->m_truckNumDeque.size() <= 8)
			{
				break;
			}
		}
	}

	m_pTaskMgr->m_truckNumInfoMap[pmn].Update(pmr->rect_pmn().confidence());
	if (m_pTaskMgr->m_truckNumInfoMap.size() > 8)
	{
		string pmn_;
		int64_t time_ = GetCurrentStamp64();
		for (auto item : m_pTaskMgr->m_truckNumInfoMap)
		{
			if (item.second.time < time_)
			{
				time_ = item.second.time;
				pmn_ = item.first;
			}
		}
		m_pTaskMgr->m_truckNumInfoMap.erase(pmn_);
	}
	g_TruckNumMatchMutex.unlock();*/


	for (auto&t: m_pTaskMgr->m_TaskInfo.ocr().pmr_arr())
	{
		if (t.pmr().pmn() == pmr->pmn())
		{
			if (pmr->rect_pmn().confidence() > t.pmr().rect_pmn().confidence())
			{
				m_pTaskMgr->AddTaskLog(console->info("更新车号结果[{}]置信度[{} -> {}]", pmr->pmn(), t.pmr().rect_pmn().confidence(), pmr->rect_pmn().confidence()));
				PMObject *pmro = (PMObject *)&t;
				pmro->mutable_pmr()->mutable_rect_pmn()->set_confidence(pmr->rect_pmn().confidence());
			}
			return true;
		}
	}
	PMObject *pmro = m_pTaskMgr->m_TaskInfo.mutable_ocr()->add_pmr_arr();
	pmro->set_group(group);
	pmro->mutable_pmr()->CopyFrom(*pmr);
	m_pTaskMgr->m_TaskInfo.set_pmn_result(1);
	m_pTaskMgr->AddTaskLog(console->info("保存车号结果[{}]置信度[{}]", pmr->pmn(), pmr->rect_pmn().confidence()));

	m_pTaskMgr->m_TruckNum = pmr->pmn();
	if (pmr->rect_pmn().confidence() > m_pTaskMgr->m_TruckNoPlateNum.truck_num_conf)
	{
		m_pTaskMgr->AddTaskLog(console->info("车顶号识别结果pmn[{}] conf[{}] 大于 之前车号置信度pmn[{}] conf[{}]替换",
			pmr->pmn(), pmr->rect_pmn().confidence(), m_pTaskMgr->m_TruckNoPlateNum.truck_num, m_pTaskMgr->m_TruckNoPlateNum.truck_num_conf));

	m_pTaskMgr->AddTaskLog(console->info("车顶号识别结果更新 旧pmn[{}] conf[{}] -> 新pmn[{}] conf[{}]",
		m_pTaskMgr->m_TruckNoPlateNum.truck_num, m_pTaskMgr->m_TruckNoPlateNum.truck_num_conf,
		pmr->pmn(), pmr->rect_pmn().confidence() ));//修改车顶号更新逻辑，高于一定置信度则取最新结果
		m_pTaskMgr->m_TruckNoPlateNum.truck_num = pmr->pmn();
		m_pTaskMgr->m_TruckNoPlateNum.truck_num_conf = pmr->rect_pmn().confidence();
	}
	else
	{
		m_pTaskMgr->AddTaskLog(console->info("车顶号识别结果pmn[{}] conf[{}] 小于 之前车号置信度pmn[{}] conf[{}] 无效",
			pmr->pmn(), pmr->rect_pmn().confidence(), m_pTaskMgr->m_TruckNoPlateNum.truck_num, m_pTaskMgr->m_TruckNoPlateNum.truck_num_conf));
	}

	return true;
}

bool ComposeResult::ComposeQR(int lane, const string &camInstallName, int group, QRResult& ret)
{
	if (ret.qr().empty())
	{
		m_pTaskMgr->AddTaskLog(console->info("[{}] 二维码结果为空", camInstallName));
		return false;
	}
	if (ret.qr().find("?") != -1)
	{
		m_pTaskMgr->AddTaskLog(console->info("[{}] 二维码结果[{}]无效", camInstallName, ret.qr()));
		return false;
	}
	for (auto&t : m_pTaskMgr->m_TaskInfo.ocr().pmr_arr())
	{
		if (t.pmr().pmn() == ret.qr())
		{
			m_pTaskMgr->AddTaskLog(console->info("[Compose] 更新二维码结果[{}]置信度[{} -> 100]", ret.qr(), t.pmr().rect_pmn().confidence()));
			PMObject *pmro = (PMObject *)&t;
			pmro->mutable_pmr()->mutable_rect_pmn()->set_confidence(100);
			// 二维码车号入样本池，fromQr=true：内集卡匹配成功时 StatusCode=1（来自二维码）
			m_pTaskMgr->m_mtxPmnItem.lock();
			m_pTaskMgr->AddPmnRet(lane, ret.qr(), 1.0f, true);
			m_pTaskMgr->m_mtxPmnItem.unlock();
			return true;
		}
	}
	PMObject *pmro = m_pTaskMgr->m_TaskInfo.mutable_ocr()->add_pmr_arr();
	pmro->set_group(group);
	pmro->mutable_pmr()->set_pmn(ret.qr());
	pmro->mutable_pmr()->mutable_rect_pmn()->CopyFrom(ret.rect_qr());
	pmro->mutable_pmr()->mutable_rect_pmn()->set_confidence(100);
	m_pTaskMgr->m_TaskInfo.set_pmn_result(1);
	m_pTaskMgr->AddTaskLog(console->info("[Compose] 保存二维码结果[{}]置信度[100]", ret.qr()));
	// 二维码车号入样本池，fromQr=true：内集卡匹配成功时 StatusCode=1（来自二维码）
	m_pTaskMgr->m_mtxPmnItem.lock();
	m_pTaskMgr->AddPmnRet(lane, ret.qr(), 1.0f, true);
	m_pTaskMgr->m_mtxPmnItem.unlock();

	return true;
}

void ComposeResult::PMRRules(PMResult & pmr)
{

}
int ComposeResult::CnmFile(std::string path)
{
	set<string> cnmSet;
	std::ifstream file(path);
	if (file)
	{
		string line;
		while (std::getline(file, line))
		{
			line.erase(remove(line.begin(), line.end(), ' '));
			if (line.length())
			{
				cnmSet.insert(line);
			}
		}
		file.close();
	}
	return cnmSet.size();
}
int ComposeResult::IsoFile(std::string path)
{
	set<string> isoSet;
	std::ifstream file(path);
	if (file)
	{
		string line;
		while (std::getline(file, line))
		{
			line.erase(remove(line.begin(), line.end(), ' '));
			if (line.length())
			{
				isoSet.insert(line);
			}
		}
		file.close();
	}
	return isoSet.size();
}
