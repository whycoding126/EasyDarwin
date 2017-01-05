/*
	Copyright (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
/*
	File:       EasyLoginModule.cpp
	Contains:   Implementation of EasyLoginModule class.
*/

#include "EasyLoginModule.h"
#include "QTSSModuleUtils.h"
#include "QTSServerInterface.h"

//// STATIC DATA
//static QTSS_PrefsObject				sServerPrefs = NULL;	//服务器主配置
//static QTSS_ServerObject			sServer = NULL;	//服务器对象
//static QTSS_ModulePrefsObject		sEasyCMSModulePrefs = NULL;	//当前模块配置
//
//static EasyCMSSession*				sCMSSession = NULL; //唯一EasyCMSSession对象

static QTSS_ServerObject			sServer = NULL;	//服务器对象
static QTSS_PrefsObject				sServerPrefs = NULL;	//服务器主配置
static QTSS_ModulePrefsObject		sEasyLoginModulePrefs = NULL;	//当前模块配置

// FUNCTION PROTOTYPES
static QTSS_Error EasyLoginModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams);
static QTSS_Error Register_EasyLoginModule(QTSS_Register_Params* inParams);
static QTSS_Error Initialize_EasyLoginModule(QTSS_Initialize_Params* inParams);
static QTSS_Error RereadPrefs_EasyLoginModule();

// FUNCTION IMPLEMENTATIONS
QTSS_Error EasyLoginModule_Main(void* inPrivateArgs)
{
	return _stublibrary_main(inPrivateArgs, EasyLoginModuleDispatch);
}

QTSS_Error EasyLoginModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams)
{
	switch (inRole)
	{
	case QTSS_Register_Role:
		return Register_EasyLoginModule(&inParams->regParams);
	case QTSS_Initialize_Role:
		return Initialize_EasyLoginModule(&inParams->initParams);
	case QTSS_RereadPrefs_Role:
		return RereadPrefs_EasyLoginModule();
	}
	return QTSS_NoErr;
}

QTSS_Error Register_EasyLoginModule(QTSS_Register_Params* inParams)
{
	// Do role & attribute setup
	(void)QTSS_AddRole(QTSS_Initialize_Role);
	(void)QTSS_AddRole(QTSS_RereadPrefs_Role);

	// Tell the server our name!
	static char* sModuleName = "EasyLoginModule";
	::strcpy(inParams->outModuleName, sModuleName);

	return QTSS_NoErr;
}

QTSS_Error Initialize_EasyLoginModule(QTSS_Initialize_Params* inParams)
{
	// Setup module utils
	QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);

	// Setup global data structures
	sServerPrefs = inParams->inPrefs;
	sServer = inParams->inServer;
	sEasyLoginModulePrefs = QTSSModuleUtils::GetModulePrefsObject(inParams->inModule);

	//读取EasyCMSModule配置
	RereadPrefs_EasyLoginModule();

	//EasyCMSSession::Initialize(sEasyCMSModulePrefs);

	//创建并开始EasyCMSSession对象
	/*sCMSSession = new EasyCMSSession();
	sCMSSession->Signal(Task::kStartEvent);*/

	return QTSS_NoErr;
}

QTSS_Error RereadPrefs_EasyLoginModule()
{
	return QTSS_NoErr;
}