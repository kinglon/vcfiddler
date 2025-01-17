#include "windows.h"
#include "winbase.h"
#include "stdafx.h"
#include "efd.h"
#include "recv.h"
#include "send.h"
#include "malloc.h"
#include <tchar.h> 
#include <time.h> 
#include <stdlib.h>
#include "malloc.h"
#include "string.h"



using namespace Fiddler;
using namespace System;
using namespace System::Collections::Generic;
using namespace System::Text;
using namespace System::IO;
using namespace System::Threading;
using namespace System::Runtime::InteropServices;
using namespace System::Security::Cryptography::X509Certificates;


RecvCallbackFun RecvCallback = NULL;
SendCallbackFun SendCallback = NULL;
EnableModifyResponseCallbackFun EnableModifyResponseCallback = NULL;
bool autoProxy = false;
bool startSSL = false;
//char *base64;
const char *delim = "|";//分隔符
char* certificateFile = nullptr;
char *upstreamGateway = nullptr;
const char* upstreamProxyServer = nullptr;
void Send_os(Session ^os);
void Recv_os(Session ^os);
void ResponseHeadersAvailable(Session^ os);
void onwebsocket(Object ^sender, WebSocketMessageEventArgs ^os);
//外部接口 - 关闭Fiddler
void _stdcall CloseFiddler()
{
	
	FiddlerApplication::Shutdown();
	
}

void _stdcall customCertificate(char* str) {
	certificateFile = str;
}

void _stdcall sslSetCert(char* str)
{
	String^ str2 = System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)str);
	FiddlerApplication::Prefs->SetStringPref("fiddler.certmaker.bc.cert", str2);
}



void _stdcall sslSetKey(char* str)
{
	String^ str2 = System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)str);
	FiddlerApplication::Prefs->SetStringPref("fiddler.certmaker.bc.key", str2);
}

char* _stdcall sslGetKey()
{
	char* key = (char*)(void*)Marshal::StringToHGlobalAnsi(FiddlerApplication::Prefs->GetStringPref("fiddler.certmaker.bc.key", ""));
	return key;
}

char* _stdcall sslGetCert()
{
	char* key = (char*)(void*)Marshal::StringToHGlobalAnsi(FiddlerApplication::Prefs->GetStringPref("fiddler.certmaker.bc.cert", ""));
	return key;
}

void _stdcall SetUpstreamGatewayTo(char* str) {
	upstreamGateway = str;
}

void _stdcall SetUpstreamProxyServer(const char* str) {
	upstreamProxyServer = str;
}

void _stdcall UpdateConfig(int iPort) {
	FiddlerCoreStartupSettings ^startupSettings;
	FiddlerCoreStartupSettingsBuilder ^c = gcnew FiddlerCoreStartupSettingsBuilder();
	if (upstreamGateway != nullptr) {
		c = c->SetUpstreamGatewayTo(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)upstreamGateway));
	}
	c = c->ListenOnPort((USHORT)iPort);
	c = c->AllowRemoteClients();
	//c = c->ChainToUpstreamGateway();
	c = c->MonitorAllConnections();
	// c = c->HookUsingPACFile();
	// c = c->CaptureLocalhostTraffic();
	c = c->OptimizeThreadPool();
	//c = c->SetUpstreamGatewayTo("116.115.208.114:4526");
	c = c->RegisterAsSystemProxy(); 
	if (autoProxy)
	{
		c = c->RegisterAsSystemProxy();
	}
	if (startSSL)
	{
		c = c->DecryptSSL();
	}
	startupSettings = c->Build();
	FiddlerApplication::Startup(startupSettings);
	
	FiddlerApplication::CreateProxyEndpoint(7777, true, "localhost");
}

