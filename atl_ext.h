#pragma once

#include <typeinfo>

#ifndef ATLTRACENOTIMPL2
#define ATLTRACENOTIMPL2() ATLTRACENOTIMPL(__FUNCTION__"\n")
#endif 

namespace ATL {

static CString AtlErrorMessage(HRESULT hr) {
	LPTSTR tempMessage;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM,
		0,
		hr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&tempMessage,
		0,
		0)) {
			const int nLen = lstrlen(tempMessage);
			if (nLen > 1 && tempMessage[nLen-1] == '\n') {
				tempMessage[nLen-1] = 0;
				if (tempMessage[nLen-2] == '\r') {
					tempMessage[nLen-2] = 0;
				}
			}
			CString message(tempMessage);
			LocalFree(tempMessage);
			return message;
	} else {
		CString message;
		//Check errors for Custom Interfaces Facility of FACILITY_ITF (see winerror.h)
		//Error codes for this facility could have duplicate descriptions (see MSDN Q183216)
		const unsigned short E_ITF_FIRST = 0x200; //first error in the range
		const HRESULT WCODE_HRESULT_FIRST = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, E_ITF_FIRST);
		const HRESULT WCODE_HRESULT_LAST  = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF+1, 0);
		if (hr >= WCODE_HRESULT_FIRST &&  hr < WCODE_HRESULT_LAST) {
			message.Format(_T("COM Interface error 0x%0lX"), hr);
		} else {
			message.Format(_T("Unknown COM error 0x%0lX"), hr);
		}
		return message;
	}
}

struct HResultNoThrow {
	static void check(HRESULT hr) {
		if (FAILED(hr)) {
			ATLTRACE2(atlTraceException, 0, _T("Error: %s\n"), AtlErrorMessage(hr));
		}
	}
};

struct HResultThrow {
	static void check(HRESULT hr) { 
		// Will trace the error
		HResultNoThrow::check(hr); 

		if (FAILED(hr)) {
			AtlThrow(hr); 
		}
	}
};

template <class HResultTraits = HResultNoThrow>
class HResult {
	HRESULT hr;

	void assign(HRESULT hr) throw() {
		HResultTraits::check(hr);
	}
public:
	HResult(HRESULT hr = S_OK) { assign(hr); }
	HResult& operator =(HRESULT hr) { 
		assign(hr);
		return *this;
	}
	operator HRESULT() const { return hr; }
};



template <class T>
struct EmptyStruct : public T {
  EmptyStruct() { ZeroMemory(this, sizeof(T)); }
  operator T&() { return *this; }
};



/** Initializes the given T structures cbSize to the sizeof(T) after 0-filling its memory.

	This provides an easy way of using common Win32 structures
	@author_philk
*/
template <class T>
struct WindowsStruct : public EmptyStruct<T> {
  WindowsStruct() {
    *((DWORD*)this) = sizeof(T);
  }
};


struct StringHelper {
	static CString capitalize(LPCTSTR text) {
		CString final;
		for (PCTSTR c = text; *c; ++c) {
			// If not first and uppercase, insert a _
			if (c > text && isupper(*c)) {
				final += _T(' ');
			}
			final += (TCHAR)(*c);
		}

		return final;
	}

	static CString getClassName(const type_info& clazz) {
		CStringA className(clazz.name());
		className.Replace("class ", "");
		className.Replace("struct ", "");
		return CString(className);
	}

	static CString capitalize(const type_info& clazz) {
		return capitalize(getClassName(clazz));
	}
};


template <class T>
class CShellComPtr : public CComPtr<T> {
public:
	__checkReturn HRESULT CoCreateInstance(__in REFCLSID rclsid, __in_opt LPUNKNOWN pUnkOuter = NULL) throw() {
		ATLASSERT(p == NULL);
		return ::SHCoCreateInstance(0, &rclsid, pUnkOuter, __uuidof(T), (void**)&p);
	}
};

/** Removes the object from the registry after it has been instantiated once.
	
	When the object is destructed it removed its registration from the 
	registry using the T::UpdateRegistry method. That implies that the class
	given as T must also implement a UpdateRegistry(bool) method.

	@author_philk
*/
template <class T>
class RunOnce {
protected:
	/** Used to modify the behaviour during runtime. 
		If you decide that the objects registration should not removed from
		the registry upon destruction of the object, then call 
		@code runOnce() = true; @endcode
	*/
	bool& runOnce() { return removeFromRegistryAfterRun; }

	RunOnce() : removeFromRegistryAfterRun(true) {}

	virtual ~RunOnce() {
		if (removeFromRegistryAfterRun) {
			T::UpdateRegistry(false);
		}
	}
private:
	/// Allows the implementing class to disable unregistrering the object
	bool removeFromRegistryAfterRun;
};

