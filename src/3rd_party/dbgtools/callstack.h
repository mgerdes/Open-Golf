/*
	dbgtools - platform independent wrapping of "nice to have" debug functions.
	
	https://github.com/wc-duck/dbgtools

	version 0.1, october, 2013

	Copyright (C) 2013- Fredrik Kihlander

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

	Fredrik Kihlander
 */

#ifndef DEBUG_CALLSTACK_H_INCLUDED
#define DEBUG_CALLSTACK_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct
{
	const char*  function; ///< name of function containing address of function.
	const char*  file;     ///< file where symbol is defined, might not work on all platforms.
	unsigned int line;     ///< line in file where symbol is defined, might not work on all platforms.
	unsigned int offset;   ///< offset from start of function where call was made.
} callstack_symbol_t;

/**
 * Generate a callstack from the current location in the code.
 * @param skip_frames number of frames to skip in output to addresses.
 * @param addresses is a pointer to a buffer where to store addresses in callstack.
 * @param num_addresses size of addresses.
 * @return number of addresses in callstack.
 */
int callstack( int skip_frames, void** addresses, int num_addresses );

/**
 * Translate addresses from, for example, callstack to symbol-names.
 * @param addresses list of pointers to translate.
 * @param out_syms list of callstack_symbol_t to fill with translated data, need to fit as many strings as there are ptrs in addresses.
 * @param num_addresses number of addresses in addresses
 * @param memory memory used to allocate strings stored in out_syms.
 * @param mem_size size of addresses.
 * @return number of addresses translated.
 *
 * @note On windows this will load dbghelp.dll dynamically from the following paths:
 *       1) same path as the current module (.exe)
 *       2) current working directory.
 *       3) the usual search-paths ( PATH etc ).
 *
 *       Some thing to be wary of is that if you are using symbol-server functionality symsrv.dll MUST reside together with
 *       the dbghelp.dll that is loaded as dbghelp.dll will only load that from the same path as where it self lives.
 *
 * @note On windows .pdb search paths will be set in the same way as dbghelp-defaults + the current module (.exe) dir, i.e.:
 *       1) same path as the current module (.exe)
 *       2) current working directory.
 *       3) The _NT_SYMBOL_PATH environment variable.
 *       4) The _NT_ALTERNATE_SYMBOL_PATH environment variable.
 *
 * @note On platforms that support it debug-output can be enabled by defining the environment variable DBGTOOLS_SYMBOL_DEBUG_OUTPUT.
 */
int callstack_symbols( void** addresses, callstack_symbol_t* out_syms, int num_addresses, char* memory, int mem_size );

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif // DEBUG_CALLSTACK_H_INCLUDED
