Build and compile D2TP applications
=====================================

D2TP is a TCP-like transport layer protocol for mobile apps,
which handles network disruptions transparently and exploits
users' delay tolerance to maximize Wi-Fi offloading ratio. 

You can build either Linux D2TP application or Android D2TP
application. This file explains how to build D2TP applications
in both environments.

Using D2TP with Android application
----------------------------------

If you want to build your own Android D2TP application,
you need to use Android NDK to make use of D2TP C library in
android applications.

1. Download Android NDK package
   (http://developer.android.com/tools/sdk/ndk/index.html)

2. Extract toolchain from NDK
  ```
  #  Extract ndk package to somewhere (e.g. /home/<username>/) and enter directory
  cd android-ndk-<version>

  # Extract ndk toolchain to somewhere
  ./build/tools/make-standalone-toolchain.sh --platform=android-<API version (5~)> --install-dir=<destination directory>
  # e.g. /build/tools/make-standalone-toolchain.sh --platform=android-9 --install-dir=/tmp/my-android-toolchain
  ```

3. Configure library API with arm-linux-androideabi
  ```
  # make path for cross-compiler
  export CC=arm-linux-androideabi-gcc
  export PATH=<ndk toolchain's location>/bin:$PATH
  # e.g. export PATH=/tmp/my-android-toolchain/bin:$PATH
  ```

  # Goto external library and configure like below and make
  ```
  ./configure --host arm-linux-eabi --build=x86_64 --enable-shared
  make
  sudo make install
  ```

4. Copy result libraries to ndk
  ```
  # enter .libs directory
  cd <library API package dir>/.libs

  # copy all *.a and *.la file to $NDK/platforms/android-9/arch-arm/usr/lib
  cp ./*.a $NDK/platforms/android-9/arch-arm/usr/lib
  ```

5. Build jni library using -l<external library name>
  Example: in <Project>/jni/Android.mk file

  ```
  LOCAL_PATH := $(call my-dir)
  include $(CLEAR_VARS)
  LOCAL_MODULE    := dtn_manager
  LOCAL_SRC_FILES := \
      dtn_manager.c \
      dtn.c \
  LOCAL_C_INCLUDES := $(LOCAL_PATH)/event/include
  LOCAL_LDLIBS = -levent
  include $(BUILD_SHARED_LIBRARY)
  ```

6. And build jni library.
  ```
  android-ndk-<version>/ndk-build -B
  ```

Using D2TP with Linux application
----------------------------------

Required: Libevent (libevent-dev in ubuntu),
          Openssl (libssl-dev in ubuntu)

1. Go to libdtp folder.
  ```
  cd libdtp
  ```

2. Build D2TP library.
  ```
  make
  ```

3. Make your own D2TP application. You can refer to
   `libdtp/src/dtp_socket.h` to check D2TP socket API.
   You need to link `libdtp.a` created in Step 2.
 
 * You can find the sample application in
   `libdtp/test/rate_limited_client_server`.