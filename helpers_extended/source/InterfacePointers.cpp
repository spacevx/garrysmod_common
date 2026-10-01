#include "InterfacePointers.hpp"
#include "InterfaceFinder.hpp"
#include "FunctionPointers.hpp"
#include "Symbols.hpp"

#include <GarrysMod/FactoryLoader.hpp>

#include <scanning/symbolfinder.hpp>

class IPlayerInfo;
class edict_t;

class IPlayerInfoManager
{
public:
	virtual IPlayerInfo *GetPlayerInfo( edict_t *pEdict ) = 0;
	virtual CGlobalVars *GetGlobalVars( ) = 0;
};

namespace InterfacePointers
{
	static SymbolFinder symbol_finder;

	static const char filesystem_name[] = "VFileSystem022";
	static const char filesystem_prefix[] = "VFileSystem";
	static const char vengineserver_name[] = "VEngineServer021";
	static const char vengineserver_prefix[] = "VEngineServer";
	static const char vengineclient_name[] = "VEngineClient015";
	static const char vengineclient_prefix[] = "VEngineClient";
	static const char cvar_name[] = "VEngineCvar004";
	static const char cvar_prefix[] = "VEngineCvar";
	static const char servergamedll_name[] = "ServerGameDLL009";
	static const char servergamedll_prefix[] = "ServerGameDLL";
	static const char networkstringtableserver_name[] = "VEngineServerStringTable001";
	static const char networkstringtableserver_prefix[] = "VEngineServerStringTable";
	static const char networkstringtableclient_name[] = "VEngineClientStringTable001";
	static const char networkstringtableclient_prefix[] = "VEngineClientStringTable";
	static const char playerinfomanager_name[] = "PlayerInfoManager002";
	static const char playerinfomanager_prefix[] = "PlayerInfoManager";

	template<class T>
	static inline T *ResolveInterface(
		const SourceSDK::FactoryLoader &loader, const char *name, const char *prefix
	)
	{
		T *iface_pointer = loader.GetInterface<T>( name );
		if( iface_pointer == nullptr )
		{
			const SourceSDK::InterfaceFinder finder( loader );
			iface_pointer = finder.GetInterface<T>( prefix );
		}

		return iface_pointer;
	}

	template<class T>
	static inline T *ResolveSymbol(
		SourceSDK::FactoryLoader &loader, const Symbol &symbol
	)
	{
		if( symbol.type == Symbol::Type::None )
			return nullptr;

#if defined SYSTEM_WINDOWS

		auto iface = reinterpret_cast<T **>( symbol_finder.Resolve(
			loader.GetModule( ), symbol.name.c_str( ), symbol.length
		) );
		return iface != nullptr ? *iface : nullptr;

#elif defined SYSTEM_POSIX

		return reinterpret_cast<T *>( symbol_finder.Resolve(
			loader.GetModule( ), symbol.name.c_str( ), symbol.length
		) );

#endif

	}

	template<class T>
	static inline T *ResolveSymbols(
		SourceSDK::FactoryLoader &loader, const std::vector<Symbol> &symbols
	)
	{
		T *iface_pointer = nullptr;
		for( const auto &symbol : symbols )
		{
			iface_pointer = ResolveSymbol<T>( loader, symbol );
			if( iface_pointer != nullptr )
				break;
		}

		return iface_pointer;
	}

	namespace Internal
	{
		namespace Client
		{
			IFileSystem *FileSystem( )
			{
				static IFileSystem *iface_pointer = nullptr;
				if( iface_pointer == nullptr )
				{
					SourceSDK::FactoryLoader client_loader( "client" );
					iface_pointer = ResolveSymbol<IFileSystem>(
						client_loader, Symbols::g_pFullFileSystem
					);
				}

				if( iface_pointer == nullptr )
				{
					SourceSDK::FactoryLoader filesystem_loader( "filesystem_stdio" );
					iface_pointer = ResolveInterface<IFileSystem>(
						filesystem_loader, filesystem_name, filesystem_prefix
					);
				}

				return iface_pointer;
			}

			INetworkStringTableContainer *NetworkStringTableContainer( )
			{
				static INetworkStringTableContainer *iface_pointer = nullptr;
				if( iface_pointer == nullptr )
				{
					SourceSDK::FactoryLoader engine_loader( "engine" );
					iface_pointer = ResolveInterface<INetworkStringTableContainer>(
						engine_loader, networkstringtableclient_name, networkstringtableclient_prefix
					);
				}

				return iface_pointer;
			}

			CGlobalVars *GlobalVars( )
			{
				static CGlobalVars *iface_pointer = nullptr;
				if( iface_pointer == nullptr )
				{
					SourceSDK::FactoryLoader client_loader( "client" );
					auto pointer = ResolveSymbols<CGlobalVars *>(
						client_loader, Symbols::GlobalVars
					);
					if( pointer != nullptr )
						iface_pointer = *pointer;
				}

				return iface_pointer;
			}
		}

