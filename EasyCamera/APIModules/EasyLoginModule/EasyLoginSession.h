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

	// �����ж�Session Socket�Ƿ�������
	Bool16				isConnected() const { return fSocket->GetSocket()->IsConnected(); }

	void				cleanupRequest();

	// �豸ע�ᵽEasyCMS
	QTSS_Error			loginUMS();

	// �ϴ�����ͼƬ��EasyCMS
	QTSS_Error			postSnap();

	// ����HTTPRequest������
	QTSS_Error			processMessage();

	// ���ÿͻ��˲���
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

	// ΪCMSSessionר�Ž����������ݰ���ȡ�Ķ���
	HTTPRequestStream*  fInputStream;
	// ΪCMSSessionר�Ž����������ݰ����͵Ķ���
	HTTPResponseStream* fOutputStream;

	// ��ʼ��ʱΪNULL
	// ��ÿһ�����󷢳����߽�������ʱ,���п�������HTTPRequest���󲢽��д���
	// ÿһ��״̬�������ڴ������kIdle~kCleanUp�����̶���Ҫ����HTTPRequest����
	HTTPRequest*        fRequest;

	// ��ȡ���籨��ǰ����סSession��ֹ�����ȡ
	OSMutex             fReadMutex;

	// Session��
	OSMutex             fMutex;

	// �����ĵ�Content����
	char*				fContentBuffer;

	// �����ĵ�Content��ȡƫ����,�ڶ�ζ�ȡ������Content����ʱ�õ�
	UInt32				fContentBufferOffset;

	// send message count
	unsigned int		fNoneACKMsgCount;

	UInt32				fCSeq;

};

#endif //__EASY_LOGIN_SESSION_H__