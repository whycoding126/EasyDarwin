/*
	Copyright (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
/*
	File:       EasyLoginSession.cpp
	Contains:   Implementation of object defined in EasyLoginSession.h.
*/
#include "EasyLoginSession.h"
#include "EasyUtil.h"
#include <OSMemory.h>
#include <SocketUtils.h>
#include <OSArrayObjectDeleter.h>
#include <QTSSModuleUtils.h>

// EasyCMS IP
static char*            sEasyCMS_IP = NULL;
static char*            sDefaultEasyCMS_IP_Addr = "121.40.50.44";
// EasyCMS Port
static UInt16			sEasyCMSPort = 10000;
static UInt16			sDefaultEasyCMSPort = 10000;
// EasyCamera Serial
static char*            sEasy_Serial = NULL;
static char*            sDefaultEasy_Serial = "CAM00000001";
// EasyCamera Name
static char*            sEasy_Name = NULL;
static char*            sDefaultEasy_Name = "CAM001";
// EasyCamera Secret key
static char*            sEasy_Key = NULL;
static char*            sDefaultEasy_Key = "123456";
// EasyCamera tag name
static char*			sEasy_Tag = NULL;
static char*			sDefaultEasy_Tag = "CAMTag001";
// EasyCMS Keep-Alive Interval
static UInt32			sKeepAliveInterval = 30;
static UInt32			sDefKeepAliveInterval = 30;
// EasyCMS Upload snap Interval
static UInt32			sUploadSnapInterval = 180;
static UInt32			sDefaultUploadSnapInterval = 180;

// ��ʼ����ȡ�����ļ��и�������
void EasyLoginSession::Initialize(QTSS_ModulePrefsObject cmsModulePrefs)
{
	delete[] sEasyCMS_IP;
	sEasyCMS_IP = QTSSModuleUtils::GetStringAttribute(cmsModulePrefs, "easycms_ip", sDefaultEasyCMS_IP_Addr);

	QTSSModuleUtils::GetAttribute(cmsModulePrefs, "easycms_port", qtssAttrDataTypeUInt16, &sEasyCMSPort, &sDefaultEasyCMSPort, sizeof(sEasyCMSPort));

	delete[] sEasy_Serial;
	sEasy_Serial = QTSSModuleUtils::GetStringAttribute(cmsModulePrefs, "device_serial", sDefaultEasy_Serial);

	delete[] sEasy_Name;
	sEasy_Name = QTSSModuleUtils::GetStringAttribute(cmsModulePrefs, "device_name", sDefaultEasy_Name);

	delete[] sEasy_Key;
	sEasy_Key = QTSSModuleUtils::GetStringAttribute(cmsModulePrefs, "device_key", sDefaultEasy_Key);

	delete[] sEasy_Tag;
	sEasy_Tag = QTSSModuleUtils::GetStringAttribute(cmsModulePrefs, "device_tag", sDefaultEasy_Tag);

	QTSSModuleUtils::GetAttribute(cmsModulePrefs, "keep_alive_interval", qtssAttrDataTypeUInt32, &sKeepAliveInterval, &sDefKeepAliveInterval, sizeof(sKeepAliveInterval));

	QTSSModuleUtils::GetAttribute(cmsModulePrefs, "upload_snap_interval", qtssAttrDataTypeUInt32, &sUploadSnapInterval, &sDefaultUploadSnapInterval, sizeof(sUploadSnapInterval));

}

EasyLoginSession::EasyLoginSession()
	: Task(),
	fSocket(NEW TCPClientSocket(Socket::kNonBlockingSocketType)),
	fTimeoutTask(NULL),
	fInputStream(new HTTPRequestStream(fSocket)),
	fOutputStream(NULL),
	fSessionStatus(kSessionOffline),
	fState(kIdle),
	fRequest(NULL),
	fReadMutex(),
	fMutex(),
	fContentBufferOffset(0),
	fContentBuffer(NULL),
	fNoneACKMsgCount(0),
	fCSeq(1)
{
	this->SetTaskName("EasyLoginSession");

	UInt32 inAddr = SocketUtils::ConvertStringToAddr(sEasyCMS_IP);

	if (inAddr)
	{
		fSocket->Set(inAddr, sEasyCMSPort);
	}
	else
	{
		//connect default EasyCMS server
		fSocket->Set(SocketUtils::ConvertStringToAddr("www.easydss.com"), sEasyCMSPort);
	}

	fTimeoutTask = new TimeoutTask(this, sKeepAliveInterval * 1000);
	fOutputStream = new HTTPResponseStream(fSocket, fTimeoutTask);
	fTimeoutTask->RefreshTimeout();

}

