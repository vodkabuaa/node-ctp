#include <thread>
#include <uv.h>
#include "uv_mduser.h"
#include "ThostFtdcMdApi.h"
#include "ThostFtdcUserApiDataType.h"

std::map<int, uv_async_t*> uv_mduser::async_map;
std::map<int, CbWrap*> uv_mduser::cb_map;

uv_mduser::uv_mduser(void) {
	iRequestID = 0;
	m_pApi = nullptr;
}

uv_mduser::~uv_mduser(void) {

}

void uv_mduser::Disposed() {
	m_pApi->RegisterSpi(nullptr);
	m_pApi->Release();
	m_pApi = nullptr;
	auto callback_it = cb_map.begin();
	while (callback_it != cb_map.end()) {
		delete callback_it->second;
		callback_it++;
	}

	auto uvasync_it = async_map.begin();
	while (uvasync_it != async_map.end()) {
		uv_close((uv_handle_s*)uvasync_it->second, NULL);
		uvasync_it++;
	}
	logger_cout("uv_mduser object destroyed");
}

int uv_mduser::On(int cb_type, void(*callback)(CbRtnField* cbResult)) {
	std::string log = "on function";
	auto it = async_map.find(cb_type);
	if (it != async_map.end()) {
		logger_cout(log.append(" event id").append(std::to_string(cb_type)).append(" register repeat").c_str());
		return 1;//Callback is defined before
	}

	uv_async_t* s_async = new uv_async_t();//析构函数中需要销毁
	uv_async_init(uv_default_loop(), s_async, completeCb);
	async_map[cb_type] = s_async;

	CbWrap* cb_wrap = new CbWrap();//析构函数中需要销毁
	cb_wrap->callback = callback;
	cb_map[cb_type] = cb_wrap;
	logger_cout(log.append(" event id").append(std::to_string(cb_type)).append(" register").c_str());
	return 0;
}

void uv_mduser::Connect(UVConnectField* pConnectField, void(*callback)(int, void*), int uuid) {
	UVConnectField* _pConnectField = new UVConnectField();
	memcpy(_pConnectField, pConnectField, sizeof(UVConnectField));
	this->invoke(_pConnectField, 0, T_CONNECT_RE, callback, uuid);//pConnectField函数外部销毁
}

void uv_mduser::ReqUserLogin(CThostFtdcReqUserLoginField *pReqUserLoginField, void(*callback)(int, void*), int uuid) {
	CThostFtdcReqUserLoginField *_pReqUserLoginField = new CThostFtdcReqUserLoginField();
	memcpy(_pReqUserLoginField, pReqUserLoginField, sizeof(CThostFtdcReqUserLoginField));
	this->invoke(_pReqUserLoginField,0, T_LOGIN_RE, callback, uuid);
}

void uv_mduser::ReqUserLogout(CThostFtdcUserLogoutField *pUserLogout, void(*callback)(int, void*), int uuid) {
	CThostFtdcUserLogoutField* _pUserLogout = new CThostFtdcUserLogoutField();
	memcpy(_pUserLogout, pUserLogout, sizeof(CThostFtdcUserLogoutField));
	this->invoke(_pUserLogout, 0, T_LOGOUT_RE, callback, uuid);
}

void uv_mduser::SubscribeMarketData(char *ppInstrumentID[], int nCount, void(*callback)(int, void*), int uuid) {
	char **_ppInstrumentID = new char*[nCount];
	for (int i = 0; i < nCount; i++) {
		memcpy(_ppInstrumentID + i, ppInstrumentID + i, sizeof(*(ppInstrumentID + i)));
	}
	this->invoke(_ppInstrumentID, nCount, T_SUBSCRIBE_MARKET_DATA_RE, callback, uuid);
}

void uv_mduser::UnSubscribeMarketData(char *ppInstrumentID[], int nCount, void(*callback)(int, void*), int uuid) {
	char **_ppInstrumentID = new char*[nCount];
	for (int i = 0; i < nCount; i++) {
		memcpy(_ppInstrumentID + i, ppInstrumentID + i, sizeof(*(ppInstrumentID + i)));
	}
	this->invoke(_ppInstrumentID, nCount, T_UNSUBSCRIBE_MARKET_DATA_RE, callback, uuid);
}

void uv_mduser::OnFrontConnected() {
	auto it = async_map.find(T_ON_CONNECT);
	if (it != async_map.end()) {
		CbRtnField* field = new CbRtnField();//调用完毕后需要销毁
		field->eFlag = T_ON_CONNECT;//FrontConnected
		it->second->data = field;//对象销毁后，指针清空
		uv_async_send(it->second);
	}
}

