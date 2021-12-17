#include <atomic>
#include <sstream>
#include <utility>
#include <list>

#include <tt/app/ComHelper.h>
#include <tt/http/helpers.h>
#include <tt/http/HttpConnectMgr.h>
#include <tt/http/HttpResponseHandler.h>
#include <tt/mem/util.h>
#include <tt/platform/tt_error.h>
#include <tt/platform/tt_printf.h>
#include <tt/settings/settings.h>
#include <tt/str/str.h>
#include <tt/thread/thread.h>
#include <tt/thread/CriticalSection.h>
#include <tt/thread/Mutex.h>
#include <tt/version/Version.h>


namespace tt {
namespace http {

tt::thread::handle g_httpThread;
tt::thread::Mutex  g_httpMutex;
typedef std::list<tt::http::HttpRequest> PendingRequests;
PendingRequests g_httpRequests;
std::atomic<bool> g_httpStopThread = false;

int httpRequestThread(void* p_arg)
{
	(void)p_arg;
	tt::http::HttpConnectMgr mgr;
	g_httpStopThread = false;
	
	while (g_httpStopThread == false)
	{
		tt::http::HttpRequest request;
		bool handleRequest = false;
		{
			tt::thread::CriticalSection critSec(&g_httpMutex);
			if (g_httpRequests.empty() == false)
			{
				request = *g_httpRequests.begin();
				g_httpRequests.pop_front();
				handleRequest = true;
			}
		}
		
		if (handleRequest)
		{
			mgr.sendRequest(request);
		}
	}
	return 0;
}


//--------------------------------------------------------------------------------------------------
// Public member functions

HttpConnectMgr::HttpConnectMgr()
#if !defined(_XBOX)
:
m_session(0)
#endif
{
#if !defined(_XBOX)
	// Compose a user agent string for our HTTP communications based on the application name and revision
	std::wstringstream userAgent;
	userAgent << tt::settings::getApplicationName() << L"_rev_"
	          << tt::version::getClientRevisionNumber()
	          << L"." << tt::version::getLibRevisionNumber();
	
	TT_Printf("HttpConnectMgr::HttpConnectMgr: User-agent string: '%s'\n",
	          tt::str::narrow(userAgent.str()).c_str());
	
	// Open an HTTP session
	DWORD sessionFlags = 0;
	sessionFlags = WINHTTP_FLAG_ASYNC;
	m_session = WinHttpOpen(
		userAgent.str().c_str(),
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		sessionFlags);
	
	TT_ASSERTMSG(m_session != 0, "Initializing WinHTTP library failed.");
	
	// Install a callback for asynchronous operations
	if (m_session != 0)
	{
		// Register a pointer to our HttpConnectMgr instance for the callback
		HttpConnectMgr* ptr = this;
		BOOL optionSet = WinHttpSetOption(m_session, WINHTTP_OPTION_CONTEXT_VALUE,
		                                  &ptr, static_cast<DWORD>(sizeof(ptr)));
		TT_ASSERTMSG(optionSet, "Setting HttpConnectMgr instance pointer as callback context failed.");
		
		// Set the callback
		if (WinHttpSetStatusCallback(m_session, statusCallback,
		                             WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0) ==
		    WINHTTP_INVALID_STATUS_CALLBACK)
		{
			TT_PANIC("Installing WinHTTP status callback for asynchronous operations failed.");
		}
	}
#endif
}


HttpConnectMgr::~HttpConnectMgr()
{
#if !defined(_XBOX)
	if (m_session != 0)
	{
		// Unregister the network callback
		WinHttpSetStatusCallback(m_session, 0, 0, 0);
		
		WinHttpCloseHandle(m_session);
	}
#endif
}


void HttpConnectMgr::startQueuingThread()
{
	TT_ASSERT(g_httpThread == 0);
	if (g_httpThread == 0)
	{
		g_httpThread = tt::thread::create(httpRequestThread, 0, false, 0,
			tt::thread::priority_below_normal, tt::thread::Affinity_None, "HTTP Request Thread");
	}
	TT_NULL_ASSERT(g_httpThread);
}


void HttpConnectMgr::stopQueuingThread()
{
	g_httpStopThread = true;
	tt::thread::wait(g_httpThread);
}


void HttpConnectMgr::queueRequest(const HttpRequest& p_request)
{
	TT_NULL_ASSERT(g_httpThread);
	tt::thread::CriticalSection critSec(&g_httpMutex);
	g_httpRequests.push_back(p_request);
}


bool HttpConnectMgr::sendRequest(const HttpRequest& p_request)
{
#if !defined(_XBOX)
	if (m_session == 0)
	{
		// WinHTTP not initialized; cannot do anything
		return false;
	}
	
	//==========================================================================
	// FIXME: Factor this preamble code out to shared code
	std::string finalUrl(p_request.url);
	
	if (p_request.getParameters.empty() == false)
	{
		finalUrl += "?" + buildQuery(p_request.getParameters);
	}
	
	std::string postData;
	if (p_request.postParameters.empty() == false)
	{
		TT_ASSERTMSG(p_request.requestMethod == HttpRequest::RequestMethod_Post,
		             "POST parameters were specified in HttpRequest, "
		             "but request method is not POST (request method is '%s'). "
		             "These POST parameters will not reach the server.",
		             p_request.getRequestMethodName(p_request.requestMethod));
		
		for (FunctionArguments::const_iterator it(p_request.postParameters.begin());
		     it != p_request.postParameters.end() ; ++it)
		{
			postData += it->first + '=' + it->second + '&';
		}
		postData.erase(postData.size() - 1);
	}
	
	/*
	TT_Printf("HTTP request:\n- Headers (%u):\n", p_request.headers.size());
	for (Headers::const_iterator it = p_request.headers.begin();
	     it != p_request.headers.end(); ++it)
	{
		TT_Printf("   - '%s' = '%s'\n", (*it).first.c_str(), (*it).second.c_str());
	}
	TT_Printf("- POST data: %s\n", postData.c_str());
	TT_Printf("- Full URL: http%s://%s%s\n", p_request.useSSL ? "s" : "",
	          p_request.server.c_str(), finalUrl.c_str());
	//*/
	//==========================================================================
	
	
	// Create an HTTP connection object
	HINTERNET connection = WinHttpConnect(
		m_session,
		tt::str::widen(p_request.server).c_str(),
		INTERNET_DEFAULT_PORT,
		0);
	if (connection == 0)
	{
#if !defined(TT_BUILD_FINAL)
		const DWORD errCode     = GetLastError();
		char        reason[256] = { 0 };
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, reason, 256, 0);
		TT_PANIC("Connecting to '%s' failed (0x%08X): '%s'",
		         p_request.server.c_str(), errCode, reason);
#endif
		return false;
	}
	
