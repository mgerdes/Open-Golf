ANDROID_BUILD_TOOLS="C:\Users\mgerd\AppData\Local\Android\Sdk\build-tools\29.0.0"
ANDROID_SDK_PLATFORM="C:\Users\mgerd\AppData\Local\Android\Sdk\platforms\android-28"
ANDROID_NDK="C:\Users\mgerd\AppData\Local\Android\Sdk\ndk\23.1.7779620"
KEYSTORE="C:\Users\mgerd\.android\debug.keystore"

mkdir -p out
mkdir -p out/android
mkdir -p out/temp
mkdir -p out/compiled_resources
cd out/android
cmake ../.. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-28
cmake --build . --config Debug
cd ../..
$ANDROID_BUILD_TOOLS/aapt package -v -f -S build/android/res -M build/android/AndroidManifest.xml -I $ANDROID_SDK_PLATFORM/android.jar -F golf-unaligned.apk 
$ANDROID_BUILD_TOOLS/aapt add -v golf-unaligned.apk out/android/libnative-golf.so
$ANDROID_BUILD_TOOLS/zipalign -f 4 golf-unaligned.apk golf.apk
$ANDROID_BUILD_TOOLS/keytool -genkeypair -keystore $KEYSTORE -storepass android -alias androiddebugkey -keypass android -keyalg RSA -validity 10000 -dname CN=,OU,O=,L=,S=,C=
