// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//

inline void INIT_LOG( const LPCWSTR testname )
{
	wprintf( L"Beginning test: %s\n", testname );
}

inline void END_LOG( const LPCWSTR testname ) 
{
	wprintf( L"End of test: %s\n", testname );
}

inline void LOG_FAILURE_HRESULT_AND_EXIT( HRESULT hr, const LPCWSTR errormessage, int LINE ) 
{
	wprintf( errormessage, hr );
	wprintf( L"  Line: %d\n", LINE );

	exit( LINE );
}

inline void LOG_FAILURE_AND_EXIT( const LPCWSTR errormessage, int LINE ) 
{
	wprintf( errormessage );
	wprintf( L"  Line: %d\n", LINE );

	exit( LINE );
}

inline void LOG_FAILURE_HRESULT( HRESULT hr, const LPCWSTR errormessage, int LINE ) 
{
	wprintf( errormessage, hr );
	wprintf( L"  Line: %d\n", LINE );
}

inline void LOG_FAILURE( const LPCWSTR errormessage, int LINE ) 
{
	wprintf( errormessage );
	wprintf( L"  Line: %d\n", LINE );
}
