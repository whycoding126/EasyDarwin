#ifndef __EASY_LOGIN_SESSION_H__
#define __EASY_LOGIN_SESSION_H__

#include "Task.h"
#include "TimeoutTask.h"

#include "QTSServerInterface.h"
#include "OSHeaders.h"
#include "QTSS.h"
#include "EasyProtocol.h"

#include "HTTPRequestStream.h"
#include "HTTPResponseStream.h"
#include "HTTPRequest.h"

using namespace EasyDarwin::Protocol;
using namespace std;

class EasyLoginSession : public Task
{
public:
	EasyLoginSession();
	virtual ~EasyLoginSession();

	static void Initialize(QTSS_ModulePrefsObject inPrefs);

	enum
	{
		kSessionOffline = 0,
		kSessionOnline = 1
	};
	typedef UInt32		SessionStatus;

	SessionStatus		GetSessionStatus() const { return fSessionStatus; }
	void				SetSessionStatus(SessionStatus status) { fSessionStatus = status; }

private:
	virtual SInt64		Run();

	// 初步判断Session Socket是否已连接
	Bool16				isConnected() const { return fSocket->GetSocket()->IsConnected(); }

	void				cleanupRequest();

	// 设备注册到EasyCMS
	QTSS_Error			loginUMS();

	// 上传快照图片到EasyCMS
	QTSS_Error			postSnap();

	// 处理HTTPRequest请求报文
	QTSS_Error			processMessage();

	// 重置客户端参数
	int resetClientSocket();

private:
	enum
	{
		kIdle = 0,
		kReadingMessage = 1,
		kProcessingMessage = 2,
		kSendingMessage = 3,
		kCleaningUp = 4
	};
	UInt32				fState;

	SessionStatus		fSessionStatus;

	TimeoutTask*		fTimeoutTask;
	ClientSocket*		fSocket;

	// 为CMSSession专门进行网络数据包读取的对象
	HTTPRequestStream*  fInputStream;
	// 为CMSSession专门进行网络数据包发送的对象
	HTTPResponseStream* fOutputStream;

	// 初始化时为NULL
	// 在每一次请求发出或者接收命令时,都有可能生成HTTPRequest对象并进行处理
	// 每一次状态机流程在处理完成kIdle~kCleanUp的流程都需要清理HTTPRequest对象
	HTTPRequest*        fRequest;

	// 读取网络报文前先锁住Session防止重入读取
	OSMutex             fReadMutex;

	// Session锁
	OSMutex             fMutex;

	// 请求报文的Content部分
	char*				fContentBuffer;

	// 请求报文的Content读取偏移量,在多次读取到完整Content部分时用到
	UInt32				fContentBufferOffset;

	// send message count
	unsigned int		fNoneACKMsgCount;

	UInt32				fCSeq;

};

#endif //__EASY_LOGIN_SESSION_H__