EasyLoginSession::~EasyLoginSession()
{
	if (fSocket)
	{
		delete fSocket;
		fSocket = NULL;
	}
	if (fRequest)
	{
		delete fRequest;
		fRequest = NULL;
	}
	if (fContentBuffer)
	{
		delete[] fContentBuffer;
		fContentBuffer = NULL;
	}

	if (fTimeoutTask)
	{
		delete fTimeoutTask;
		fTimeoutTask = NULL;
	}

	if (fInputStream)
	{
		delete fInputStream;
		fInputStream = NULL;
	}

	if (fOutputStream)
	{
		delete fOutputStream;
		fOutputStream = NULL;
	}

}

void EasyLoginSession::cleanupRequest()
{
	if (fRequest != NULL)
	{
		// NULL out any references to the current request
		delete fRequest;
		fRequest = NULL;
	}

	//��շ��ͻ�����
	fOutputStream->ResetBytesWritten();
}

SInt64 EasyLoginSession::Run()
{
	//OSMutexLocker locker(&fMutex);

	OS_Error theErr = OS_NoErr;
	EventFlags events = this->GetEvents();

	while (true)
	{
		switch (fState)
		{
		case kIdle:
			{
				if (!isConnected())
				{
					// TCPSocketδ���ӵ����,���Ƚ��е�¼����
					if (doDSRegister() == QTSS_NoErr && sUploadSnapInterval >= sKeepAliveInterval)
					{
						doDSPostSnap();
					}
				}
				else
				{
					printf("EasyCamera EasyCMSSession ackcount -- %d \n", fNoneACKMsgCount);
					if (fNoneACKMsgCount > 3)
					{
						printf("ackmsgcount > 3 \n");
						return resetClientSocket();
					}

					// TCPSocket�����ӵ�����������־����¼�����
					if (events & Task::kStartEvent)
					{
						// �����ӣ���״̬Ϊ����,���½������߶���
						if (kSessionOffline == fSessionStatus)
						{
							if (doDSRegister() == QTSS_NoErr && sUploadSnapInterval >= sKeepAliveInterval)
							{
								doDSPostSnap();
							}
						}

					}

					if (events & Task::kReadEvent)
					{
						// �����ӣ�������Ϣ��Ҫ��ȡ(���ݻ��߶Ͽ�)
						fState = kReadingMessage;
					}

					if (events & Task::kTimeoutEvent)
					{
						// �����ӣ�����ʱ�䵽��Ҫ���ͱ����
						if (sUploadSnapInterval >= sKeepAliveInterval && fCSeq % (sUploadSnapInterval / sKeepAliveInterval) == 1)
						{
							printf("post snap \n");
							doDSPostSnap();
						}
						else
						{
							printf("register camera \n");
							doDSRegister();
						}

						fTimeoutTask->RefreshTimeout();
					}

					if (events & Task::kUpdateEvent && sUploadSnapInterval >= sKeepAliveInterval)
					{
						//�����ӣ����Ϳ��ո���
						doDSPostSnap();
					}
				}

				// �������Ϣ��Ҫ��������뷢������
				if (fOutputStream->GetBytesWritten() > 0)
				{
					fState = kSendingMessage;
				}

				// 
				if (kIdle == fState)
				{
					return 0;
				}

				break;
			}
		case kReadingMessage:
			{
				// ���������Ĵ洢��fInputStream��
				if ((theErr = fInputStream->ReadRequest()) == QTSS_NoErr)
				{
					//���RequestStream����QTSS_NoErr���ͱ�ʾ�Ѿ���ȡ��Ŀǰ���������������
					//���������ܹ���һ�����屨��Header���֣���Ҫ�����ȴ���ȡ...
					fSocket->GetSocket()->SetTask(this);
					fSocket->GetSocket()->RequestEvent(EV_RE);

					fState = kIdle;
					return 0;
				}

				if ((theErr != QTSS_RequestArrived) && (theErr != E2BIG) && (theErr != QTSS_BadArgument))
				{
					//Any other error implies that the input connection has gone away.
					// We should only kill the whole session if we aren't doing HTTP.
					// (If we are doing HTTP, the POST connection can go away)
					Assert(theErr > 0);
					// If we've gotten here, this must be an HTTP session with
					// a dead input connection. If that's the case, we should
					// clean up immediately so as to not have an open socket
					// needlessly lingering around, taking up space.
					Assert(!fSocket->GetSocket()->IsConnected());
					printf("reading message theErr == %d \n", theErr);
					return resetClientSocket();
				}

				// �������󳬹��˻�����������Bad Request
				if ((theErr == E2BIG) || (theErr == QTSS_BadArgument))
				{
					//����HTTP���ģ�������408
					//(void)QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientBadRequest, qtssMsgBadBase64);
					fState = kCleaningUp;
				}
				else
				{
					fState = kProcessingMessage;
				}
				break;
			}
		case kProcessingMessage:
			{
				// �������籨��
				Assert(fInputStream->GetRequestBuffer());
				Assert(fRequest == NULL);

				// ���ݾ��������Ĺ���HTTPRequest������
				fRequest = NEW HTTPRequest(&QTSServerInterface::GetServerHeader(), fInputStream->GetRequestBuffer());

				// ��շ��ͻ�����
				fOutputStream->ResetBytesWritten();

				Assert(theErr == QTSS_RequestArrived);

				QTSS_Error err = processMessage();

				if (err == QTSS_WouldBlock)
				{
					fSocket->GetSocket()->SetTask(this);
					fSocket->GetSocket()->RequestEvent(EV_RE);

					return 0;
				}

				// ÿһ���������Ӧ�����Ƿ�����ɣ������ֱ�ӽ��лظ���Ӧ
				if (fOutputStream->GetBytesWritten() > 0)
				{
					fState = kSendingMessage;
				}
				else
				{
					fState = kCleaningUp;
				}
				break;
			}
		case kSendingMessage:
			{
				theErr = fOutputStream->Flush();

				if (theErr == EAGAIN || theErr == EINPROGRESS)
				{
					//qtss_printf("EasyCMSSession::Run fOutputStream.Flush() theErr:%d \n", theErr);
					fSocket->GetSocket()->SetTask(this);
					fSocket->GetSocket()->RequestEvent(fSocket->GetEventMask());
					this->ForceSameThread();
					return 0;
				}
				else if (theErr != QTSS_NoErr)
				{
					// Any other error means that the client has disconnected, right?
					printf("sending message != noerr errno %d \n", theErr);
					Assert(!this->isConnected());
					return resetClientSocket();
				}

				++fNoneACKMsgCount;

				fState = kCleaningUp;
				break;
			}
		case kCleaningUp:
			{
				// Cleaning up consists of making sure we've read all the incoming Request Body
				// data off of the socket
				////if (this->GetRemainingReqBodyLen() > 0)
				////{
				////	err = this->DumpRequestData();

				////	if (err == EAGAIN)
				////	{
				////		fInputSocketP->RequestEvent(EV_RE);
				////		this->ForceSameThread();    // We are holding mutexes, so we need to force
				////									// the same thread to be used for next Run()
				////		return 0;
				////	}
				////}

				// clean up and wait for next run
				cleanupRequest();
				fState = kIdle;

				if (isConnected())
				{
					fSocket->GetSocket()->SetTask(this);
					fSocket->GetSocket()->RequestEvent(EV_RE | EV_WR);
				}
				return 0;
			}

		}
	}
}

