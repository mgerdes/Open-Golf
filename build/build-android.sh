mkdir -p out
mkdir -p out/android
mkdir -p out/temp
mkdir -p out/compiled_resources
cd out/android
cmake ../.. -G Ninja -DCMAKE_TOOLCHAIN_FILE=/home/michael/Android/Sdk/ndk/23.1.7779620/build/cmake/android.toolchain.cmake -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-28
cmake --build . --config Debug
cd ../..
/home/michael/Android/Sdk/build-tools/28.0.3/aapt package -v -f -S build/android/res -M build/android/AndroidManifest.xml -I /home/michael/Android/Sdk/platforms/android-28/android.jar -F golf-unaligned.apk 
/home/michael/Android/Sdk/build-tools/28.0.3/aapt add -v golf-unaligned.apk out/android/libnative-golf.so
/home/michael/Android/Sdk/build-tools/28.0.3/zipalign -f 4 golf-unaligned.apk golf.apk
