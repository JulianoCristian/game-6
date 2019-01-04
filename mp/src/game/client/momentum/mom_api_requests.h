#pragma once

#include "igamesystem.h"
#include "steam/steam_api.h"
#include "utldelegate.h"

typedef CUtlDelegate<void (KeyValues *pKv)> CallbackFunc;

class CAPIRequests : public CAutoGameSystemPerFrame
{
public:
    CAPIRequests();

    // === HTTP API METHODS ===
    // The way to call these is to create a delegate of a callback function, and pass that in.
    // Example:
    //      `g_pAPIRequests->GetMaps(nullptr, UtlMakeDelegate(this, &SomeClass::SomeCallbackFunc));`
    //      where SomeCallbackFunc is a void of SomeClass that takes a KeyValues pointer as the only parameter.
    // API requests will pass a KeyValues object (that will auto delete itself, no worries!) with:
    //      "code" -- The integer HTTP status code returned. 0 if it's an IO error
    //      "data" -- The response data, parsed JSON represented as KeyValues
    //      "error" -- An error object represented as KeyValues
    //          "err_parse" -- If any parsing issue happens with JSON, it will be logged here as a string, inside error
    //
    // All API requests return `true` if the call succeeded in sending, else `false`.

    // ===== Auth ======
    // Sends an auth ticket to the server to get an API key to use for requests. 
    // This should be the first API call to make, and is done automatically on game boot.
    bool SendAuthTicket(CallbackFunc func);

    // ===== Maps ======
    bool GetMaps(KeyValues *pKvFilters, CallbackFunc func);
    bool GetMapInfo(uint32 mapID, CallbackFunc func);
    bool GetMapByName(const char *pMapName, CallbackFunc func);

    // ==== Leaderboards ====
    bool GetTop10MapTimes(uint32 mapID, CallbackFunc func);
    bool GetFriendsTimes(uint32 mapID, CallbackFunc func);
    bool GetAroundTimes(uint32 mapID, CallbackFunc func);
    bool SubmitRun(uint32 mapID, const CUtlBuffer &replayBuf, CallbackFunc func);

    // === File Downloading ===
    /**
     * @param pszURL        The URL to the file
     * @param start         The start function of the download, see DownloadCall for more info
     * @param prog          The progress function of the download, see DownloadCall for more info
     * @param end           The complete function of the download, see DownloadCall for more info
     * @param pFileName     The file name (including any path) of where the file should be stored
     * @param pFilePathID   (Optional) The pathID of where the file should be stored. Defaults to "GAME".
     * @return The handle of the request
     */
    HTTPRequestHandle DownloadFile(const char *pszURL, CallbackFunc start, CallbackFunc prog, CallbackFunc end,
                                   const char *pFileName, const char *pFilePathID = "GAME");

protected:
    // CAutoGameSystemPerFrame
    void PostInit() OVERRIDE;
    void Shutdown() OVERRIDE;

    // Auth ticket impl
    STEAM_CALLBACK(CAPIRequests, OnAuthTicket, GetAuthSessionTicketResponse_t);
    void OnAuthHTTP(KeyValues *pResponse);
    void DoAuth();

    STEAM_CALLBACK(CAPIRequests, OnDownloadHTTPHeader, HTTPRequestHeadersReceived_t);
    STEAM_CALLBACK(CAPIRequests, OnDownloadHTTPData, HTTPRequestDataReceived_t);
    void OnDownloadHTTPComplete(HTTPRequestCompleted_t *pParam, bool bIO);

    // Base HTTP response method, the CallbackFunc is passed the JSON object here
    void OnHTTPResp(HTTPRequestCompleted_t *pParam, bool bIOFailure);
private:
    // Creates an HTTP request with the proper authorization header added. 
    // If bAuth = true, it will add the API key to the request, and will also return false if the key isn't set
    bool CreateAPIRequest(HTTPRequestHandle &handle, const char *pszURL, EHTTPMethod kMethod, bool bAuth = true);
    // Should be called after the HTTP request is prepared by the API calls (above)
    bool SendAPIRequest(HTTPRequestHandle hRequest, CallbackFunc func, const char *pRequest);
    // Check the response for errors, insert error objects in here
    bool CheckAPIResponse(HTTPRequestCompleted_t *pCallback, bool bIOFailure);

    struct APICallback
    {
        APICallback() : handle(INVALID_HTTPREQUEST_HANDLE), callResult(nullptr) {}
        ~APICallback()
        {
            if (callResult)
                delete callResult; // Should call cancel if still in progress
        }
        HTTPRequestHandle handle;
        CallbackFunc callbackFunc;
        CCallResult<CAPIRequests, HTTPRequestCompleted_t> *callResult;
        bool operator==(const APICallback &other) const
        {
            return handle == other.handle;
        }
    };
    CUtlMap<HTTPRequestHandle, APICallback*> m_mapAPICalls;

    struct DownloadCall
    {
        DownloadCall() : handle(INVALID_HTTPREQUEST_HANDLE), completeResult(nullptr)
        {
            m_pszFileName[0] = '\0';
            m_pszFilePathID[0] = '\0';
        }

        ~DownloadCall()
        {
            if (completeResult)
                delete completeResult;
        }
        HTTPRequestHandle handle;
        CCallResult<CAPIRequests, HTTPRequestCompleted_t> *completeResult;

        // The first function called, when the headers are returned for the download request.
        // Data in the response KeyValues:
        //  "request"   (uint64)    The request handle that the download operates under
        //  "size"      (uint64)    The size of the download, in bytes. Will be 0 if the headers did not contain a Content-Length header
        CallbackFunc startFunc;
        // The second function (repeatedly) called when data is transferred from the server to the client.
        // Data in the response KeyValues:
        //  "request"   (uint64)    The request handle that the download operates under
        //  "percent"   (float)     The percent of download completion (NOTE: not very reliable for progress, use offset and size!)
        //  "offset"    (uint64)    The offset (from 0) of the bytes being downloaded
        //  "size"      (uint64)    The size of the chunk of data being downloaded, in bytes
        CallbackFunc progressFunc;
        // The last function to be called when downloading a file.
        // Data in the response KeyValues:
        //  "request"   (uint64)    The request handle that the download operates under
        //  "error"     (bool)      If the request fails in any way, will be true, otherwise false
        //  "code"      (int)       The HTTP status code of the request if it failed, otherwise 0
        CallbackFunc completeFunc;

        char m_pszFileName[MAX_PATH];
        char m_pszFilePathID[16];
        CUtlBuffer m_bufFileData;

        bool operator==(const DownloadCall &other) const
        {
            return handle == other.handle;
        }
    };
    CUtlMap<HTTPRequestHandle, DownloadCall*> m_mapDownloadCalls;

    // Auth ticket impl
    HAuthTicket m_hAuthTicket;
    byte* m_bufAuthBuffer;
    uint32 m_iAuthActualSize;
    // API key for requests
    char *m_pAPIKey;
};

extern CAPIRequests *g_pAPIRequests;