QTSS_Error EasyLoginSession::processMessage()
{
	if (NULL == fRequest) return QTSS_BadArgument;

	QTSS_Error theErr = fRequest->Parse();
	if (theErr != QTSS_NoErr)
		return QTSS_BadArgument;

	//��ȡ����Content json���ݲ���
	StrPtrLen* lengthPtr = fRequest->GetHeaderValue(httpContentLengthHeader);
	StringParser theContentLenParser(lengthPtr);
	theContentLenParser.ConsumeWhitespace();
	UInt32 content_length = theContentLenParser.ConsumeInteger(NULL);

	//printf("EasyCMSSession::ProcessMessage Parse ContentLength:%d \n", content_length);

	if (content_length)
	{
		//qtss_printf("EasyCMSSession::ProcessMessage read content-length:%lu \n", content_length);
		// ���content��fContentBuffer��fContentBufferOffset�Ƿ���ֵ����,������ڣ�˵�������Ѿ���ʼ
		// ����content������,���������,������Ҫ��������ʼ��fContentBuffer��fContentBufferOffset
		if (fContentBuffer == NULL)
		{
			fContentBuffer = NEW char[content_length + 1];
			memset(fContentBuffer, 0, content_length + 1);
			fContentBufferOffset = 0;
		}

		UInt32 theLen = 0;
		// ��ȡHTTP Content��������
		theErr = fInputStream->Read(fContentBuffer + fContentBufferOffset, content_length - fContentBufferOffset, &theLen);
		Assert(theErr != QTSS_BadArgument);

		//printf("EasyCMSSession::ProcessMessage Read Length:%d (%d/%d) \n", theLen, fContentBufferOffset+theLen, content_length);

		if (theErr == QTSS_RequestFailed)
		{
			OSCharArrayDeleter charArrayPathDeleter(fContentBuffer);
			fContentBufferOffset = 0;
			fContentBuffer = NULL;

			return QTSS_RequestFailed;
		}

		//printf("EasyCMSSession::ProcessMessage() Add Len:%lu \n", theLen);

		if ((theErr == QTSS_WouldBlock) || (theLen < (content_length - fContentBufferOffset)))
		{
			//
			// Update our offset in the buffer
			fContentBufferOffset += theLen;

			Assert(theErr == QTSS_NoErr);
			return QTSS_WouldBlock;
		}

		Assert(theErr == QTSS_NoErr);

		// ������ɱ��ĺ���Զ�����Delete����
		OSCharArrayDeleter charArrayPathDeleter(fContentBuffer);

		//printf("EasyCMSSession::ProcessMessage Get Complete Msg:\n%s", fContentBuffer);

		fNoneACKMsgCount = 0;

		EasyProtocol protocol(fContentBuffer);
		int nNetMsg = protocol.GetMessageType();
		switch (nNetMsg)
		{
		case MSG_SD_REGISTER_ACK:
			{
				EasyMsgSDRegisterACK ack(fContentBuffer);

				qtss_printf("session id = %s\n", ack.GetBodyValue(EASY_TAG_SESSION_ID).c_str());
				qtss_printf("device serial = %s\n", ack.GetBodyValue(EASY_TAG_SERIAL).c_str());
			}
			break;
		case MSG_SD_POST_SNAP_ACK:
			{
				;
			}
			break;
		}
	}

	// ����fContentBuffer��fContentBufferOffsetֵ
	fContentBufferOffset = 0;
	fContentBuffer = NULL;

	return theErr;
}

