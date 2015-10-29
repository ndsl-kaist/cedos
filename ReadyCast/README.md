Build and compile ReadyCast (Android D2TP app)
=====================================

* Required: Eclipse IDE for Java Developers, JDK (Java Development Kit), Android SDK
* Optional: Android NDK


Build ReadyCast without D2TP modification (default)
----------------------------------------------------

1. Install Eclipse IDE for Java developers, JDK, Android SDK
   to setup the Android development environment.
   * https://eclipse.org/downloads/
   * http://www.oracle.com/technetwork/java/javase/downloads/jdk8-downloads-2133151.html
   * http://developer.android.com/sdk/installing/index.html > Select 'STAND-ALONE SDK TOOLS'.

2. Run eclipse and import ReadyCast project.
 * [File] > [Import] > [Import existing project]
 * Select `ReadyCast` folder.

3. Click `Run` button to build and compile the project.


Build ReadyCast with modified D2TP (advanced)
----------------------------------------------

If you want to apply your changes in D2TP and build ReadyCast,
you need to create D2TP library object file on your own.

Option 1. Follow the steps in 'Using D2TP with Android application'
          in [../libdtp/README.md].

Option 2. Download the NDK package from our website and use it.
          (http://cedos.kaist.edu/android-ndk-r8d.tar.gz)

1. Download the package and extract the files from it.
  ```
  wget http://cedos.kaist.edu/android-ndk-r8d.tar.gz
  tar xvfz http://cedos.kaist.edu/android-ndk-r8d.tar.gz
  ```

2. Compile D2TP library in ReadyCast with the ndk
  ```
  (PATH_TO_NDK)/ndk-build -B
  ```

3. Run eclipse and import ReadyCast project.
 * [File] > [Import] > [Import existing project]
 * Select `ReadyCast` folder.

4. Click `Run` button to build and compile the project.