	// Prepare a URL request
	static LPCWSTR acceptTypes[] =
	{
		L"text/plain",
		L"text/html",
		L"text/xml",
		L"application/octet-stream",
		L"application/json",
		0
	};
	
	const std::wstring requestVerb(tt::str::widen(HttpRequest::getRequestMethodName(p_request.requestMethod)));
	HINTERNET request = WinHttpOpenRequest(
		connection,
		requestVerb.c_str(),
		tt::str::widen(finalUrl).c_str(),
		0,
		WINHTTP_NO_REFERER,
		acceptTypes,
		(p_request.useSSL ? WINHTTP_FLAG_SECURE : 0));
	if (request == 0)
	{
#if !defined(TT_BUILD_FINAL)
		const DWORD errCode     = GetLastError();
		char        reason[256] = { 0 };
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, reason, 256, 0);
		TT_PANIC("Opening GET request to '%s' failed (0x%08X): '%s'",
		         finalUrl.c_str(), errCode, reason);
#endif
		WinHttpCloseHandle(connection);
		return false;
	}
	
	// Record the download information
	DownloadInfo info;
	info.connection      = connection;
	info.state           = DownloadState_SendingRequest;
	info.responseHandler = p_request.responseHandler;
	info.optionalData    = postData;
	
	std::pair<DownloadMapping::iterator, bool> trackIt =
		m_downloadTracking.insert(std::make_pair(request, info));
	
	// Compose the headers into a single header string (needed by WinHTTP)
	std::wstring headers(tt::str::widen(implodeKeyValue(p_request.headers, ": ", "\r\n", true)));
	headers += L"Content-Type: application/x-www-form-urlencoded\r\n";
	
	
	// Send the download request
	
