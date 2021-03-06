#include "DesktopToastANE.h"
#include <sstream>
#include <iostream>
#include <windows.h>
#include <conio.h>

#include "FlashRuntimeExtensions.h"
bool isSupportedInOS = true;
std::string pathSlash = "\\";

#include "../include/ANEhelper.h"
#include <string>
#include <vector>
#include "json.hpp"
//wchar_t const* AppId_t;
wchar_t const* appShortcut;
DWORD windowID;
HWND _hwnd;
HWND _hEdit;
FREContext dllContext;

std::wstring s2ws(const std::string& s) {
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

std::string wcharToString(const wchar_t* arg) {
	using namespace std;
	std::wstring ws(arg);
	std::string str(ws.begin(), ws.end());
	return str;
}

/*****************************************************************************/

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <SDKDDKVer.h>
#include <Windows.h>
#include <Psapi.h>
#include <strsafe.h>
#include <ShObjIdl.h>
#include <Shlobj.h>
#include <Pathcch.h>
#include <propvarutil.h>
#include <propkey.h>
#include <wrl.h>
#include <wrl\wrappers\corewrappers.h>
#include <windows.ui.notifications.h>
#include "NotificationActivationCallback_TR.h"
#include "StringReferenceWrapper.h"

//  Name:     System.AppUserModel.ToastActivatorCLSID -- PKEY_AppUserModel_ToastActivatorCLSID
//  Type:     Guid -- VT_CLSID
//  FormatID: {9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3}, 26
//  
//  Used to CoCreate an INotificationActivationCallback interface to notify about toast activations.
EXTERN_C const PROPERTYKEY DECLSPEC_SELECTANY PKEY_AppUserModel_ToastActivatorCLSID = { { 0x9F4C2855, 0x9F79, 0x4B39,{ 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 } }, 26 };

using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::UI::Notifications;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

struct CoTaskMemStringTraits {
	typedef PWSTR Type;
	inline static bool Close(_In_ Type h) throw() { ::CoTaskMemFree(h); return true; }
	inline static Type GetInvalidValue() throw() { return nullptr; }
};
typedef HandleT<CoTaskMemStringTraits> CoTaskMemString;

wchar_t AppId[] = L"com.tuarua.DesktopToasts";



// For the app to be activated from Action Center, it needs to provide a COM server to be called
// when the notification is activated.  The CLSID of the object needs to be registered with the
// OS via its shortcut so that it knows who to call later.

/*
class DECLSPEC_UUID("23A5B06E-20BB-4E7E-A0AC-6982ED6A6041") NotificationActivator WrlSealed
	: public RuntimeClass < RuntimeClassFlags<ClassicCom>,
	INotificationActivationCallback > WrlFinal*/
class DECLSPEC_UUID("23A5B06E-20BB-4E7E-A0AC-6982ED6A6041") NotificationActivator
	WrlFinal : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, INotificationActivationCallback, FtmBase>
{
public:
	virtual HRESULT STDMETHODCALLTYPE Activate(
		_In_ LPCWSTR appUserModelI,
		_In_ LPCWSTR invokedArgs,
		_In_reads_(dataCount) const NOTIFICATION_USER_INPUT_DATA* data,
		ULONG dataCount) 
	{

		if (invokedArgs == nullptr) {
			// Start my app or just do nothing because COM started the app already.
		} else {
			using json = nlohmann::json;
			json j;
			json j2;
			j["arguments"] = wcharToString(invokedArgs);
			if (dataCount > 0) {
				for (size_t i = 0; i < dataCount; i++) {
					json j3;
					j3["key"] = wcharToString(data[i].Key);
					j3["value"] = wcharToString(data[i].Value);
					j2.push_back(j3);

				}
			}
			j["data"] = j2;

			std::string evnt = "Toast.Clicked";
			FREDispatchStatusEventAsync(dllContext, (uint8_t*)j.dump().c_str(), (const uint8_t*)evnt.c_str());
		}

		LRESULT result = ::SendMessage(_hEdit, WM_SETTEXT, reinterpret_cast<WPARAM>(nullptr), reinterpret_cast<LPARAM>(L"The user clicked on the toast."));


		return S_OK;
	}

	

};
CoCreatableClass(NotificationActivator);


_Use_decl_annotations_
HRESULT InstallShortcut(PCWSTR shortcutPath, PCWSTR exePath) {
	ComPtr<IShellLink> shellLink;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
	if (SUCCEEDED(hr)) {
		hr = shellLink->SetPath(exePath);
		if (SUCCEEDED(hr)) {
			ComPtr<IPropertyStore> propertyStore;

			hr = shellLink.As(&propertyStore);
			if (SUCCEEDED(hr)) {
				PROPVARIANT propVar;
				propVar.vt = VT_LPWSTR;
				propVar.pwszVal = const_cast<PWSTR>(AppId); // for _In_ scenarios, we don't need a copy
				hr = propertyStore->SetValue(PKEY_AppUserModel_ID, propVar);
				if (SUCCEEDED(hr)) {
					propVar.vt = VT_CLSID;
					propVar.puuid = const_cast<CLSID*>(&__uuidof(NotificationActivator));
					hr = propertyStore->SetValue(PKEY_AppUserModel_ToastActivatorCLSID, propVar);
					if (SUCCEEDED(hr)) {
						hr = propertyStore->Commit();
						if (SUCCEEDED(hr)) {
							ComPtr<IPersistFile> persistFile;
							hr = shellLink.As(&persistFile);
							if (SUCCEEDED(hr))
								hr = persistFile->Save(shortcutPath, TRUE);
						}
					}
				}
			}
		}
	}
	return hr;
}


_Use_decl_annotations_
HRESULT RegisterComServer(PCWSTR exePath) {
	// We don't need to worry about overflow here as ::GetModuleFileName won't
	// return anything bigger than the max file system path (much fewer than max of DWORD).
	DWORD dataSize = static_cast<DWORD>((::wcslen(exePath) + 1)  * sizeof(WCHAR));

	// In this sample, the app UI is registered to launch when the COM callback is needed.
	// Other options might be to launch a background process instead that then decides to launch
	// the UI if needed by that particular notification.
	return HRESULT_FROM_WIN32(::RegSetKeyValue(
		HKEY_CURRENT_USER,
		LR"(SOFTWARE\Classes\CLSID\{23A5B06E-20BB-4E7E-A0AC-6982ED6A6041}\LocalServer32)",
		nullptr,
		REG_SZ,
		reinterpret_cast<const BYTE*>(exePath),
		dataSize));
}

// In order to display toasts, a desktop application must have a shortcut on the Start menu.
// Also, an AppUserModelID must be set on that shortcut.
//
// For the app to be activated from Action Center, it needs to register a COM server with the OS
// and register the CLSID of that COM server on the shortcut.
//
// The shortcut should be created as part of the installer. The following code shows how to create
// a shortcut and assign the AppUserModelID and ToastActivatorCLSID properties using Windows APIs.
//
// Included in this project is a wxs file that be used with the WiX toolkit
// to make an installer that creates the necessary shortcut. One or the other should be used.
//
// This sample doesn't clean up the shortcut or COM registration.

HRESULT RegisterAppForNotificationSupport() {
	CoTaskMemString appData;
	wchar_t shortcutPath[MAX_PATH];
	DWORD charWritten = GetEnvironmentVariable(L"APPDATA", shortcutPath, MAX_PATH);
	HRESULT hr = charWritten > 0 ? S_OK : E_INVALIDARG;

	if (SUCCEEDED(hr)) {
		errno_t concatError = wcscat_s(shortcutPath, ARRAYSIZE(shortcutPath), appShortcut);
		hr = concatError == 0 ? S_OK : E_INVALIDARG;
		if (SUCCEEDED(hr)) {
			DWORD attributes = ::GetFileAttributes(shortcutPath);
			bool fileExists = attributes < 0xFFFFFFF;
			if (!fileExists) {
				wchar_t exePath[MAX_PATH];
				DWORD charWritten = ::GetModuleFileName(nullptr, exePath, ARRAYSIZE(exePath));
				hr = charWritten > 0 ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
				if (SUCCEEDED(hr)) {
					hr = InstallShortcut(shortcutPath, exePath);
					if (SUCCEEDED(hr))
						hr = RegisterComServer(exePath);
				}
			}
		}
	}
	return hr;
}


// Register activator for notifications
HRESULT RegisterActivator() {
	// Module<OutOfProc> needs a callback registered before it can be used.
	// Since we don't care about when it shuts down, we'll pass an empty lambda here.
	Module<OutOfProc>::Create([] {});

	// If a local server process only hosts the COM object then COM expects
	// the COM server host to shutdown when the references drop to zero.
	// Since the user might still be using the program after activating the notification,
	// we don't want to shutdown immediately.  Incrementing the object count tells COM that
	// we aren't done yet.
	Module<OutOfProc>::GetModule().IncrementObjectCount();

	return Module<OutOfProc>::GetModule().RegisterObjects();
}

// Unregister our activator COM object
void UnregisterActivator() {
	Module<OutOfProc>::GetModule().UnregisterObjects();
	Module<OutOfProc>::GetModule().DecrementObjectCount();
}


// Create the toast XML from a template
_Use_decl_annotations_
HRESULT CreateToastXml(_Outptr_ IXmlDocument** inputXml, const wchar_t *xmlString) {
	HStringReference toastXML(xmlString);
	ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocumentIO> xmlDocument;
	HRESULT hr = Windows::Foundation::ActivateInstance(StringReferenceWrapper(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(), &xmlDocument);
	if (SUCCEEDED(hr)) {
		hr = xmlDocument->LoadXml(toastXML.Get());
		if (SUCCEEDED(hr))
			hr = xmlDocument.CopyTo(inputXml);
	}
	return hr;
}


// Create and display the toast
_Use_decl_annotations_
HRESULT CreateToast(IToastNotificationManagerStatics* toastManager, IXmlDocument* xml) {
	ComPtr<IToastNotifier> notifier;
	HRESULT hr = toastManager->CreateToastNotifierWithId(HStringReference(AppId).Get(), &notifier);
	if (SUCCEEDED(hr)) {
		ComPtr<IToastNotificationFactory> factory;
		hr = Windows::Foundation::GetActivationFactory(
			HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(),
			&factory);
		if (SUCCEEDED(hr)) {
			ComPtr<IToastNotification> toast;
			hr = factory->CreateToastNotification(xml, &toast);
			if (SUCCEEDED(hr)) {
				// Register the event handlers
				//
				// These handlers are called asynchronously.  This sample doesn't handle the
				// the fact that these events could be raised after the app object has already
				// been decontructed.
				EventRegistrationToken activatedToken, dismissedToken, failedToken;

				using namespace ABI::Windows::Foundation;


				hr = toast->add_Activated(Callback < Implements < RuntimeClassFlags<ClassicCom>,
					ITypedEventHandler<ToastNotification*, IInspectable* >> >(
						[](IToastNotification*, IInspectable* e)
				{

					// When the user clicks or taps on the toast, the registered
					// COM object is activated, and the Activated event is raised.
					// There is no guarantee which will happen first. If the COM
					// object is activated first, then this message may not show.
					//DesktopToastsApp::GetInstance()->SetMessage(L"The user clicked on the toast. Yep");

					//can get called twice, first from Activator or vice versa

					//std::string msg = "";
					//std::string evnt = "Toast.Clicked";
					//FREDispatchStatusEventAsync(dllContext, (uint8_t*)msg.c_str(), (const uint8_t*)evnt.c_str());
					
					return S_OK;
				}).Get(),
					&activatedToken);


				if (SUCCEEDED(hr)) {
					hr = toast->add_Dismissed(Callback < Implements < RuntimeClassFlags<ClassicCom>,
						ITypedEventHandler<ToastNotification*, ToastDismissedEventArgs* >> >(
							[](IToastNotification*, IToastDismissedEventArgs* e) {
						ToastDismissalReason reason;
						if (SUCCEEDED(e->get_Reason(&reason))) {

							std::string evnt;
							std::string msg = "";

							switch (reason) {
							case ToastDismissalReason_ApplicationHidden:
								evnt = "Toast.Hidden";
								break;
							case ToastDismissalReason_UserCanceled:
								evnt = "Toast.Dismissed";
								break;
							case ToastDismissalReason_TimedOut:
								evnt = "Toast.TimedOut";
								break;
							default:
								evnt = "Toast.NotActivated";
								break;
							}

							FREDispatchStatusEventAsync(dllContext, (uint8_t*)msg.c_str(), (const uint8_t*)evnt.c_str());

						}
						return S_OK;
					}).Get(),
						&dismissedToken);

					if (SUCCEEDED(hr)) {
						hr = toast->add_Failed(Callback < Implements < RuntimeClassFlags<ClassicCom>,
							ITypedEventHandler<ToastNotification*, ToastFailedEventArgs* >> >(
								[](IToastNotification*, IToastFailedEventArgs* /*e */) {

							std::string evnt = "Toast.Error";
							std::string msg = "";
							FREDispatchStatusEventAsync(dllContext, (uint8_t*)msg.c_str(), (const uint8_t*)evnt.c_str());
							return S_OK;
						}).Get(),
							&failedToken);

						if (SUCCEEDED(hr)) {
							hr = notifier->Show(toast.Get());
						}
					}
				}

			}
		}
	}
	return hr;
}

// Display the toast using classic COM. Note that is also possible to create and
// display the toast using the new C++ /ZW options (using handles, COM wrappers, etc.)
HRESULT DisplayToast(const wchar_t *xmlString) {
	ComPtr<IToastNotificationManagerStatics> toastStatics;
	HRESULT hr = Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),&toastStatics);

	if (SUCCEEDED(hr)) {
		ComPtr<IXmlDocument> toastXml;
		hr = CreateToastXml(&toastXml, xmlString);
		if (SUCCEEDED(hr))
			hr = CreateToast(toastStatics.Get(), toastXml.Get());
	}
	return hr;
}