void uv_mduser::OnFrontDisconnected(int nReason) {
	logger_cout("fffffffffffffffffffffffffffffffffffffff");
	auto it = async_map.find(T_ON_DISCONNECTED);
	if (it != async_map.end()) {
		CbRtnField* field = new CbRtnField();//调用完毕后需要销毁
		field->eFlag = T_ON_DISCONNECTED;//FrontConnected
		field->nReason = nReason;
		it->second->data = field;//对象销毁后，指针清空
		uv_async_send(it->second);
	}
}

void uv_mduser::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
	CThostFtdcRspUserLoginField* _pRspUserLogin = nullptr;
	if (pRspUserLogin) {
		_pRspUserLogin = new CThostFtdcRspUserLoginField();
		memcpy(_pRspUserLogin, pRspUserLogin, sizeof(CThostFtdcRspUserLoginField));
	}
	pkg_senduv(T_ON_RSPUSERLOGIN, _pRspUserLogin, pRspInfo, nRequestID, bIsLast);
}

void uv_mduser::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
	CThostFtdcUserLogoutField* _pUserLogout = nullptr;
	if (pUserLogout) {
		_pUserLogout = new CThostFtdcUserLogoutField();
		memcpy(_pUserLogout, pUserLogout, sizeof(CThostFtdcUserLogoutField));
	}
	pkg_senduv(T_ON_RSPUSERLOGOUT, _pUserLogout, pRspInfo, nRequestID, bIsLast);
}

void uv_mduser::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
	CThostFtdcRspInfoField* _pRspInfo = nullptr;
	if (pRspInfo) {
		_pRspInfo = new CThostFtdcRspInfoField();
		memcpy(_pRspInfo, pRspInfo, sizeof(CThostFtdcRspInfoField));
	}
	pkg_senduv(T_ON_RSPERROR, _pRspInfo, pRspInfo, nRequestID, bIsLast);
}

void uv_mduser::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
	CThostFtdcSpecificInstrumentField* _pSpecificInstrument = nullptr;
	if (pSpecificInstrument) {
		_pSpecificInstrument = new CThostFtdcSpecificInstrumentField();
		memcpy(_pSpecificInstrument, pSpecificInstrument, sizeof(CThostFtdcSpecificInstrumentField));
	}
	pkg_senduv(T_ON_RSPSUBMARKETDATA, _pSpecificInstrument, pRspInfo, nRequestID, bIsLast);
}

void uv_mduser::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
	CThostFtdcSpecificInstrumentField* _pSpecificInstrument = nullptr;
	if (pSpecificInstrument) {
		_pSpecificInstrument = new CThostFtdcSpecificInstrumentField();
		memcpy(_pSpecificInstrument, pSpecificInstrument, sizeof(CThostFtdcSpecificInstrumentField));
	}
	pkg_senduv(T_ON_RSPUNSUBMARKETDATA, _pSpecificInstrument, pRspInfo, nRequestID, bIsLast);
}

void uv_mduser::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) {
	CThostFtdcDepthMarketDataField* _pDepthMarketData = nullptr;
	if (pDepthMarketData) {
		_pDepthMarketData = new CThostFtdcDepthMarketDataField();
		memcpy(_pDepthMarketData, pDepthMarketData, sizeof(CThostFtdcDepthMarketDataField));
	}
	pkg_senduv(T_ON_RTNDEPTHMARKETDATA, _pDepthMarketData, new CThostFtdcRspInfoField(), 0, 0);
}

