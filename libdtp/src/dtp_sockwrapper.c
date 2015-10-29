#include <stdio.h>
#include <jni.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "debug.h"
#include "dtp_socket.h"
#include "fcntl.h"

/*
  #define __ANDROID_LOG_H_
  #include <android/log.h>

  #ifdef __ANDROID_LOG_H_
  #define LOGV(...)   __android_log_print(ANDROID_LOG_VERBOSE, "libnav", __VA_ARGS__)
  #define LOGD(...)   __android_log_print(ANDROID_LOG_DEBUG, "libnav", __VA_ARGS__)
  #define LOGI(...)   __android_log_print(ANDROID_LOG_INFO, "libnav", __VA_ARGS__)
  #define LOGW(...)   __android_log_print(ANDROID_LOG_WARN, "libnav", __VA_ARGS__)
  #define LOGE(...)   __android_log_print(ANDROID_LOG_ERROR, "libnav", __VA_ARGS__)
  #endif
*/

#define BUFSIZE 32000

int is_dtp = 1;

jint
Java_dtn_net_service_DTPNetThread_dtpsocket(JNIEnv* env, jobject thiz) {
  
	if (is_dtp)
		return dtp_socket();
	else
		return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

jint
Java_dtn_net_service_DTPNetThread_dtpconnect(JNIEnv* env, jobject thiz, jint socket, jstring strip, jint port) {
  
	struct sockaddr_in serv_addr;
	int sock = socket;

	memset(&serv_addr, 0, sizeof(serv_addr));
  
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr((*env)->GetStringUTFChars(env, strip, 0));
	serv_addr.sin_port = htons(port);

	if (is_dtp) {
		dtp_fcntl(sock, DTP_F_SETFL, O_NONBLOCK);
	}
  
	if (is_dtp)
		return dtp_connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	else
		return connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));  
}


jint
Java_dtn_net_service_DTPNetThread_dtpread(JNIEnv* env, jobject thiz, jint socket, jbyteArray jbuf, jint size) {

	char buf[BUFSIZE];
	int recvsize;
  
  	if (is_dtp)
		recvsize = dtp_read(socket, buf, size);
	else
		recvsize = read(socket, buf, size);

	if (recvsize == -1)
		recvsize = recvsize * errno;

	if (recvsize > 0)
		(*env) -> SetByteArrayRegion(env, jbuf, 0, recvsize, (jbyte*) buf);

	return recvsize;
}

jint
Java_dtn_net_service_DTPNetThread_dtpwrite(JNIEnv* env, jobject thiz, jint socket, jbyteArray buf, jint size) {
  
	int res;
	int off=0;
	//jint size = (*env) -> GetArrayLength(env, buf);
  
	jbyte *pbyte = (*env)->GetByteArrayElements(env, buf, 0);
  
	char err[] = "ERROR";

	while (size > 0){
		if (is_dtp){
			if ((res = dtp_write(socket, pbyte+off, size)) == -1) {
				return -1;
			}
		} else {
			if ((res = write(socket, pbyte+off, size)) == -1) {
				return -1;
			}
		}
		if (res <= 0)
			break;
		size -= res;
		off += res;
	}

	(*env)->ReleaseByteArrayElements(env, buf, pbyte, JNI_ABORT);

	return 0;
}

jint
Java_dtn_net_service_DTPNetThread_dtploginit(JNIEnv* env, jobject thiz, jbyteArray path, jint size,
											 jbyteArray hid, jbyteArray imei, jint size2) {
  
	int ret;
	char hbuf[40];

	jbyte *pbyte = (*env)->GetByteArrayElements(env, path, 0);
	jbyte *pbyte2 = (*env)->GetByteArrayElements(env, imei, 0);

	ret = dtp_loginit(pbyte, size, hbuf, pbyte2, size2);

	(*env)->SetByteArrayRegion(env, hid, 0, 40, (jbyte*) hbuf);
	(*env)->ReleaseByteArrayElements(env, path, pbyte, JNI_ABORT);

	return ret;
}

jint
Java_dtn_net_service_DTPNetThread_dtpclose(JNIEnv* env, jobject thiz, jint socket)
{
	if (is_dtp)
		return dtp_close(socket);
	else
		return close(socket);
}

jint
Java_dtn_net_service_DTPNetThread_dtpgetlocktime(JNIEnv* env, jobject thiz)
{
	if (is_dtp)
		return dtp_getlocktime();
	else
		return -1;
}


jint 
Java_dtn_net_service_DTPNetThread_dtpsetsockopt(JNIEnv* env, jobject thiz, jint socket,
												  jint level, jint optname, jint optval)
{
	int optval_i = optval;

	if (is_dtp)
		return dtp_setsockopt(socket, level, optname, &optval_i, sizeof(optval_i));
	else
		return setsockopt(socket, level, optname, &optval_i, sizeof(optval_i));
}
 
jint
Java_dtn_net_service_DTPNetThread_dtpgetsockopt(JNIEnv* env, jobject thiz, jint socket,
												jint level, jint optname, jint optval)
{
    int ret;
    int optval_i;
    int optval_len;

    if (is_dtp)
        ret = dtp_getsockopt(socket, level, optname, &optval_i, &optval_len);
    else
        ret = getsockopt(socket, level, optname, &optval_i, &optval_len);

    if (ret < 0)
		return -1;
    else
        return optval_i;
}

jint
Java_dtn_net_service_DTPNetThread_dtpselect(JNIEnv* env, jobject thiz,
                                            jint maxfd, jbyteArray readfd, jint t)
{
	fd_set readfs;
	struct timeval timeout;
	int ret;

	FD_ZERO(&readfs);

	jbyte *pbyte = (*env)->GetByteArrayElements(env, readfd, 0);

	int i;
	for (i = 0; i < maxfd + 1; i++) {
		if (pbyte[i] > 0) {
			FD_SET(i, &readfs);
		}
	}
	(*env)->ReleaseByteArrayElements(env, readfd, pbyte, JNI_ABORT);

	timeout.tv_sec = t;
	timeout.tv_usec = 0;

	if (is_dtp)
		ret = dtp_select(maxfd + 1, &readfs, NULL, NULL, &timeout);
	else
		ret = select(maxfd + 1, &readfs, NULL, NULL, &timeout);


	if (ret < 0)
		return -1;

	for (i = 0; i < maxfd + 1; i++) {
		if (FD_ISSET(i, &readfs)) {
			return i;
		}
	}

	if (ret == 0)
		return -2;


	return -2;
}


jint
Java_dtn_net_service_DTPNetThread_dtpgetsocklog(JNIEnv* env, jobject thiz, jint socket,
												jint level, jint optname, jint optval)
{
    int ret;
    int optval_i;

    if (is_dtp)
        ret = dtp_getsocklog(socket, level, optname, &optval_i, sizeof(optval_i));
    else
        ret = -1;

    if (ret < 0)
		return -1;
    else
        return optval_i;
}