/*****************************************************************************/




extern "C" {
	int logLevel = 1;
	extern void trace(std::string msg) {
		//if (logLevel > 0)
			FREDispatchStatusEventAsync(dllContext, (uint8_t*)msg.c_str(), (const uint8_t*) "TRACE");
	}

	FREObject show(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]) {
		using namespace std;
		string xmlStr = getStringFromFREObject(argv[0]);

		wstring widestr = s2ws(xmlStr);
		const wchar_t* xmlString = widestr.c_str();

		int ret = DisplayToast(xmlString);
		return NULL;
	}

	FREObject init(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]) {
		using namespace std;
		string appIdStr = getStringFromFREObject(argv[0]);
		string appNameStr = getStringFromFREObject(argv[1]);
		string appShortcutStr = "\\Microsoft\\Windows\\Start Menu\\Programs\\" + appNameStr + ".lnk";
		wstring widestr = s2ws(appShortcutStr);
		appShortcut = widestr.c_str();

		wstring widestr1 = s2ws(appNameStr);
		std::copy(std::begin(widestr1), std::end(widestr1), AppId);

		HRESULT hr = RegisterAppForNotificationSupport();
		if (SUCCEEDED(hr))
			hr = RegisterActivator();

		return NULL;
	}
	
	BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam) {
		GetWindowThreadProcessId(hwnd, &windowID);
		if (windowID == lParam) {
			_hwnd = hwnd;
			return false;
		}
		return true;
	}

	void contextInitializer(void* extData, const uint8_t* ctxType, FREContext ctx, uint32_t* numFunctionsToSet, const FRENamedFunction** functionsToSet) {
		
		DWORD processID = GetCurrentProcessId();
		EnumWindows(EnumProc, processID);

		static FRENamedFunction extensionFunctions[] = {
			{ (const uint8_t*) "init",NULL, &init }
			,{ (const uint8_t*) "show",NULL, &show }

		};

		*numFunctionsToSet = sizeof(extensionFunctions) / sizeof(FRENamedFunction);
		*functionsToSet = extensionFunctions;
		dllContext = ctx;
	}


	void contextFinalizer(FREContext ctx) {
		return;
	}

	void TRDTTExtInizer(void** extData, FREContextInitializer* ctxInitializer, FREContextFinalizer* ctxFinalizer) {
		*ctxInitializer = &contextInitializer;
		*ctxFinalizer = &contextFinalizer;
	}

	void TRDTTExtFinizer(void* extData) {
		FREContext nullCTX;
		nullCTX = 0;
		UnregisterActivator();
		contextFinalizer(nullCTX);
		return;
	}
}