/** Performs certain operations on the objects defined in the ATL module.

	Use this class to iterator over all objects declared in the modules 
	object map.

	@author_philk
*/
class ModuleObjects {
public:
	/**
	    @return if the entry is creatable through COM
	*/
	static bool isCreatable(const _ATL_OBJMAP_ENTRY& entry) {
		return entry.pfnGetClassObject || entry.pfnCreateInstance;
	}

	/** Vistor for object map entries.	    
	*/
	struct Visitor {
		/**	
			@param [in] entry
		    @return S_OK to continue iteration, any other value will stop 
			the iteration and return the value back to the caller of the 
			ModuleObjects::visitObjects method.
		*/
		STDMETHOD(visit)(const _ATL_OBJMAP_ENTRY& entry) = 0;
	};

	/**
		@param [in] visitor The visitors visit method will be called for all found
		@param [in] onlyCreatable only creatable objects should be visited.

	    @return S_OK or what the Visitor::visit method returns.
	*/
	STDMETHODIMP visitObjects(Visitor& visitor, const bool onlyCreatable = true) {
		HRESULT hr = S_OK;
		for (_ATL_OBJMAP_ENTRY** ppEntry = _AtlComModule.m_ppAutoObjMapFirst; 
			ppEntry < _AtlComModule.m_ppAutoObjMapLast && S_OK == hr; // S_FALSE will also break the iteration
			++ppEntry) {
				if (*ppEntry != NULL) {
					if (onlyCreatable && !isCreatable(**ppEntry)) {
						continue;
					}
					hr = visitor.visit(**ppEntry);
				}
		}
		return hr;
	}
	
	/** Calls the Visitor::visit() method only for objects that implement the given category.
	*/
	STDMETHODIMP visitObjects(Visitor& visitor, REFCATID catid, const bool onlyCreatable = true) {
		return visitObjects(CategoryVisitor(visitor, catid));
	}

protected:
	/** Specialized visitor that filters on the categories a object is implementing.

	@par Usage
	@code
	struct MyVisitor : ModuleObjects::Visitor {
	};
	MyVisitor myVisitor;
	CategoryVisitor catVis(myVisitor, CATID_DeskBand);
	ModuleObjects mo;
	mo.visitObjects(catVis);
	@endcode
	You can use the shortcut provided by the method 
	ModuleObjects::visitObjects(Visitor& visitor, REFCATID catid, const bool onlyCreatable = true)
	*/
	class CategoryVisitor : public Visitor {
		REFCATID catid;
		Visitor& outer;

		STDMETHODIMP visit(const _ATL_OBJMAP_ENTRY& entry) {
			for (const _ATL_CATMAP_ENTRY* categories = entry.pfnGetCategoryMap();
				categories && categories->iType; 
				++categories) {
					if (catid == *categories->pcatid) {
						return outer.visit(entry);
						// Can the category be in the category map more than once?
						// Yes, but we do not care!
					}
			}
			return S_OK;
		};
	public:
		CategoryVisitor(Visitor& outer, REFCATID catid) : outer(outer), catid(catid) {}
	};
};

template <class T>
class CModuleObjectsVisitor {
	template<typename T>
	struct TemplateVisitor : public ModuleObjects::Visitor {
		typedef HRESULT (T::*VisitorMethod)(const _ATL_OBJMAP_ENTRY& entry);

		T* pThis;
		VisitorMethod visitorMethod;

		TemplateVisitor(T* pThis, VisitorMethod visitorMethod) : 
			pThis(pThis), 
			visitorMethod(visitorMethod) {
		}

		STDMETHODIMP visit(const _ATL_OBJMAP_ENTRY& entry) {
			return visitorMethod ? (pThis->*visitorMethod)(entry) : S_OK;
		}
	};

protected:
	STDMETHODIMP visitObjects(HRESULT (T::*visitorMethod)(const _ATL_OBJMAP_ENTRY& entry), REFCATID catid) {
		HRESULT hr = S_OK;
		ModuleObjects objects;
		return objects.visitObjects(TemplateVisitor<T>(static_cast<T*>(this), visitorMethod), catid);
	};
};

/** Initializes and uninitializes the COM subsystem.

	Use this in threads when you want to work with COM objects.
	Place a ComInit at the beginning of the Thread worker proc.
	It will automatically initialize COM for the thread and before the proc
	returns it will un-initialize the COM subsystem.

	@author_philk
*/
struct ComInit {
	ComInit() { ::CoInitialize(0); }
	~ComInit() { ::CoUninitialize(); }
};



/** Extension of CPath set to a module file path.

	Use this class to quickly get the name of a modules filepath.

	@par Environments
	ANSI,UNICODE

	@author_philk
*/
class CModulePath : public CPath {
	static CPath create(HINSTANCE instance) {
		TCHAR path[MAX_PATH];
		::GetModuleFileName(instance, path, MAX_PATH);
		return path;
	}
public:
	/**		    
	*/
	CModulePath(HINSTANCE instance=GetModuleHandle(0)) : CPath(create(instance)) {};
};

} // ATL