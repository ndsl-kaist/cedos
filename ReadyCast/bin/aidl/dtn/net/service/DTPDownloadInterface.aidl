package dtn.net.service;

import dtn.net.service.IDownloadCallback;


interface DTPDownloadInterface
{
	String registerDownload(IDownloadCallback cb, String url, String filepath, int deadlineSec);
	int connectToDownload(IDownloadCallback cb, String url);
	void unregisterDownload(String url);
	void finishDownloads();
	void systemShutdown();
    
}