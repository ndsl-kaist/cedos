/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Original file: /home/ygmoon/workspace/ReadyCast/src/dtn/net/service/DTPDownloadInterface.aidl
 */
package dtn.net.service;
public interface DTPDownloadInterface extends android.os.IInterface
{
/** Local-side IPC implementation stub class. */
public static abstract class Stub extends android.os.Binder implements dtn.net.service.DTPDownloadInterface
{
private static final java.lang.String DESCRIPTOR = "dtn.net.service.DTPDownloadInterface";
/** Construct the stub at attach it to the interface. */
public Stub()
{
this.attachInterface(this, DESCRIPTOR);
}
/**
 * Cast an IBinder object into an dtn.net.service.DTPDownloadInterface interface,
 * generating a proxy if needed.
 */
public static dtn.net.service.DTPDownloadInterface asInterface(android.os.IBinder obj)
{
if ((obj==null)) {
return null;
}
android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
if (((iin!=null)&&(iin instanceof dtn.net.service.DTPDownloadInterface))) {
return ((dtn.net.service.DTPDownloadInterface)iin);
}
return new dtn.net.service.DTPDownloadInterface.Stub.Proxy(obj);
}
@Override public android.os.IBinder asBinder()
{
return this;
}
@Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
{
switch (code)
{
case INTERFACE_TRANSACTION:
{
reply.writeString(DESCRIPTOR);
return true;
}
case TRANSACTION_registerDownload:
{
data.enforceInterface(DESCRIPTOR);
dtn.net.service.IDownloadCallback _arg0;
_arg0 = dtn.net.service.IDownloadCallback.Stub.asInterface(data.readStrongBinder());
java.lang.String _arg1;
_arg1 = data.readString();
java.lang.String _arg2;
_arg2 = data.readString();
int _arg3;
_arg3 = data.readInt();
java.lang.String _result = this.registerDownload(_arg0, _arg1, _arg2, _arg3);
reply.writeNoException();
reply.writeString(_result);
return true;
}
case TRANSACTION_connectToDownload:
{
data.enforceInterface(DESCRIPTOR);
dtn.net.service.IDownloadCallback _arg0;
_arg0 = dtn.net.service.IDownloadCallback.Stub.asInterface(data.readStrongBinder());
java.lang.String _arg1;
_arg1 = data.readString();
int _result = this.connectToDownload(_arg0, _arg1);
reply.writeNoException();
reply.writeInt(_result);
return true;
}
case TRANSACTION_unregisterDownload:
{
data.enforceInterface(DESCRIPTOR);
java.lang.String _arg0;
_arg0 = data.readString();
this.unregisterDownload(_arg0);
reply.writeNoException();
return true;
}
case TRANSACTION_finishDownloads:
{
data.enforceInterface(DESCRIPTOR);
this.finishDownloads();
reply.writeNoException();
return true;
}
case TRANSACTION_systemShutdown:
{
data.enforceInterface(DESCRIPTOR);
this.systemShutdown();
reply.writeNoException();
return true;
}
}
return super.onTransact(code, data, reply, flags);
}
private static class Proxy implements dtn.net.service.DTPDownloadInterface
{
private android.os.IBinder mRemote;
Proxy(android.os.IBinder remote)
{
mRemote = remote;
}
@Override public android.os.IBinder asBinder()
{
return mRemote;
}
public java.lang.String getInterfaceDescriptor()
{
return DESCRIPTOR;
}
@Override public java.lang.String registerDownload(dtn.net.service.IDownloadCallback cb, java.lang.String url, java.lang.String filepath, int deadlineSec) throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
java.lang.String _result;
try {
_data.writeInterfaceToken(DESCRIPTOR);
_data.writeStrongBinder((((cb!=null))?(cb.asBinder()):(null)));
_data.writeString(url);
_data.writeString(filepath);
_data.writeInt(deadlineSec);
mRemote.transact(Stub.TRANSACTION_registerDownload, _data, _reply, 0);
_reply.readException();
_result = _reply.readString();
}
finally {
_reply.recycle();
_data.recycle();
}
return _result;
}
@Override public int connectToDownload(dtn.net.service.IDownloadCallback cb, java.lang.String url) throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
int _result;
try {
_data.writeInterfaceToken(DESCRIPTOR);
_data.writeStrongBinder((((cb!=null))?(cb.asBinder()):(null)));
_data.writeString(url);
mRemote.transact(Stub.TRANSACTION_connectToDownload, _data, _reply, 0);
_reply.readException();
_result = _reply.readInt();
}
finally {
_reply.recycle();
_data.recycle();
}
return _result;
}
@Override public void unregisterDownload(java.lang.String url) throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
try {
_data.writeInterfaceToken(DESCRIPTOR);
_data.writeString(url);
mRemote.transact(Stub.TRANSACTION_unregisterDownload, _data, _reply, 0);
_reply.readException();
}
finally {
_reply.recycle();
_data.recycle();
}
}
@Override public void finishDownloads() throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
try {
_data.writeInterfaceToken(DESCRIPTOR);
mRemote.transact(Stub.TRANSACTION_finishDownloads, _data, _reply, 0);
_reply.readException();
}
finally {
_reply.recycle();
_data.recycle();
}
}
@Override public void systemShutdown() throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
try {
_data.writeInterfaceToken(DESCRIPTOR);
mRemote.transact(Stub.TRANSACTION_systemShutdown, _data, _reply, 0);
_reply.readException();
}
finally {
_reply.recycle();
_data.recycle();
}
}
}
static final int TRANSACTION_registerDownload = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
static final int TRANSACTION_connectToDownload = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
static final int TRANSACTION_unregisterDownload = (android.os.IBinder.FIRST_CALL_TRANSACTION + 2);
static final int TRANSACTION_finishDownloads = (android.os.IBinder.FIRST_CALL_TRANSACTION + 3);
static final int TRANSACTION_systemShutdown = (android.os.IBinder.FIRST_CALL_TRANSACTION + 4);
}
public java.lang.String registerDownload(dtn.net.service.IDownloadCallback cb, java.lang.String url, java.lang.String filepath, int deadlineSec) throws android.os.RemoteException;
public int connectToDownload(dtn.net.service.IDownloadCallback cb, java.lang.String url) throws android.os.RemoteException;
public void unregisterDownload(java.lang.String url) throws android.os.RemoteException;
public void finishDownloads() throws android.os.RemoteException;
public void systemShutdown() throws android.os.RemoteException;
}