//
// ���ԭ��ClientSocket,NEW���µ�ClientSocket
// ��ֵSocket���ӵķ�������ַ�Ͷ˿�
// ����fInputSocket��fOutputSocket��fSocket����
// ����״̬��״̬
int EasyLoginSession::resetClientSocket()
{
	qtss_printf("EasyLoginSession::ResetClientSocket()\n");

	cleanupRequest();

	if (fSocket)
	{
		fSocket->GetSocket()->Cleanup();
		delete fSocket;
		fSocket = NULL;
	}

	fSocket = NEW TCPClientSocket(Socket::kNonBlockingSocketType);
	UInt32 inAddr = SocketUtils::ConvertStringToAddr(sEasyCMS_IP);
	if (inAddr) fSocket->Set(inAddr, sEasyCMSPort);

	fInputStream->AttachToSocket(fSocket);
	fOutputStream->AttachToSocket(fSocket);
	fState = kIdle;

	fNoneACKMsgCount = 0;

    return 1000;
}

QTSS_Error EasyLoginSession::loginUMS()
{
	QTSS_Error theErr = QTSS_NoErr;

	QTSS_RoleParams params;
	params.cameraStateParams.outIsLogin = 0;

	UInt32 fCurrentModule = 0;
	UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kGetCameraStateRole);
	for (; fCurrentModule < numModules; fCurrentModule++)
	{
		QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kGetCameraStateRole, fCurrentModule);
		theErr = theModule->CallDispatch(Easy_GetCameraState_Role, &params);
	}

	if ((theErr == QTSS_NoErr) && params.cameraStateParams.outIsLogin)
	{
		EasyDevices channels;

		EasyNVR nvr(sEasy_Serial, sEasy_Name, sEasy_Key, sEasy_Tag, channels);

		EasyMsgDSRegisterREQ req(EASY_TERMINAL_TYPE_ARM, EASY_APP_TYPE_CAMERA, nvr, fCSeq++);
		string msg = req.GetMsg();

		StrPtrLen jsonContent((char*)msg.data());

		// ����HTTPע�ᱨ��,�ύ��fOutputStream���з���
		HTTPRequest httpReq(&QTSServerInterface::GetServerHeader(), httpRequestType);

		if (!httpReq.CreateRequestHeader()) return QTSS_Unimplemented;

		if (jsonContent.Len)
			httpReq.AppendContentLengthHeader(jsonContent.Len);

		char respHeader[2048] = { 0 };
		StrPtrLen* ackPtr = httpReq.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		fOutputStream->Put(respHeader);
		if (jsonContent.Len > 0)
			fOutputStream->Put(jsonContent.Ptr, jsonContent.Len);
	}

	return theErr;
}