///uv_queue_work 异步调用方法
void uv_mduser::_async(uv_work_t * work) {
	auto baton = static_cast<LookupCtpApiBaton*>(work->data);
	auto uv_mduser_obj = static_cast<uv_mduser*>(baton->uv_trader_obj);
	std::string log = "uv_mduser uv_queue async run,";
	switch (baton->fun) {
	case T_CONNECT_RE:
	{
						 UVConnectField* _pConnectF = static_cast<UVConnectField*>(baton->args);
						 uv_mduser_obj->m_pApi = CThostFtdcMdApi::CreateFtdcMdApi(_pConnectF->szPath);
						 uv_mduser_obj->m_pApi->RegisterSpi(uv_mduser_obj);			// 注册事件类
						 uv_mduser_obj->m_pApi->RegisterFront(_pConnectF->front_addr);
						 uv_mduser_obj->m_pApi->Init();
						 logger_cout(log.append("ftdc_mduser_api init successed").c_str());
						 break;
	}
	case T_LOGIN_RE:
	{
					   CThostFtdcReqUserLoginField *_pReqUserLoginField = static_cast<CThostFtdcReqUserLoginField*>(baton->args);
					   baton->nResult = uv_mduser_obj->m_pApi->ReqUserLogin(_pReqUserLoginField, baton->iRequestID);
					   logger_cout(log.append("ftdc_mduser_api ReqUserLogin run,the result:").append(std::to_string(baton->nResult)).c_str());
					   break;
	}
	case T_LOGOUT_RE:
	{
						CThostFtdcUserLogoutField* _pUserLogout = static_cast<CThostFtdcUserLogoutField*>(baton->args);
						baton->nResult = uv_mduser_obj->m_pApi->ReqUserLogout(_pUserLogout, baton->iRequestID);
						logger_cout(log.append("ftdc_mduser_api ReqUserLogout run,the result:").append(std::to_string(baton->nResult)).c_str());

						break;
	}
	case T_SUBSCRIBE_MARKET_DATA_RE:
	{
									   char ** _ppInstrumentID = static_cast<char **>(baton->args);	 									   
									   baton->nResult = uv_mduser_obj->m_pApi->SubscribeMarketData(_ppInstrumentID, baton->nCount);
									   logger_cout(log.append("ftdc_mduser_api SubscribeMarketData run,the result:").append(std::to_string(baton->nResult)).c_str());
									   break;
	}
	case T_UNSUBSCRIBE_MARKET_DATA_RE:
	{
										 char ** _ppInstrumentID = static_cast<char **>(baton->args);
										 baton->nResult = uv_mduser_obj->m_pApi->UnSubscribeMarketData(_ppInstrumentID, baton->nCount);
										 logger_cout(log.append("ftdc_mduser_api UnSubscribeMarketData run,the result:").append(std::to_string(baton->nResult)).c_str());

										 break;
	}
	}
}
///uv_queue_work 方法完成回调
void uv_mduser::_completed(uv_work_t * work, int) {
	std::string df = "fucku ";
	auto baton = static_cast<LookupCtpApiBaton*>(work->data);
	baton->callback(baton->nResult, baton);
	if (baton->fun == T_SUBSCRIBE_MARKET_DATA_RE || baton->fun == T_UNSUBSCRIBE_MARKET_DATA_RE) {
		char** idArray = static_cast<char**>(baton->args);
		for (int i = 0; i < baton->nCount; i++) { 		 
			delete *(idArray + i);			 
		}
	}
	delete baton->args;
	delete baton;
}
///uv_async_init 服务器消息回调
void uv_mduser::completeCb(uv_async_t* handle, int) {
	CbRtnField* cbTrnField = (CbRtnField*)handle->data;	 	
	auto it = cb_map.find(cbTrnField->eFlag);
	if (it != cb_map.end()) {
		cb_map[cbTrnField->eFlag]->callback(cbTrnField);
	}
	if (cbTrnField->rtnField)
		delete cbTrnField->rtnField;
	if (cbTrnField->rspInfo)
		delete cbTrnField->rspInfo;
	delete cbTrnField;
	handle->data = nullptr;
}

void uv_mduser::invoke(void* field, int count, int ret, void(*callback)(int, void*), int uuid) {
	auto baton = new LookupCtpApiBaton();//完成函数中需要销毁
	baton->work.data = baton;
	baton->uv_trader_obj = this;
	baton->callback = callback;
	baton->args = field;
	baton->fun = ret;
	baton->iRequestID = ++iRequestID;
	baton->uuid = uuid;
	baton->nCount = count;
	std::string head = "uv_mduser invoke function uuid:";
	logger_cout(head.append(std::to_string(uuid)).append(",requestid:").append(std::to_string(baton->iRequestID)).c_str());
	uv_queue_work(uv_default_loop(), &baton->work, _async, _completed);
}

void uv_mduser::pkg_senduv(int event_type, void* _stru, CThostFtdcRspInfoField *pRspInfo_org, int nRequestID, bool bIsLast) {
	std::string log = "ftdc_mduser_api callback,event type:";
	logger_cout(log.append(std::to_string(event_type)).append(",requestid:").append(std::to_string(nRequestID)).append(",islast:").append(std::to_string(bIsLast)).c_str());
	auto it = async_map.find(event_type);
	if (it != async_map.end()) {
		CThostFtdcRspInfoField* _pRspInfo = nullptr;
		if (pRspInfo_org) {
			_pRspInfo = new CThostFtdcRspInfoField();
			memcpy(_pRspInfo, pRspInfo_org, sizeof(CThostFtdcRspInfoField));		
		}
	 
		CbRtnField* field = new CbRtnField();
		field->eFlag = event_type;
		field->rtnField = _stru;
		field->rspInfo =  (void*)_pRspInfo;			
		field->nRequestID = nRequestID;
		field->bIsLast = bIsLast;
		it->second->data = field;  		
		uv_async_send(it->second);		
	}
}