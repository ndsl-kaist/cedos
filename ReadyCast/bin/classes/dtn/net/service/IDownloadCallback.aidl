package dtn.net.service;
 
oneway interface IDownloadCallback {
    void valueChanged(int value);
    void setContentLength(long contentLength);
    void setNotFound();
}