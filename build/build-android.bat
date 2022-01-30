set ANDROID_BUILD_TOOLS=C:\Users\mgerd\AppData\Local\Android\Sdk\build-tools\28.0.3
set ANDROID_SDK_PLATFORM=C:\Users\mgerd\AppData\Local\Android\Sdk\platforms\android-28
set ANDROID_NDK=C:\Users\mgerd\AppData\Local\Android\Sdk\ndk\23.1.7779620
set KEYSTORE=C:\Users\mgerd\.android\debug.keystore

mkdir out
mkdir out/android
mkdir out/temp
mkdir out/compiled_resources
cd out/android
cmake ../.. -G Ninja -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK%\build\cmake\android.toolchain.cmake -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-28
cmake --build . --config Debug
cd ../..
%ANDROID_BUILD_TOOLS%\aapt package -v -f -S build/android/res -M build/android/AndroidManifest.xml -I %ANDROID_SDK_PLATFORM%/android.jar -F golf-unaligned.apk 
%ANDROID_BUILD_TOOLS%\aapt add -v golf-unaligned.apk out/android/libnative-golf.so
%ANDROID_BUILD_TOOLS%\zipalign -f 4 golf-unaligned.apk golf-unsigned.apk
%ANDROID_BUILD_TOOLS%\apksigner sign -v --ks %KEYSTORE% --ks-pass pass:android --key-pass pass:android --ks-key-alias androiddebugkey --out golf.apk golf-unsigned.apk
%ANDROID_BUILD_TOOLS%\apksigner verify -v golf.apk

echo "HI"
