/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Original file: /home/ygmoon/workspace/ReadyCast/src/dtn/net/service/IDownloadCallback.aidl
 */
package dtn.net.service;
public interface IDownloadCallback extends android.os.IInterface
{
/** Local-side IPC implementation stub class. */
public static abstract class Stub extends android.os.Binder implements dtn.net.service.IDownloadCallback
{
private static final java.lang.String DESCRIPTOR = "dtn.net.service.IDownloadCallback";
/** Construct the stub at attach it to the interface. */
public Stub()
{
this.attachInterface(this, DESCRIPTOR);
}
/**
 * Cast an IBinder object into an dtn.net.service.IDownloadCallback interface,
 * generating a proxy if needed.
 */
public static dtn.net.service.IDownloadCallback asInterface(android.os.IBinder obj)
{
if ((obj==null)) {
return null;
}
android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
if (((iin!=null)&&(iin instanceof dtn.net.service.IDownloadCallback))) {
return ((dtn.net.service.IDownloadCallback)iin);
}
return new dtn.net.service.IDownloadCallback.Stub.Proxy(obj);
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
case TRANSACTION_valueChanged:
{
data.enforceInterface(DESCRIPTOR);
int _arg0;
_arg0 = data.readInt();
this.valueChanged(_arg0);
return true;
}
case TRANSACTION_setContentLength:
{
data.enforceInterface(DESCRIPTOR);
long _arg0;
_arg0 = data.readLong();
this.setContentLength(_arg0);
return true;
}
case TRANSACTION_setNotFound:
{
data.enforceInterface(DESCRIPTOR);
this.setNotFound();
return true;
}
}
return super.onTransact(code, data, reply, flags);
}
private static class Proxy implements dtn.net.service.IDownloadCallback
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
@Override public void valueChanged(int value) throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
try {
_data.writeInterfaceToken(DESCRIPTOR);
_data.writeInt(value);
mRemote.transact(Stub.TRANSACTION_valueChanged, _data, null, android.os.IBinder.FLAG_ONEWAY);
}
finally {
_data.recycle();
}
}
@Override public void setContentLength(long contentLength) throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
try {
_data.writeInterfaceToken(DESCRIPTOR);
_data.writeLong(contentLength);
mRemote.transact(Stub.TRANSACTION_setContentLength, _data, null, android.os.IBinder.FLAG_ONEWAY);
}
finally {
_data.recycle();
}
}
@Override public void setNotFound() throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
try {
_data.writeInterfaceToken(DESCRIPTOR);
mRemote.transact(Stub.TRANSACTION_setNotFound, _data, null, android.os.IBinder.FLAG_ONEWAY);
}
finally {
_data.recycle();
}
}
}
static final int TRANSACTION_valueChanged = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
static final int TRANSACTION_setContentLength = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
static final int TRANSACTION_setNotFound = (android.os.IBinder.FIRST_CALL_TRANSACTION + 2);
}
public void valueChanged(int value) throws android.os.RemoteException;
public void setContentLength(long contentLength) throws android.os.RemoteException;
public void setNotFound() throws android.os.RemoteException;
}