//外部接口 - 初始化Fiddler   void _stdcall StartSSL(bool start)
 int _stdcall InitFiddler(int iPort,int recvcallback,int sendcallback, int enableModifyResponseCallback)
{
	 
	 //srand ((unsigned int)time(NULL));;
	 //Fiddler::CertMaker
	 Proxy ^oSecureEndpoint;
	 RecvCallback = (RecvCallbackFun)recvcallback;
	 SendCallback = (SendCallbackFun)sendcallback;
	 EnableModifyResponseCallback = (EnableModifyResponseCallbackFun)enableModifyResponseCallback;
	 int iSecureEndpointPort = 7777;
	 String ^sSecureEndpointHostname = "localhost";
	 CONFIG::IgnoreServerCertErrors = true;
	 FiddlerApplication::Prefs->SetBoolPref("fiddler.network.streaming.abortifclientaborts", true);
	 FiddlerCoreStartupSettings ^startupSettings;
	 FiddlerCoreStartupSettingsBuilder ^c = gcnew FiddlerCoreStartupSettingsBuilder();
	 
	 if (upstreamGateway != nullptr) {
		 c = c->SetUpstreamGatewayTo(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)upstreamGateway));
	 }
	 c = c->ListenOnPort((USHORT)iPort);
	 c = c->AllowRemoteClients();
	 //c = c->ChainToUpstreamGateway();
	 c = c->MonitorAllConnections();
	// c = c->HookUsingPACFile();
	// c = c->CaptureLocalhostTraffic();
	 c = c->OptimizeThreadPool();
	 //c = c->SetUpstreamGatewayTo("116.115.208.114:4526");
	 
	 if (autoProxy)
	 {
		 c = c->RegisterAsSystemProxy();
	 }
	 if (startSSL)
	 {
		 c = c->DecryptSSL();
	 }

	 startupSettings = c->Build();
	 //FiddlerApplication::oDefaultClientCertificate = gcnew X509Certificate("d:\\FiddlerRoot.cer");
	 FiddlerApplication::Startup(startupSettings);
	 
	 oSecureEndpoint = FiddlerApplication::CreateProxyEndpoint(iSecureEndpointPort, true, sSecureEndpointHostname);

	 
	 Fiddler::FiddlerApplication::BeforeRequest += gcnew SessionStateHandler(Send_os);//SessionStateHandler
	 Fiddler::FiddlerApplication::BeforeResponse += gcnew SessionStateHandler(Recv_os);
	 Fiddler::FiddlerApplication::ResponseHeadersAvailable += gcnew SessionStateHandler(ResponseHeadersAvailable);
	 //Fiddler::FiddlerApplication::OnWebSocketMessage += gcnew EventHandler<WebSocketMessageEventArgs^>(onwebsocket);//EventHandler<WebSocketMessageEventArgs^>^ value
	 return 1;
}

 //外部接口 - 删除证书
 bool _stdcall removeCerts()
 {
	 //打开证书存储区
	 System::Security::Cryptography::X509Certificates::X509Store^ certStore = gcnew System::Security::Cryptography::X509Certificates::X509Store(StoreName::Root, StoreLocation::LocalMachine);
	 certStore->Open(OpenFlags::ReadWrite);
	 //获取所有证书
	 System::Security::Cryptography::X509Certificates::X509Certificate2Collection^ test = certStore->Certificates;

	 //搜索关于FD的证书
	 System::Security::Cryptography::X509Certificates::X509Certificate2Collection^ Fiddler = test->Find(System::Security::Cryptography::X509Certificates::X509FindType::FindByIssuerName, "DO_NOT_TRUST_FiddlerRoot", true);
	 //删除之前的证书
	 for (int i = 0; i < Fiddler->Count; i++)
	 {
		 X509Certificate2^ oRootCert = Fiddler[i];
		 
		 certStore->Remove(oRootCert);
	 }
	 return true;

 }

 //外部接口 - 安装证书
 int _stdcall InstCert()
 {		

	 //X509Certificate^ oRootCert = gcnew X509Certificate("d:\\FiddlerRoot.cer");
	 X509Certificate2 ^oRootCert = CertMaker::GetRootCertificate();
	 FiddlerApplication::oDefaultClientCertificate = oRootCert;
	 System::Security::Cryptography::X509Certificates::X509Store^ certStore = gcnew System::Security::Cryptography::X509Certificates::X509Store(StoreName::Root, StoreLocation::LocalMachine);
	 
	 certStore->Open(OpenFlags::ReadWrite);
	 try
	 {
		 certStore->Add(oRootCert);
	 }
	 finally
	 {
		 certStore->Close();
	 }


	 if (!CertMaker::trustRootCert())
	 {		
		 return -1;
	 }
	 return 1;


 }

 //外部接口 - 创建root根证书 （需要依赖C***相关dll文件，易语言运行模式下记得copy到e_debug）
 int _stdcall CreateRootCert()
 {
	 
	 if (!CertMaker::createRootCert()) {
		 return 0;
	 }

	 
	 return 1;
 }

 //外部接口 - 检查根证书是否存在
 bool _stdcall rootCertExists()
 {
	 return CertMaker::rootCertExists();

 }

 //检查是否安装机器证书
 bool _stdcall rootCertIsMachineTrusted()
 {
	 return CertMaker::rootCertExists();
 }

 //检查是否安装root证书
  bool _stdcall rootCertIsTrusted()
 {
	  return CertMaker::rootCertIsTrusted();
 }

 //外部接口 - 设置自动运行代理
 void _stdcall AutoStartFiddlerProxy(bool start)
 {
	 autoProxy = start;
 }

 //外部接口 - 设置是否开启SSL抓包
 void _stdcall StartSSL(bool start)
 {
	 startSSL = start;
 }


 /*
*	@name websocket事件
*	@remake 当拦截到websocket消息数据时触发
*/
 void onwebsocket(Object ^sender, WebSocketMessageEventArgs ^os)
 {
	 void pw_data(WebSocketMessage ^ms, int *t);
	 WebSocketMessage ^message = os->oWSM;

	 int *p  = (int *)malloc(sizeof(int) * 10 + 1);
	 memset(p, 0, sizeof(int) * 10 + 1);
	 char* url = (char*)(void*)Marshal::StringToHGlobalAnsi(message->FrameType.ToString());
	 char* head = (char*)(void*)Marshal::StringToHGlobalAnsi(message->iCloseReason.ToString());
	 char* cookie = (char*)(void*)Marshal::StringToHGlobalAnsi(message->ID.ToString());
	 char* LocalProcess = (char*)(void*)Marshal::StringToHGlobalAnsi(message->IsFinalFrame.ToString());
	 char* postdata = (char*)(void*)Marshal::StringToHGlobalAnsi(message->PayloadAsString());
	 char* postdataRaw = (char*)malloc(message->PayloadAsBytes()->Length);
	 IntPtr Pptr(postdataRaw);
	 if (message->PayloadAsBytes()->Length > 0) {
		 Marshal::Copy(message->PayloadAsBytes(), 0, Pptr, message->PayloadAsBytes()->Length);
	 }
	 //
	 char* contenttype = (char*)(void*)Marshal::StringToHGlobalAnsi(message->IsOutbound.ToString());
	 
	 *(p) = (int)url;//指针地址1（1，5）
	 *(p + 1) = (int)head;//指针地址2（5，9）
	 *(p + 2) = (int)cookie;//指针地址3（9，13）
	 *(p + 3) = (int)LocalProcess;//指针地址4（13，17）
	 *(p + 4) = (int)postdata;//指针地址5（17，21）
	 *(p + 5) = (int)postdataRaw;//指针地址6（17，21）
	 *(p + 6) = (int)contenttype;//指针地址7（21，25）
	 *(p + 7) = (int)message->PayloadLength;//指针地址8（25，29）
	 *(p + 8) = (int)message->ID;//会话ID（29,33）
	 *(p + 9) = 2;//数据类型（29,33）
	// Callback(&os,(int)p);//(int)*p
	
	 free(postdataRaw);
	 Marshal::FreeHGlobal(IntPtr(postdata));
	 Marshal::FreeHGlobal(IntPtr(cookie));
	 Marshal::FreeHGlobal(IntPtr(head));
	 Marshal::FreeHGlobal(IntPtr(url));
	 Marshal::FreeHGlobal(IntPtr(LocalProcess));
	 Marshal::FreeHGlobal(IntPtr(contenttype));
	 
	 postdata = nullptr;
	 cookie = nullptr;
	 head = nullptr;
	 url = nullptr;
	 LocalProcess = nullptr;
	 free(p); 

 }

 /*
 *	@name 请求事件
 *	@remake 当拦截到所有HTTP请求链接时触发本函数
 */
 void Send_os(Session ^os)
 {
	 
	
	 //创建一个可以容纳100次处理事件的指针
	 //用于接收用户存放的拦截类型及数据
	 //可容纳100次处理，第一位是索引位，后面则是数量 
	 EFD_pmMessage* pmmessage = (EFD_pmMessage*)malloc(100 * 4);//可容纳100次处理，第一位是索引位，后面则是数量 
	 EFD_pmMessage pmIndex = *(pmmessage);
	 pmIndex.count = 0;
	 *(pmmessage) = pmIndex;

	 //指定证书文件
	 if (certificateFile != nullptr) {
		 os["https-Client-Certificate"] = System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)certificateFile);
	 }

	 if (upstreamProxyServer && strlen(upstreamProxyServer) > 0)
	 {
		 os["x-overrideGateway"] = System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)(char*)upstreamProxyServer);
	 }

	 //忽略未解密成功的URL
	 char* url_dz = (char*)(void*)Marshal::StringToHGlobalAnsi(os->fullUrl);
	 if (strstr(url_dz, ":443") > 0){Marshal::FreeHGlobal(IntPtr(url_dz)); return; }
	 Marshal::FreeHGlobal(IntPtr(url_dz));
	

	 //获取请求Request对象
	 //获取url,head,postBody等信息
	 Fiddler::ClientChatter ^request = os->oRequest;
	 char* url = (char*)(void*)Marshal::StringToHGlobalAnsi(os->fullUrl);
	 char* method = (char*)(void*)Marshal::StringToHGlobalAnsi(os->RequestMethod);
	 char* head = (char*)(void*)Marshal::StringToHGlobalAnsi(os->oRequest->headers->ToString());
	 char* cookie = (char*)(void*)Marshal::StringToHGlobalAnsi(request["Cookie"]);
	 char* LocalProcess = (char*)(void*)Marshal::StringToHGlobalAnsi(os->LocalProcess);
	 char *postdata = (char*)malloc(os->requestBodyBytes->Length);
	 char *postdataText = (char*)(void*)Marshal::StringToHGlobalAnsi(os->GetRequestBodyAsString());
	 IntPtr Pptr(postdata);
	 if (os->requestBodyBytes->Length > 0) {
		 Marshal::Copy(os->requestBodyBytes, 0, Pptr, os->requestBodyBytes->Length);
	 }
	 
	// char* contenttype = (char*)(void*)Marshal::StringToHGlobalAnsi(os->oResponse["Content-Type"]);

	 

	 if (SendCallback != NULL) {
		 SendCallback(pmmessage, url, head, method, cookie, (int)postdata, (unsigned int)os->requestBodyBytes->Length);
	 }
	
	 pmIndex = *(pmmessage);
	 if (pmIndex.count > 0) {

		 //os->bBufferResponse = true;
		 os->utilDecodeResponse();
		
	 }



	 for (int i = 1; i <= pmIndex.count; i++) {
		 EFD_pmMessage pm = (EFD_pmMessage) * (pmmessage + i);
		 switch (pm.type)
		 {
		 case SEND_RESET_POST_BYTES:
			 array<unsigned char>^ byte1;
			 byte1 = Convert::FromBase64String(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1));
			 os->RequestBody = byte1;
			 break;
		 case SEND_RESET_POST_STR:
			 os->utilSetRequestBody(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1));
			 break;
		 case SEND_RESET_URL:
			 os->fullUrl = System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1);
			 break;
		 case SEND_REPLACE_URL:
			 os->fullUrl = os->fullUrl->Replace(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1), System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata2));
			 break;
		 case SEND_REPLACE_POST:
			 os->utilReplaceInRequest(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1), System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata2));
			 break;
		 case SEND_RESET_COOKIE:
			 os->oRequest["Cookie"] = System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1);
			 break;
		 case SEND_RESET_HEADER:
			 os->oRequest->headers->Remove(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1));
			 os->oRequest->headers->Add(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1), System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata2));
		 break;
			 
		 }

	 }

	 if (pmIndex.count > 0) {
		 os->utilDecodeRequest();
	 }

	 Marshal::FreeHGlobal(IntPtr(url));
	 Marshal::FreeHGlobal(IntPtr(method));
	 Marshal::FreeHGlobal(IntPtr(head));
	 Marshal::FreeHGlobal(IntPtr(cookie));
	 Marshal::FreeHGlobal(IntPtr(LocalProcess));
	 Marshal::FreeHGlobal(IntPtr(postdata));
	 Marshal::FreeHGlobal(IntPtr(postdataText));
	 Marshal::FreeHGlobal(IntPtr(pmmessage));
	 //Marshal::FreeHGlobal(IntPtr(contenttype));

		
	// free(pmmessage);
	 //os->Abort();

	 
	
 }


 /*
 *	@name 响应事件
 *	@remake 当拦截到所有HTTP响应数据时触发本函数
 */
 void Recv_os(Session ^os)
 {
	 EFD_pmMessage *pmmessage = (EFD_pmMessage*)malloc(100 * 4);//可容纳100次处理，第一位是索引位，后面则是数量 
	 EFD_pmMessage pmIndex = *(pmmessage);
	 pmIndex.count = 0;
	 *(pmmessage) = pmIndex;

	 char* url_dz = (char*)(void*)Marshal::StringToHGlobalAnsi(os->fullUrl);
	 if (strstr(url_dz, ":443") > 0){Marshal::FreeHGlobal(IntPtr(url_dz)); return;}
	 Marshal::FreeHGlobal(IntPtr(url_dz));
	 Fiddler::ServerChatter ^response = os->oResponse;
	 Fiddler::ClientChatter ^request = os->oRequest;
	 char* url = (char*)(void*)Marshal::StringToHGlobalAnsi(os->fullUrl);
	 char* method = (char*)(void*)Marshal::StringToHGlobalAnsi(os->RequestMethod);
	 char* head = (char*)(void*)Marshal::StringToHGlobalAnsi(os->oResponse->headers->ToString());
	 char* cookie = (char*)(void*)Marshal::StringToHGlobalAnsi(request["Cookie"]);
	 char* LocalProcess = (char*)(void*)Marshal::StringToHGlobalAnsi(os->LocalProcess);
	 char * raw = (char*)malloc(os->ResponseBody->Length);
	 char* retHtml = (char*)(void*)Marshal::StringToHGlobalAnsi(os->GetResponseBodyAsString());
	 IntPtr Pptr(raw);
	 if (os->ResponseBody->Length > 0) {
		 Marshal::Copy(os->ResponseBody, 0, Pptr, os->ResponseBody->Length);
	 }
	 
	 if (RecvCallback != NULL) {		 
		 RecvCallback(pmmessage, url, method, head, cookie, (int)raw, (unsigned int)os->ResponseBody->Length);//(int)*p 更新
	 } 
	 
	 pmIndex = *(pmmessage);
	 if (pmIndex.count > 0) {
		 os->bBufferResponse = true;
		 os->utilDecodeResponse();
	 }

	 for (int i = 1; i<= pmIndex.count; i++) {
		 EFD_pmMessage pm = (EFD_pmMessage)*(pmmessage + i);
		 switch (pm.type)
		 {
		 case RECV_REPLACE_BODY:
			os->utilReplaceInResponse(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1), System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata2));
			break;
		 case RECV_RESET_BODY:
			 os->utilSetResponseBody(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1));
			 break;
		 case RECV_INSERT_BODY:
			 os->utilPrependToResponseBody(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1));
			 break;
		 case RECV_INSERT_COOKIE:
			 os->oResponse->headers->Add("Set-Cookie", System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1));
			 break;
		 case RECV_REMOVE_COOKIE:
			 os->oResponse->headers->Remove("Set-Cookie");
			 break;
		 case RECV_RESET_STATE:
			 os->oResponse->headers->SetStatus(int::Parse(System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata1)), System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)pm.pmdata2));
			 break;
		 }
		 
	 }
	  

	 if (pmIndex.count > 0) {
		 os->utilDecodeRequest();
	 }


	 Marshal::FreeHGlobal(IntPtr(url));
	 Marshal::FreeHGlobal(IntPtr(method));
	 Marshal::FreeHGlobal(IntPtr(head));
	 Marshal::FreeHGlobal(IntPtr(cookie));
	 Marshal::FreeHGlobal(IntPtr(LocalProcess));
	 Marshal::FreeHGlobal(IntPtr(retHtml));
	 Marshal::FreeHGlobal(IntPtr(pmmessage));
	 free(raw);
	 //free(pmmessage);
	 //os->Abort();
 }

 void ResponseHeadersAvailable(Session^ os)
 {	 
	 if (EnableModifyResponseCallback == nullptr)
	 {
		 return;
	 }

	 char* url =  (char*)(void*)Marshal::StringToHGlobalAnsi(os->fullUrl);
	 char* method = (char*)(void*)Marshal::StringToHGlobalAnsi(os->RequestMethod);
	 if (EnableModifyResponseCallback(url, method))
	 {
		 os->bBufferResponse = true;
	 }
	 Marshal::FreeHGlobal(IntPtr(url));
	 Marshal::FreeHGlobal(IntPtr(method));
 }

 //处理拦截websocket
 void pw_data(WebSocketMessage ^ms, int *t)
 {
	 for (int i = 0; i < 10; i++)
	 {
		 int cfo = i * 3;//控制位  = 0
		 char cft = *(t + cfo);//控制符 = 0
		 if (cft == 0)
		 {
			 break;
		 }

		 char* data1 = (char *)*(t + cfo + 1);//内存地址 = 5
		 char* data2 = (char *)*(t + cfo + 2);//内存地址 = 9
		 String^ str2 = System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)data2);
		 String^ str1 = System::Runtime::InteropServices::Marshal::PtrToStringAnsi((IntPtr)data1);
		 free(data2);
		 free(data1);
		 switch (cft)
		 {
			 case 15://控制符 = 1 代表替换字符串（websocket）
			 {
				 ms->SetPayload(ms->PayloadAsString()->Replace(str1, str2));
				 break;
			 }
			 case 16://控制符 = 1 设置新字符串（websocket）
			 {
				 array<unsigned char>^ byte1;
				 byte1 = Convert::FromBase64String(str1);
				 ms->SetPayload(str1);
				 break;
			 }
			  case 17://控制符 = 1 设置新字节集（websocket）
			  {
				  array<unsigned char> ^ byte1;
				  byte1 = Convert::FromBase64String(str1);
				  ms->SetPayload(byte1);
				  break;
				}
	
			
		
		 }



	 }

 }