	BOOL requestSent = WinHttpSendRequest(
		request,
		headers.c_str(),
		static_cast<DWORD>(headers.length()), // Length of header string in characters
		(void*)((*trackIt.first).second.optionalData.c_str()), // This buffer must remain available until the 
		                     // request handle is closed or the call to WinHttpReceiveResponse has completed.
		static_cast<DWORD>(info.optionalData.length()), // Bytes
		static_cast<DWORD>(info.optionalData.length()), // Bytes
		0);
	if (requestSent == FALSE)
	{
#if !defined(TT_BUILD_FINAL)
		const DWORD errCode     = GetLastError();
		char        reason[256] = { 0 };
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, reason, 256, 0);
		TT_PANIC("Sending GET request failed (0x%08X): '%s'", errCode, reason);
#endif
		m_downloadTracking.erase(trackIt.first);
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connection);
		return false;
	}
	
#else
	TT_PANIC("HttpConnectMgr not implemented for Xbox 360 yet.");
	(void)p_request;
#endif
	
	return true;
}


void HttpConnectMgr::cancelAllRequestsForResponseHandler(HttpResponseHandler* p_handler)
{
#if !defined(_XBOX)
	for (DownloadMapping::iterator it = m_downloadTracking.begin();
	     it != m_downloadTracking.end(); )
	{
		if ((*it).second.responseHandler == p_handler)
		{
			// This is a request that needs to be cancelled
			// First get the required information for cancellation
			HINTERNET request    = (*it).first;
			HINTERNET connection = (*it).second.connection;
			
			/*
			TT_Printf("HttpConnectMgr::cancelAllRequestsForResponseHandler: "
			          "Found request 0x%08X on connection 0x%08X for response handler 0x%08X\n",
			          request, connection, p_handler);
			//*/
			
			// Remove the download tracking info for this request
			DownloadMapping::iterator eraseIt(it);
			++it;
			m_downloadTracking.erase(eraseIt);
			
			// Cancel the request by closing the handles
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connection);
		}
		else
		{
			// Should not cancel this request; continue with the next entry
			++it;
		}
	}
#else
	TT_PANIC("HttpConnectMgr not implemented for Xbox 360 yet.");
	(void)p_handler;
#endif
}


bool HttpConnectMgr::hasInternetConnection() const
{
	// Dummy implementation for Windows: always indicate there is an internet connection
	// FIXME: Do we want accurate connection availability detection on Windows? If so, research this...
	return true;
}


void HttpConnectMgr::openUrlExternally(const std::string& p_url)
{
#if !defined(_XBOX)
	TT_ASSERTMSG(tt::app::ComHelper::isMultiThreaded() == false,
	             "In order for HttpConnectMgr::openUrlExternally to work, "
	             "COM must be initialized in single-threaded mode (Shell API restriction).");
	
	const std::wstring wideUrl(tt::str::utf8ToUtf16(p_url));
	SHELLEXECUTEINFOW execInfo = { 0 };
	execInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
	execInfo.fMask  = SEE_MASK_FLAG_NO_UI;
	execInfo.hwnd   = HWND_DESKTOP;
	execInfo.nShow  = SW_SHOW;
	execInfo.lpFile = wideUrl.c_str();
	
	if (ShellExecuteExW(&execInfo) == FALSE)
	{
		const DWORD errCode     = GetLastError();
		wchar_t     errMsg[512] = { 0 };
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, errMsg, 512, 0);
		
		std::wostringstream msg;
		msg << L"Opening URL '" << wideUrl << L"' failed.\n\n"
		    << L"Error (code " << errCode << L"):\n" << errMsg;
		
		MessageBoxW(HWND_DESKTOP, msg.str().c_str(),
		            (settings::getApplicationName() + L" - Open URL").c_str(), MB_ICONERROR);
	}
#else
	TT_PANIC("HttpConnectMgr not implemented for Xbox 360 yet.");
	(void)p_url;
#endif
}


//--------------------------------------------------------------------------------------------------
// Private member functions