		namespace Server
		{
			IFileSystem *FileSystem( )
			{
				static IFileSystem *iface_pointer = nullptr;
				if( iface_pointer != nullptr )
					return iface_pointer;

				auto factory = FunctionPointers::FileSystemFactory( );
				if( factory == nullptr )
				{
					SourceSDK::FactoryLoader dedicated_loader( "dedicated" );
					iface_pointer = ResolveSymbol<IFileSystem>(
						dedicated_loader, Symbols::g_pFullFileSystem
					);
					if( iface_pointer == nullptr )
					{
						SourceSDK::FactoryLoader server_loader( "server" );
						iface_pointer = ResolveSymbol<IFileSystem>(
							server_loader, Symbols::g_pFullFileSystem
						);
					}
				}
				else
				{
					iface_pointer =
						static_cast<IFileSystem *>( factory( filesystem_name, nullptr ) );
				}

				if( iface_pointer == nullptr )
				{
					SourceSDK::FactoryLoader filesystem_loader( "filesystem_stdio" );
					iface_pointer = ResolveInterface<IFileSystem>(
						filesystem_loader, filesystem_name, filesystem_prefix
					);
				}

				return iface_pointer;
			}

			INetworkStringTableContainer *NetworkStringTableContainer( )
			{
				static INetworkStringTableContainer *iface_pointer = nullptr;
				if( iface_pointer == nullptr )
				{
					SourceSDK::FactoryLoader engine_loader( "engine" );
					iface_pointer = ResolveInterface<INetworkStringTableContainer>(
						engine_loader, networkstringtableserver_name, networkstringtableserver_prefix
					);
				}

				return iface_pointer;
			}

			CGlobalVars *GlobalVars( )
			{
				static CGlobalVars *iface_pointer = nullptr;
				if( iface_pointer == nullptr )
				{
					SourceSDK::FactoryLoader server_loader( "server" );
					auto player_info_manager = ResolveInterface<IPlayerInfoManager>(
						server_loader, playerinfomanager_name, playerinfomanager_prefix
					);
					if( player_info_manager != nullptr )
						iface_pointer = player_info_manager->GetGlobalVars( );
				}

				return iface_pointer;
			}
		}
	}

	IVEngineServer *VEngineServer( )
	{
		static IVEngineServer *iface_pointer = nullptr;
		if( iface_pointer == nullptr )
		{
			SourceSDK::FactoryLoader engine_loader( "engine" );
			iface_pointer = ResolveInterface<IVEngineServer>(
				engine_loader, vengineserver_name, vengineserver_prefix
			);
		}

		return iface_pointer;
	}

	IVEngineClient *VEngineClient( )
	{
		static IVEngineClient *iface_pointer = nullptr;
		if( iface_pointer == nullptr )
		{
			SourceSDK::FactoryLoader engine_loader( "engine" );
			iface_pointer = ResolveInterface<IVEngineClient>(
				engine_loader, vengineclient_name, vengineclient_prefix
			);
		}

		return iface_pointer;
	}

	ICvar *Cvar( )
	{
		static ICvar *iface_pointer = nullptr;
		if( iface_pointer == nullptr )
		{
			SourceSDK::FactoryLoader vstdlib_loader( "vstdlib" );
			iface_pointer = ResolveInterface<ICvar>( vstdlib_loader, cvar_name, cvar_prefix );
		}

		return iface_pointer;
	}

	IServerGameDLL *ServerGameDLL( )
	{
		static IServerGameDLL *iface_pointer = nullptr;
		if( iface_pointer == nullptr )
		{
			SourceSDK::FactoryLoader server_loader( "server" );
			iface_pointer = ResolveInterface<IServerGameDLL>(
				server_loader, servergamedll_name, servergamedll_prefix
			);
		}

		return iface_pointer;
	}

	IServer *Server( )
	{
		static IServer *iface_pointer = nullptr;
		if( iface_pointer == nullptr )
		{
			SourceSDK::FactoryLoader engine_loader( "engine" );
			iface_pointer = ResolveSymbols<IServer>( engine_loader, Symbols::IServer );
		}

		return iface_pointer;
	}

	CSteamGameServerAPIContext *SteamGameServerAPIContext( )
	{
		static CSteamGameServerAPIContext *iface_pointer = nullptr;
		if( iface_pointer == nullptr )
		{
			auto Steam3Server = FunctionPointers::Steam3Server( );
			if( Steam3Server != nullptr )
				iface_pointer = reinterpret_cast<CSteamGameServerAPIContext *>( Steam3Server( ) );
		}

		return iface_pointer;
	}
}
