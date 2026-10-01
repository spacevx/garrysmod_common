#include "Symbols.hpp"
#include "Symbol.hpp"
#include "Platform.hpp"

namespace Symbols
{

	const std::vector<Symbol> CBasePlayer_HandleClientLuaError = {
		Symbol::FromName( "?HandleClientLuaError@@YAXPEAVCBasePlayer@@PEBD@Z" ),
		Symbol::FromSignature( "\x48\x89\x54\x24\x10\x48\x89\x4C\x24\x08\x55\x41\x54\x41\x57\x48\x8D\xAC\x24\x50\xFD\xFF\xFF" )
	};

	const std::vector<Symbol> FileSystemFactory = {
		Symbol::FromName( "?FileSystemFactory@@YAPEAXPEBDPEAH@Z" )
	};

	const Symbol g_pFullFileSystem = Symbol::FromName( "?g_pFullFileSystem@@3PEAVIFileSystem@@EA" );

	const std::vector<Symbol> IServer = {
		Symbol::FromName( "?sv@@3VCGameServer@@A" ),
		Symbol::FromSignature( "\x48\x8D\x0D\x2A\x2A\x2A\x2A\xE8\x2A\x2A\x2A\x2A\x84\xC0\x74\x2A\x83\x3D\x2A\x2A\x2A\x2A\x03", 3 )
	};

	const std::vector<Symbol> CNetChan_ProcessMessages = {
		Symbol::FromName( "?ProcessMessages@CNetChan@@AEAA_NAEAVbf_read@@@Z" ),
		Symbol::FromSignature( "\x40\x53\x56\x57\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x48\xF7\x05\x2A\x2A\x2A\x2A\x00\x10\x00\x00" )
	};

	const std::vector<Symbol> CBaseClient_ConnectionStart = {
		Symbol::FromName( "?ConnectionStart@CBaseClient@@UEAAXPEAVINetChannel@@@Z" ),
		Symbol::FromSignature( "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x41\x56\x41\x57\x48\x83\xEC\x20\x48\x8B\xE9\x48\x8B\xF2" )
	};

	const std::vector<Symbol> CBaseClientState_ConnectionStart = {
		Symbol::FromName( "?ConnectionStart@CBaseClientState@@UEAAXPEAVINetChannel@@@Z" ),
		Symbol::FromSignature( "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x20\x48\x8B\xE9\x48\x8B\xFA" )
	};

	const std::vector<Symbol> CLC_CmdKeyValues_Constructor = {
		Symbol::FromSignature( "\x48\x8D\x05\x69\x2A\x2A\x2A\xC6\x41\x2A\x01\x48\x89\x01\x48\x8B\xC1\x48\xC7\x41\x2A\x00\x00\x00\x00\x48\x89\x51\x2A\xC3" )
	};

	const std::vector<Symbol> SVC_CreateStringTable_Constructor = {
		Symbol::FromSignature( "\x40\x53\x48\x83\xEC\x20\x48\x8D\x05\x2A\x2A\x2A\x2A\xC6\x41\x08\x01\x48\x89\x01" )
	};

	const std::vector<Symbol> SVC_CmdKeyValues_Constructor = {
		Symbol::FromSignature( "\x48\x8D\x05\xC9\x2A\x2A\x2A\xC6\x41\x2A\x01\x48\x89\x01\x48\x8B\xC1\x48\xC7\x41\x2A\x00\x00\x00\x00\x48\x89\x51\x2A\xC3" )
	};

	const std::vector<Symbol> CBaseServer_RecalculateTags = {
		Symbol::FromName( "?RecalculateTags@CBaseServer@@QEAAXXZ" )
	};

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif

	const std::vector<Symbol> SteamGameServerAPIContext = {
		Symbol::FromName( "?s_SteamGameServerAPIContext@@3VCSteamGameServerAPIContext@@A" )
	};

#ifdef _MSC_VER
#pragma warning( pop )
#endif

	const std::vector<Symbol> GModDataPack_SendFileToClient = {
		Symbol::FromName( "?SendFileToClient@GModDataPack@@QEAAXHH@Z" ),
		Symbol::FromSignature( "\x40\x55\x56\x41\x54\x41\x57\x48\x8D\x6C\x24\xC1\x48\x81\xEC\xA8\x00\x00\x00" )
	};

	const std::vector<Symbol> CNetChan_IsValidFileForTransfer = {
		Symbol::FromName( "?IsValidFileForTransfer@CNetChan@@SA_NPEBD@Z" ),
		Symbol::FromSignature( "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x48\x8B\xF1\x48\x85\xC9" )
	};

	const std::vector<Symbol> net_sockets = {
		Symbol::FromSignature( "\x48\x8B\x0D\x2A\x2A\x2A\x2A\x8B\x41\x40\x44\x8B\x49\x20\x44\x8B\x41\x10\x8B\x11", 3 )
	};

	const Symbol GMOD_GetNetSocket = Symbol::FromName( "?GMOD_GetNetSocket@@YAPEAUnetsocket_t@@H@Z" );

	const std::vector<Symbol> GModDataPack_AddOrUpdateFile = {
		Symbol::FromName( "?AddOrUpdateFile@GModDataPack@@QEAAXPEAULuaFile@@_N@Z" ),
		Symbol::FromSignature( "\x48\x89\x5C\x24\x10\x48\x89\x4C\x24\x08\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\x6C\x24\xD9" )
	};

	const std::vector<Symbol> Steam3Server = {
		Symbol::FromSignature( "\x48\x8D\x05\x29\xA3\x2A\x2A\xC3" )
	};

	const std::vector<Symbol> GlobalVars = {
		Symbol::FromSignature( "\x48\x89\x1D\x2A\x2A\x2A\x2A\x41\x8B\xD7\x48\x8D\x8D\xB8\x01\x00\x00\xE8", 3 )
	};

	const std::vector<Symbol> AdvancedLuaErrorReporter = {
		Symbol::FromName( "?AdvancedLuaErrorReporter@@YAHPEAUlua_State@@@Z" ),
		Symbol::FromSignature( "\x48\x89\x5C\x24\x08\x55\x56\x57\x48\x81\xEC\x90\x00\x00\x00\x48\x8B\xF9" )
	};

	const std::vector<Symbol> NET_ProcessSocket = {
		Symbol::FromSignature( "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x54\x41\x56\x41\x57\x48\x83\xEC\x30\x8B\xF1" )
	};

	const std::vector<Symbol> NET_CreateNetChannel = {
		Symbol::FromSignature( "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x83\xEC\x40\x80\x7C\x24\x70\x00" )
	};

    const std::vector<Symbol> HandleChange = {
        Symbol::FromSignature( "\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x48\x89\x7c\x24\x18\x55\x48\x8d\x6c\x24\xa9\x48\x81\xec\xa0\x00\x00\x00\x48\x8b\xf9\x48\x8b\xd1" )
    };

}
