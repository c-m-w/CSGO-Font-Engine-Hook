/// CSGO.cpp

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <cstdio>
#include <thread>

constexpr char FONT[ ] = R"(C:\Windows\Fonts\comic.ttf)";
unsigned char *pFontData = nullptr;
std::size_t zFontSize = 0;
HMODULE hFreeType = nullptr;
void *pNewFace = nullptr;
void *pNewMemoryFace = nullptr;

unsigned char *ReadFontData( );
bool PatchIAT( HMODULE hModule );
int FT_New_Face( void *library, const char *filepathname, long face_index, void *aface );
int FT_New_Memory_Face( void *library, const unsigned char *file_base, long file_size, long face_index, void *aface );

DWORD WINAPI ThreadProc(
	_In_ LPVOID lpParameter
)
{
	if ( ( pFontData = ReadFontData( ) ) == nullptr )
		return FALSE;

	do
	{
		hFreeType = GetModuleHandle( L"libfreetype-6.dll" );
	} while ( hFreeType == nullptr );

	pNewFace = decltype( pNewFace )( GetProcAddress( hFreeType, "FT_New_Face" ) );
	pNewMemoryFace = decltype( pNewMemoryFace )( GetProcAddress( hFreeType, "FT_New_Memory_Face" ) );

	if ( !PatchIAT( GetModuleHandle( L"libpangoft2-1.0-0.dll" ) )
		 || !PatchIAT( GetModuleHandle( L"libfontconfig-1.dll" ) )
		 || !PatchIAT( GetModuleHandle( L"panorama_text_pango.dll" ) ) )
		return FALSE;

	while ( true )
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

	FreeLibraryAndExitThread( *reinterpret_cast< HMODULE * >( lpParameter ), TRUE );
}

BOOL WINAPI DllMain(
	_In_ HINSTANCE hinstDLL,
	_In_ DWORD     fdwReason,
	_In_ LPVOID    lpvReserved
)
{
	if ( fdwReason != DLL_PROCESS_ATTACH )
		return TRUE;

	DisableThreadLibraryCalls( hinstDLL );
	CreateThread( nullptr, 0, ThreadProc, &hinstDLL, 0, nullptr );
	return TRUE;
}

unsigned char *ReadFontData( )
{
	const auto pFont = fopen( FONT, "rb" );
	if ( pFont == nullptr )
		return nullptr;

	fseek( pFont, 0, SEEK_END );
	zFontSize = ftell( pFont );
	const auto pReturn = new unsigned char[ zFontSize + 1 ];
	memset( pReturn, 0, zFontSize + 1 );
	rewind( pFont );
	fread( pReturn, sizeof( unsigned char ), zFontSize, pFont );
	fclose( pFont );
	return pReturn;
}

bool PatchIAT( HMODULE hModule )
{
	const auto pDOSHeaders = reinterpret_cast< IMAGE_DOS_HEADER * >( hModule );
	const auto pNTHeaders = reinterpret_cast< IMAGE_NT_HEADERS * >( std::uintptr_t( hModule ) + pDOSHeaders->e_lfanew );
	const auto pImportList = reinterpret_cast< IMAGE_IMPORT_DESCRIPTOR * >( std::uintptr_t( hModule ) + pNTHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ].VirtualAddress );

	for ( auto i = 0; ; i++ )
	{
		const auto pImport = pImportList[ i ];
		if ( pImport.Characteristics == 0 )
			break;

		if ( strcmp( reinterpret_cast< const char * >( std::uintptr_t( hModule ) + pImport.Name ), "libfreetype-6.dll" ) != 0 )
			continue;

		for ( auto j = 0; ; j++ )
		{
			constexpr auto fnModifyAddress = [ ]( void **pAddress, std::uintptr_t ptrValue ) -> bool
			{
				DWORD dwOld;
				if ( VirtualProtect( pAddress, sizeof( void * ), PAGE_READWRITE, &dwOld ) == FALSE )
					return false;

				*reinterpret_cast < std::uintptr_t* >( pAddress ) = ptrValue;
				return VirtualProtect( pAddress, sizeof( void * ), dwOld, &dwOld ) != FALSE;
			};

			auto &o = reinterpret_cast< IMAGE_THUNK_DATA * >( std::uintptr_t( hModule ) + pImport.OriginalFirstThunk )[ j ],
				&n = reinterpret_cast< IMAGE_THUNK_DATA * >( std::uintptr_t( hModule ) + pImport.FirstThunk )[ j ];

			if ( o.u1.AddressOfData == NULL )
				break;

			const auto szImport = reinterpret_cast< const char * >( ( o.u1.Ordinal & IMAGE_ORDINAL_FLAG ) > 0 ? IMAGE_ORDINAL( o.u1.Ordinal )
																	: std::uintptr_t( reinterpret_cast< IMAGE_IMPORT_BY_NAME * >( std::uintptr_t( hModule ) + o.u1.AddressOfData )->Name ) );
			if ( strcmp( szImport, "FT_New_Face" ) == 0 )
			{
				if ( !fnModifyAddress( reinterpret_cast< void ** >( &n.u1.Function ), std::uintptr_t( FT_New_Face ) ) )
					return false;
			}
			else if ( strcmp( szImport, "FT_New_Memory_Face" ) == 0 )
			{
				if ( !fnModifyAddress( reinterpret_cast< void ** >( &n.u1.Function ), std::uintptr_t( FT_New_Memory_Face ) ) )
					return false;
			}
		}
	}

	return true;
}

int FT_New_Face( void *library, const char *filepathname, long face_index, void *aface )
{
	return reinterpret_cast< decltype( FT_New_Face )* >( pNewFace )( library, FONT, face_index, aface );
}

int FT_New_Memory_Face( void *library, const unsigned char *file_base, long file_size, long face_index, void *aface )
{
	return reinterpret_cast< decltype( FT_New_Memory_Face )* >( pNewMemoryFace )( library, pFontData, zFontSize, face_index, aface );
}