QTSS_Error EasyLoginSession::postSnap()
{
	QTSS_Error theErr = QTSS_NoErr;

	QTSS_RoleParams params;
	params.cameraSnapParams.outSnapLen = 0;

	UInt32 fCurrentModule = 0;
	UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kGetCameraSnapRole);
	for (; fCurrentModule < numModules; fCurrentModule++)
	{
		qtss_printf("EasyCameraSource::Run::kGetCameraSnapRole\n");
		QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kGetCameraSnapRole, fCurrentModule);
		theErr = theModule->CallDispatch(Easy_GetCameraSnap_Role, &params);
	}

	if ((theErr == QTSS_NoErr) && (params.cameraSnapParams.outSnapLen > 0))
	{
		char szTime[32] = { 0, };
		EasyJsonValue body;
		body[EASY_TAG_SERIAL] = sEasy_Serial;

		string type = EasyProtocol::GetSnapTypeString(params.cameraSnapParams.outSnapType);

		body[EASY_TAG_TYPE] = type.c_str();
		body[EASY_TAG_TIME] = szTime;
		body[EASY_TAG_IMAGE] = EasyUtil::Base64Encode(reinterpret_cast<const char*>(params.cameraSnapParams.outSnapPtr), params.cameraSnapParams.outSnapLen);

		EasyMsgDSPostSnapREQ req(body, fCSeq++);
		string msg = req.GetMsg();

		//�����ϴ�����
		StrPtrLen jsonContent(const_cast<char*>(msg.data()));
		HTTPRequest httpReq(&QTSServerInterface::GetServerHeader(), httpRequestType);

		if (httpReq.CreateRequestHeader())
		{
			if (jsonContent.Len)
				httpReq.AppendContentLengthHeader(jsonContent.Len);

			//Push msg to OutputBuffer
			char respHeader[2048] = { 0 };
			StrPtrLen* ackPtr = httpReq.GetCompleteHTTPHeader();
			strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

			fOutputStream->Put(respHeader);
			if (jsonContent.Len > 0)
				fOutputStream->Put(jsonContent.Ptr, jsonContent.Len);
		}
	}

	return theErr;
}

