#include "stdafx.h"
#include "resource.h"
#include "autoroute.h"
#include <atlsync.h>
#include <atlutil.h>
#include <Ras.h>
#include <IPHlpApi.h>
#include "atl_ext.h"
#include "messages.h"

#pragma comment(lib, "Rasapi32")
#pragma comment(lib, "iphlpapi")
#pragma comment(lib, "ws2_32")


template <class T>
struct EmptyStruct : public T {
  EmptyStruct() { ZeroMemory(this, sizeof(T)); }
  operator T&() { return *this; }
};



class CAutoRouteModule : public ATL::CAtlServiceModuleT<CAutoRouteModule, IDS_SERVICENAME>, public ATL::IWorkerThreadClient {
  ATL::CWorkerThread<> connectionListenerThread;
  
  DWORD addRoute(PCTSTR ip, PCTSTR mask, PCTSTR gateway, DWORD interfaceIndex) {
    EmptyStruct<MIB_IPFORWARDROW> row;
    row.dwForwardProto = MIB_IPPROTO_NETMGMT;
    row.dwForwardIfIndex = interfaceIndex;
    row.dwForwardType = 4;
    row.dwForwardAge = INFINITE;
    row.dwForwardMetric1 = 1;
    row.dwForwardMetric2 = 
    row.dwForwardMetric3 = 
    row.dwForwardMetric4 = -1;
    row.dwForwardDest = inet_addr(ATL::CT2A(ip));
    row.dwForwardMask = inet_addr(ATL::CT2A(mask));
    row.dwForwardNextHop = inet_addr(ATL::CT2A(gateway));
    return CreateIpForwardEntry(&row);
  }

  void LogEventEx(const DWORD id, const WORD category, PCTSTR* strings, 
      const WORD count, WORD type) throw() {
    if (m_szServiceName) {
      HANDLE hEventSource = RegisterEventSource(NULL, m_szServiceName);
      if (hEventSource != NULL) {
        ReportEvent(hEventSource, 
          type,
          category,
          id,
          NULL,
          count,
          0,
          strings,
          NULL);
        DeregisterEventSource(hEventSource);
      }
    }
  }

  virtual HRESULT AddCommonRGSReplacements(IRegistrarBase* pRegistrar) throw() {
    pRegistrar->AddReplacement(_T("ServiceName"), m_szServiceName);
    ATL::CStringW types;
    types.Format(_T("%d"), EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE);
    pRegistrar->AddReplacement(_T("SupportedTypes"), types);
    return __super::AddCommonRGSReplacements(pRegistrar);
  }

  HRESULT Execute(DWORD_PTR dwParam, HANDLE hObject) {
    DWORD size = 0;
    PIP_ADAPTER_ADDRESSES ipadapters = (PIP_ADAPTER_ADDRESSES)HeapAlloc(GetProcessHeap(), 0, sizeof(IP_ADAPTER_ADDRESSES));
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersAddresses (AF_INET, 0, 0, ipadapters, &size)) {
      HeapFree(GetProcessHeap(), 0, ipadapters);
      ipadapters = (PIP_ADAPTER_ADDRESSES)HeapAlloc(GetProcessHeap(), 0, size);
    }
    DWORD error = GetAdaptersAddresses (AF_INET, 0, 0, ipadapters, &size);
    if (NO_ERROR != error) {
      return HRESULT_FROM_WIN32(error);
    }
    
    ATL::CModulePath modulePath;
    modulePath.RemoveFileSpec();
    for (PIP_ADAPTER_ADDRESSES item = ipadapters; item; item = item->Next) {
      // Try to open routes files
      ATL::CPath filePath(modulePath);
      filePath.Append(item->FriendlyName);
      filePath.AddExtension(_T(".ini"));
      if (filePath.FileExists()) {
        TCHAR ip[MAX_PATH];
        TCHAR gateway[MAX_PATH];
        TCHAR mask[MAX_PATH];
        const TCHAR sectionNameFormat[] = _T("Route.%d");
        ATL::CString sectionName;
        for (int i=1; ;++i) {
          sectionName.Format(sectionNameFormat, i);
          GetPrivateProfileString(sectionName, _T("IP"), _T(""), ip, MAX_PATH, filePath);
          GetPrivateProfileString(sectionName, _T("Gateway"), _T(""), gateway, MAX_PATH, filePath);
          GetPrivateProfileString(sectionName, _T("Mask"), _T("255.255.255.0"), mask, MAX_PATH, filePath);
          if (*ip && *gateway && *mask) {
            DWORD result = addRoute(ip, mask, gateway, item->IfIndex);
            if (NO_ERROR == result) {
              PCTSTR args[] = { ip, mask, gateway, item->FriendlyName, filePath };
              LogEventEx(ROUTE_SET, CATEGORY_ONE, args, sizeof(args) / sizeof(args[0]), EVENTLOG_SUCCESS);
              //LogEvent(_T("Route %s with MASK %s and Gateway %s added for %s using file \"%s\""), ip, mask, gateway, item->FriendlyName, filePath);
            } else {
              PCTSTR args[] = { ip, mask, gateway, item->FriendlyName, filePath, ATL::AtlErrorMessage(result) };
              LogEventEx(ROUTE_NOT_SET, CATEGORY_ONE, args, sizeof(args) / sizeof(args[0]), EVENTLOG_ERROR_TYPE);
              
              //CString message;
              //message.Format(_T("Could not add route %s with MASK %s and Gateway %s added for %s using file \"%s\":\nError: %s"), ip, mask, gateway, item->FriendlyName, filePath, AtlErrorMessage(result));
              //LogEventEx(0, message, EVENTLOG_ERROR_TYPE);
            }
          } else {
            break;
          }
        }
      }
    }

    HeapFree(GetProcessHeap(), 0, ipadapters);
    /*if (connections != &connection) {
      HeapFree(GetProcessHeap(), 0, connections);
    }*/
    return S_OK;
  }

  HRESULT CloseHandle(HANDLE handle) {
    return ::CloseHandle(handle) ? S_OK : E_FAIL;
  }

  ATL::CEvent killEvent;

public:
  CAutoRouteModule() {		
  }

  void OnStop() {
    connectionListenerThread.Shutdown();
    __super::OnStop();
  }

  HRESULT Run(int nShowCmd = SW_HIDE) throw() {
    connectionListenerThread.Initialize();
    HANDLE connectionEvent = CreateEvent(0, false, false, _T("ConnectionEvent"));
    connectionListenerThread.AddHandle(connectionEvent, this, 0);
    RasConnectionNotification(0, connectionEvent, RASCN_Connection);
    SetEvent(connectionEvent);
    return __super::Run(nShowCmd);
  }
  
  HRESULT InitializeSecurity() throw() {
    return S_OK;
  }

  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_AUTOROUTE, "{3B6FAF03-9205-41CE-84B5-56921CD61097}")
};

CAutoRouteModule _AtlModule;

extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd) {
  return _AtlModule.WinMain(nShowCmd);
}