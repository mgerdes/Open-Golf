#include "golf/thread.h"

#if GOLF_PLATFORM_WINDOWS

#pragma comment( lib, "winmm.lib" )

#define _CRT_NONSTDC_NO_DEPRECATE 
#define _CRT_SECURE_NO_WARNINGS

#if !defined( _WIN32_WINNT ) || _WIN32_WINNT < 0x0501 
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x501// requires Windows XP minimum
#endif

#define _WINSOCKAPI_
#pragma warning( push )
#pragma warning( disable: 4668 ) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning( disable: 4255 )
#include <windows.h>
#pragma warning( pop )

// To set thread name
const DWORD _MS_VC_EXCEPTION = 0x406D1388;
#pragma pack( push, 8 )
typedef struct tagTHREADNAME_INFO
    {
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
    } THREADNAME_INFO;
#pragma pack(pop)

#elif GOLF_PLATFORM_LINUX || GOLF_PLATFORM_IOS || GOLF_PLATFORM_ANDROID

#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>

#else
#error Unknown platform
#endif

golf_thread_t golf_thread_create(int (*proc)(void*), void *user_data, const char *name) {
#if GOLF_PLATFORM_WINDOWS

    DWORD thread_id;
    HANDLE handle = CreateThread( NULL, 0U, (LPTHREAD_START_ROUTINE)(uintptr_t) proc, user_data, 0, &thread_id );
    if( !handle ) return NULL;

    // Yes, this crazy construct with __try and RaiseException is how you name a thread in Visual Studio :S
    if( name && IsDebuggerPresent() )
    {
        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name;
        info.dwThreadID = thread_id;
        info.dwFlags = 0;

        __try
        {
            RaiseException( _MS_VC_EXCEPTION, 0, sizeof( info ) / sizeof( ULONG_PTR ), (ULONG_PTR*) &info );
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
        }
    }

    return (golf_thread_t) handle;

#elif GOLF_PLATFORM_LINUX || GOLF_PLATFORM_IOS || GOLF_PLATFORM_ANDROID 

    pthread_t thread;
    if( 0 != pthread_create( &thread, NULL, ( void* (*)( void * ) ) proc, user_data ) )
        return NULL;

    return (golf_thread_t) thread;

#else
#error Unknown platform
    return NULL;
#endif
}

void golf_thread_destroy(golf_thread_t thread) {
}

int golf_thread_join(golf_thread_t thread) {
    return 0;
}

void golf_mutex_init(golf_mutex_t *mutex) {
#if GOLF_PLATFORM_WINDOWS

    // Compile-time size check
#pragma warning( push )
#pragma warning( disable: 4214 ) // nonstandard extension used: bit field types other than int
    struct x { char thread_mutex_type_too_small : ( sizeof( golf_mutex_t ) < sizeof( CRITICAL_SECTION ) ? 0 : 1 ); }; 
#pragma warning( pop )

    InitializeCriticalSectionAndSpinCount( (CRITICAL_SECTION*) mutex, 32 );

#elif GOLF_PLATFORM_LINUX || GOLF_PLATFORM_IOS || GOLF_PLATFORM_ANDROID

    // Compile-time size check
    struct x { char thread_mutex_type_too_small : ( sizeof( golf_mutex_t ) < sizeof( pthread_mutex_t ) ? 0 : 1 ); };

    pthread_mutex_init( (pthread_mutex_t*) mutex, NULL );

#else
#error Unknown platform.
#endif
}

void golf_mutex_deinit(golf_mutex_t *mutex) {
#if GOLF_PLATFORM_WINDOWS

    DeleteCriticalSection( (CRITICAL_SECTION*) mutex );

#elif GOLF_PLATFORM_LINUX || GOLF_PLATFORM_IOS || GOLF_PLATFORM_ANDROID

    pthread_mutex_destroy( (pthread_mutex_t*) mutex );

#else
#error Unknown platform.
#endif
}

void golf_mutex_lock(golf_mutex_t *mutex) {
#if GOLF_PLATFORM_WINDOWS

    EnterCriticalSection( (CRITICAL_SECTION*) mutex );

#elif GOLF_PLATFORM_LINUX || GOLF_PLATFORM_IOS || GOLF_PLATFORM_ANDROID

    pthread_mutex_lock( (pthread_mutex_t*) mutex );

#else
#error Unknown platform.
#endif
}

void golf_mutex_unlock(golf_mutex_t *mutex) {
#if GOLF_PLATFORM_WINDOWS

    LeaveCriticalSection( (CRITICAL_SECTION*) mutex );

#elif GOLF_PLATFORM_LINUX || GOLF_PLATFORM_IOS || GOLF_PLATFORM_ANDROID

    pthread_mutex_unlock( (pthread_mutex_t*) mutex );

#else
#error Unknown platform.
#endif
}
