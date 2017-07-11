// BrowserView.h : interface of the CBrowserView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <exdispid.h>

const int _nDispatchID = 1;

#define WM_BROWSERTITLECHANGE        (WM_APP)
#define WM_BROWSERDOCUMENTCOMPLETE   (WM_APP + 1)
#define WM_BROWSERSTATUSTEXTCHANGE   (WM_APP + 2)


class CBrowserView : public CWindowImpl<CBrowserView, CAxWindow>, 
		public IDispEventSimpleImpl<_nDispatchID, CBrowserView, &DIID_DWebBrowserEvents2>
{
public:
	DECLARE_WND_SUPERCLASS(_T("TabBrowser_TabPageWindow"), CAxWindow::GetWndClassName())

	// IDispatch events function info
	static _ATL_FUNC_INFO DocumentComplete2_Info;
	static _ATL_FUNC_INFO TitleChange_Info;
	static _ATL_FUNC_INFO StatusTextChange_Info;
	static _ATL_FUNC_INFO CommandStateChange_Info;

	bool m_bCanGoBack;
	bool m_bCanGoForward;


	CBrowserView() : m_bCanGoBack(false), m_bCanGoForward(false)
	{
	}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		if((pMsg->message < WM_KEYFIRST || pMsg->message > WM_KEYLAST) &&
		   (pMsg->message < WM_MOUSEFIRST || pMsg->message > WM_MOUSELAST))
			return FALSE;

		BOOL bRet = FALSE;
		// give HTML page a chance to translate this message
		if(pMsg->hwnd == m_hWnd || IsChild(pMsg->hwnd))
			bRet = (BOOL)SendMessage(WM_FORWARDMSG, 0, (LPARAM)pMsg);

		return bRet;
	}

	virtual void OnFinalMessage(HWND /*hWnd*/)
	{
		delete this;
	}

	void SetFocusToHTML()
	{
		CComPtr<IWebBrowser2> spWebBrowser;
		HRESULT hRet = QueryControl(IID_IWebBrowser2, (void**)&spWebBrowser);
		if(SUCCEEDED(hRet) && spWebBrowser != NULL)
		{
			CComPtr<IDispatch> spDocument;
			hRet = spWebBrowser->get_Document(&spDocument);
			if(SUCCEEDED(hRet) && spDocument != NULL)
			{
				CComQIPtr<IHTMLDocument2> spHtmlDoc = spDocument;
				if(spHtmlDoc != NULL)
				{
					CComPtr<IHTMLWindow2> spParentWindow;
					hRet = spHtmlDoc->get_parentWindow(&spParentWindow);
					if(spParentWindow != NULL)
						spParentWindow->focus();
				}
			}
		}
	}

// Event map and handlers
#ifdef _VC80X
  #pragma warning(disable:4867)
#endif
	BEGIN_SINK_MAP(CBrowserView)
		SINK_ENTRY_INFO(_nDispatchID, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE, OnEventDocumentComplete, &DocumentComplete2_Info)
		SINK_ENTRY_INFO(_nDispatchID, DIID_DWebBrowserEvents2, DISPID_TITLECHANGE, OnEventTitleChange, &TitleChange_Info)
		SINK_ENTRY_INFO(_nDispatchID, DIID_DWebBrowserEvents2, DISPID_STATUSTEXTCHANGE, OnEventStatusTextChange, &StatusTextChange_Info)
		SINK_ENTRY_INFO(_nDispatchID, DIID_DWebBrowserEvents2, DISPID_COMMANDSTATECHANGE, OnEventCommandStateChange, &CommandStateChange_Info)
	END_SINK_MAP()
#ifdef _VC80X
  #pragma warning(default:4867)
#endif

	void __stdcall OnEventDocumentComplete(IDispatch* /*pDisp*/, VARIANT* URL)
	{
		// Send message to the main frame
		ATLASSERT(V_VT(URL) == VT_BSTR);
		USES_CONVERSION;
		SendMessage(GetTopLevelWindow(), WM_BROWSERDOCUMENTCOMPLETE, (WPARAM)m_hWnd, (LPARAM)OLE2T(URL->bstrVal));

		SetFocusToHTML();
	}

	void __stdcall OnEventTitleChange(BSTR Text)
	{
		USES_CONVERSION;
		SendMessage(GetTopLevelWindow(), WM_BROWSERTITLECHANGE, (WPARAM)m_hWnd, (LPARAM)OLE2CT(Text));
	}

	void __stdcall OnEventStatusTextChange(BSTR Text)
	{
		USES_CONVERSION;
		SendMessage(GetTopLevelWindow(), WM_BROWSERSTATUSTEXTCHANGE, (WPARAM)m_hWnd, (LPARAM)OLE2CT(Text));
	}

	void __stdcall OnEventCommandStateChange(long Command, VARIANT_BOOL Enable)
	{
		if(Command == CSC_NAVIGATEBACK)
			m_bCanGoBack = (Enable != VARIANT_FALSE);
		else if(Command == CSC_NAVIGATEFORWARD)
			m_bCanGoForward = (Enable != VARIANT_FALSE);
	}

// Message map and handlers
	BEGIN_MSG_MAP(CBrowserView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
	END_MSG_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

		// Connect events
		CComPtr<IWebBrowser2> spWebBrowser2;
		HRESULT hRet = QueryControl(IID_IWebBrowser2, (void**)&spWebBrowser2);
		if(SUCCEEDED(hRet))
		{
			if(FAILED(DispEventAdvise(spWebBrowser2, &DIID_DWebBrowserEvents2)))
				ATLASSERT(FALSE);
		}

		// Set host flag to indicate that we handle themes
		CComPtr<IAxWinAmbientDispatch> spHost;
		hRet = QueryHost(IID_IAxWinAmbientDispatch, (void**)&spHost);
		if(SUCCEEDED(hRet))
		{
			const DWORD _DOCHOSTUIFLAG_THEME = 0x40000;
			hRet = spHost->put_DocHostFlags(DOCHOSTUIFLAG_NO3DBORDER | _DOCHOSTUIFLAG_THEME);
			ATLASSERT(SUCCEEDED(hRet));
		}

		return lRet;
	}
	
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		// Disconnect events
		CComPtr<IWebBrowser2> spWebBrowser2;
		HRESULT hRet = QueryControl(IID_IWebBrowser2, (void**)&spWebBrowser2);
		if(SUCCEEDED(hRet))
			DispEventUnadvise(spWebBrowser2, &DIID_DWebBrowserEvents2);

		bHandled=FALSE;
		return 1;
	}

	LRESULT OnSetFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);
		SetFocusToHTML();

		return lRet;
	}
};

__declspec(selectany) _ATL_FUNC_INFO CBrowserView::DocumentComplete2_Info = { CC_STDCALL, VT_EMPTY, 2, { VT_DISPATCH, VT_BYREF | VT_VARIANT } };
__declspec(selectany) _ATL_FUNC_INFO CBrowserView::TitleChange_Info = { CC_STDCALL, VT_EMPTY, 1, { VT_BSTR } };
__declspec(selectany) _ATL_FUNC_INFO CBrowserView::StatusTextChange_Info = { CC_STDCALL, VT_EMPTY, 1, { VT_BSTR } };
__declspec(selectany) _ATL_FUNC_INFO CBrowserView::CommandStateChange_Info = { CC_STDCALL, VT_EMPTY, 2, { VT_I4, VT_BOOL } };