void HttpConnectMgr::handleReturnData(s32 p_statusCode, const Data& p_data,
                                      HttpResponseHandler* p_responseHandler)
{
	std::string data;
	if (p_data.empty() == false)
	{
		data = parseDataAsString(&p_data.at(0), p_data.size());
	}
	
	if (p_responseHandler != 0)
	{
		p_responseHandler->handleHttpResponse(p_statusCode, data);
	}
}


#if !defined(TT_BUILD_FINAL)
const char* HttpConnectMgr::getDownloadStateName(HttpConnectMgr::DownloadState p_state)
{
	switch (p_state)
	{
	case DownloadState_Idle:               return "Idle";
	case DownloadState_SendingRequest:     return "SendingRequest";
	case DownloadState_WaitingForResponse: return "WaitingForResponse";
	case DownloadState_QueryDataAvailable: return "QueryDataAvailable";
	case DownloadState_ReceivingData:      return "ReceivingData";
	default:                               return "unknown";
	}
}
#endif


#if !defined(_XBOX)

void CALLBACK HttpConnectMgr::statusCallback(HINTERNET p_handle,
                                             DWORD_PTR p_context,
                                             DWORD     p_internetStatus,
                                             LPVOID    p_statusInformation,
                                             DWORD     p_statusInformationLength)
{
	// Filter out notifications we're not interested in
	switch (p_internetStatus)
	{
	case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:
	case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
		return;
	}
	
	HttpConnectMgr* impl = reinterpret_cast<HttpConnectMgr*>(p_context);
	if (impl == 0)
	{
		return;
	}
	
	// Check if there is tracking information for this handle
	DownloadMapping::iterator it = impl->m_downloadTracking.find(p_handle);
	if (it == impl->m_downloadTracking.end())
	{
		TT_WARN("No tracking information available for handle 0x%08X: ignoring callback.", p_handle);
		return;
	}
	
	DownloadInfo& info = (*it).second;
	
	//TT_Printf("HttpConnectMgr::statusCallback: Handle 0x%08X is in state %s.\n",
	//          p_handle, getDownloadStateName(info.state));
	
	switch (p_internetStatus)
	{
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		// WinHttpSendRequest completed; start receiving the response
		TT_ASSERT(info.state == DownloadState_SendingRequest);
		info.state = DownloadState_WaitingForResponse;
		if (WinHttpReceiveResponse(p_handle, 0) == FALSE)
		{
			reportHttpError("No response received to GET request", info);
		}
		break;
		
	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		// WinHttpReceiveResponse completed; start querying for data
		TT_ASSERT(info.state == DownloadState_WaitingForResponse);
		
		{
			DWORD statusCode     = 0;
			DWORD statusCodeSize = sizeof(DWORD);
			if (WinHttpQueryHeaders(p_handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			                        0, &statusCode, &statusCodeSize, 0) == FALSE)
			{
				reportHttpError("Retrieving HTTP response status code failed", info);
			}
			
			info.statusCode = static_cast<s32>(statusCode);
			//TT_Printf("HttpConnectMgr::statusCallback: HTTP response status code: %d\n", info.statusCode);
		}
		
		
		info.state = DownloadState_QueryDataAvailable;
		if (WinHttpQueryDataAvailable(p_handle, 0) == FALSE)
		{
			reportHttpError("Querying whether data is available failed", info);
		}
		break;
		
	case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
		{
			TT_ASSERT(info.state == DownloadState_QueryDataAvailable);
			TT_ASSERT(p_statusInformationLength == sizeof(DWORD));
			TT_ASSERT(info.rawData == 0);
			DWORD dataLength = *reinterpret_cast<DWORD*>(p_statusInformation);
			
			if (dataLength == 0)
			{
				// No more data available
				//TT_Printf("HttpConnectMgr::statusCallback: No more data available. Closing connection.\n");
				WinHttpCloseHandle(p_handle);
				WinHttpCloseHandle(info.connection);
				
				impl->handleReturnData(info.statusCode, info.data, info.responseHandler);
				
				impl->m_downloadTracking.erase(it);
				break;
			}
			
			// Allocate space to receive the data in
			info.state = DownloadState_ReceivingData;
			info.rawData = new u8[dataLength];
			tt::mem::zero8(info.rawData, static_cast<tt::mem::size_type>(dataLength));
			
			// Read the Data
			if (WinHttpReadData(p_handle, info.rawData, dataLength, 0) == FALSE)
			{
				reportHttpError("Reading data from HTTP connection failed", info);
			}
		}
		break;
		
	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
		TT_ASSERT(info.state == DownloadState_ReceivingData);
		TT_NULL_ASSERT(info.rawData);
		
		info.data.insert(info.data.end(), info.rawData, info.rawData + p_statusInformationLength);
		
		delete[] info.rawData;
		info.rawData = 0;
		
		if (p_statusInformationLength == 0)
		{
			// No more data available
			//TT_Printf("HttpConnectMgr::statusCallback: No more data available. Closing connection.\n");
			WinHttpCloseHandle(p_handle);
			WinHttpCloseHandle(info.connection);
			
			impl->handleReturnData(info.statusCode, info.data, info.responseHandler);
			
			impl->m_downloadTracking.erase(it);
			break;
		}
		
		// Request more data
		info.state = DownloadState_QueryDataAvailable;
		if (WinHttpQueryDataAvailable(p_handle, 0) == FALSE)
		{
			reportHttpError("Querying whether data is available failed", info);
		}
		break;
		
		// Also handle error states (notify client of errors)
	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		if (info.responseHandler != 0)
		{
			info.responseHandler->handleHttpError(L"An error occurred while sending an HTTP request.");
		}
		break;
		
	case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
		if (info.responseHandler != 0)
		{
			info.responseHandler->handleHttpError(L"One or more errors were encountered while "
				L"retrieving a Secure Sockets Layer (SSL) certificate from the server.");
		}
		break;
	}
	
#if 0
	// DEBUG: Display the name of the status notification that was received
	const char* statusName = "unknown";
	switch (p_internetStatus)
	{
	case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:    statusName = "WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION";    break;
	case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:   statusName = "WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER";   break;
	case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:  statusName = "WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER";  break;
	case WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED:     statusName = "WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED";     break;
	case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:        statusName = "WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE";        break;
	case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:        statusName = "WINHTTP_CALLBACK_STATUS_HANDLE_CREATED";        break;
	case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:        statusName = "WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING";        break;
	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:     statusName = "WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE";     break;
	case WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE: statusName = "WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE"; break;
	case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:         statusName = "WINHTTP_CALLBACK_STATUS_NAME_RESOLVED";         break;
	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:         statusName = "WINHTTP_CALLBACK_STATUS_READ_COMPLETE";         break;
	case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:    statusName = "WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE";    break;
	case WINHTTP_CALLBACK_STATUS_REDIRECT:              statusName = "WINHTTP_CALLBACK_STATUS_REDIRECT";              break;
	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:         statusName = "WINHTTP_CALLBACK_STATUS_REQUEST_ERROR";         break;
	case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:          statusName = "WINHTTP_CALLBACK_STATUS_REQUEST_SENT";          break;
	case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:        statusName = "WINHTTP_CALLBACK_STATUS_RESOLVING_NAME";        break;
	case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:     statusName = "WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED";     break;
	case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:        statusName = "WINHTTP_CALLBACK_STATUS_SECURE_FAILURE";        break;
	case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:       statusName = "WINHTTP_CALLBACK_STATUS_SENDING_REQUEST";       break;
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:  statusName = "WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE";  break;
	case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:        statusName = "WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE";        break;
	}
	
	TT_Printf("HttpConnectMgr::statusCallback: Handle 0x%08X received status 0x%08X ('%s').\n",
	          p_handle, p_internetStatus, statusName);
#endif
}


void HttpConnectMgr::reportHttpError(const char* p_internalMessage, DownloadInfo& p_info)
{
	const DWORD errCode     = GetLastError();
	wchar_t     reason[512] = { 0 };
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, reason, 256, 0);
	TT_PANIC("%s (0x%08X): '%s'", p_internalMessage, errCode, tt::str::utf16ToUtf8(reason).c_str());
	
	// FIXME: Clean up connection somehow?
	if (p_info.responseHandler != 0)
	{
		p_info.responseHandler->handleHttpError(reason);
	}
}

#endif  // !defined(_XBOX)

// Namespace end
}
}
