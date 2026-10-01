#include "Lua/Entity.hpp"
#include "InterfacePointers.hpp"
#include "InterfaceFinder.hpp"

#include <GarrysMod/FactoryLoader.hpp>
#include <GarrysMod/Lua/LuaBase.h>

#include <cstddef>

namespace GarrysMod
{

namespace Lua
{

namespace Entity
{

namespace
{

const uint32_t entry_mask = 0x3FFF;
const uint32_t max_edicts = 8192;

const char cliententitylist_prefix[] = "VClientEntityList";
const char cliententitylist_name[] = "VClientEntityList003";

class BaseHandle
{
public:
	explicit BaseHandle( uint32_t _index ) :
		index( _index )
	{ }

	BaseHandle( const BaseHandle &other ) :
		index( other.index )
	{ }

	BaseHandle &operator=( const BaseHandle &other ) = delete;

	uint32_t Get( ) const
	{
		return index;
	}

private:
	uint32_t index;
};

static_assert( sizeof( BaseHandle ) == 4, "CBaseHandle layout mismatch" );

class HandleEntity
{
public:
	virtual ~HandleEntity( ) = default;
	virtual void Slot1( ) = 0;
	virtual const BaseHandle &GetRefEHandle( ) const = 0;
};

class ServerUnknown : public HandleEntity
{
public:
	virtual void Slot3( ) = 0;
	virtual void Slot4( ) = 0;
	virtual CBaseEntity *GetBaseEntity( ) = 0;
};

struct Edict
{
	int32_t state_flags;
	int16_t network_serial_number;
	int16_t edict_index;
	void *networkable;
	ServerUnknown *unknown;
	float free_time;
};

static_assert( offsetof( Edict, networkable ) == 8, "edict_t layout mismatch" );
static_assert( offsetof( Edict, unknown ) == 8 + sizeof( void * ), "edict_t layout mismatch" );
static_assert( sizeof( Edict ) == ( sizeof( void * ) == 8 ? 32 : 20 ), "edict_t layout mismatch" );

class EngineServer
{
public:
	virtual void Slot0( ) = 0;
	virtual void Slot1( ) = 0;
	virtual void Slot2( ) = 0;
	virtual void Slot3( ) = 0;
	virtual void Slot4( ) = 0;
	virtual void Slot5( ) = 0;
	virtual void Slot6( ) = 0;
	virtual void Slot7( ) = 0;
	virtual void Slot8( ) = 0;
	virtual void Slot9( ) = 0;
	virtual void Slot10( ) = 0;
	virtual void Slot11( ) = 0;
	virtual void Slot12( ) = 0;
	virtual void Slot13( ) = 0;
	virtual void Slot14( ) = 0;
	virtual void Slot15( ) = 0;
	virtual void Slot16( ) = 0;
	virtual void Slot17( ) = 0;
	virtual int32_t IndexOfEdict( const Edict *edict ) = 0;
	virtual Edict *PEntityOfEntIndex( int32_t index ) = 0;
};

class ClientEntityList
{
public:
	virtual void Slot0( ) = 0;
	virtual void Slot1( ) = 0;
	virtual void Slot2( ) = 0;
	virtual void Slot3( ) = 0;
	virtual IClientEntity *GetClientEntityFromHandle( BaseHandle handle ) = 0;
};

ClientEntityList *GetClientEntityList( )
{
	static ClientEntityList *iface_pointer = nullptr;
	if( iface_pointer == nullptr )
	{
		SourceSDK::FactoryLoader client_loader( "client" );
		const SourceSDK::InterfaceFinder client_finder( client_loader );
		iface_pointer = client_finder.GetInterface<ClientEntityList>( cliententitylist_prefix );
		if( iface_pointer == nullptr )
			iface_pointer = client_loader.GetInterface<ClientEntityList>( cliententitylist_name );
	}

	return iface_pointer;
}

void PushHandle( ILuaBase *LUA, uint32_t handle )
{
	PushByIndex( LUA, static_cast<int32_t>( handle & entry_mask ) );
	if( GetHandle( LUA, -1 ) != handle )
	{
		LUA->Pop( 1 );
		LUA->PushNil( );
	}
}

}

bool IsEntity( ILuaBase *LUA, int32_t index )
{
	return LUA->IsType( index, Type::Entity );
}

uint32_t GetHandle( ILuaBase *LUA, int32_t index )
{
	if( !LUA->IsType( index, Type::Entity ) )
		return InvalidHandle;

	const BaseHandle *handle = LUA->GetUserType<BaseHandle>( index, Type::Entity );
	return handle != nullptr ? handle->Get( ) : InvalidHandle;
}

int32_t GetEntryIndex( ILuaBase *LUA, int32_t index )
{
	const uint32_t handle = GetHandle( LUA, index );
	return handle != InvalidHandle ? static_cast<int32_t>( handle & entry_mask ) : -1;
}

CBaseEntity *GetServer( ILuaBase *LUA, int32_t index )
{
	return GetServerFromHandle( GetHandle( LUA, index ) );
}

IClientEntity *GetClient( ILuaBase *LUA, int32_t index )
{
	return GetClientFromHandle( GetHandle( LUA, index ) );
}

CBaseEntity *GetServerFromHandle( uint32_t handle )
{
	const uint32_t entry = handle & entry_mask;
	if( handle == InvalidHandle || entry >= max_edicts )
		return nullptr;

	auto engine = reinterpret_cast<EngineServer *>( InterfacePointers::VEngineServer( ) );
	if( engine == nullptr )
		return nullptr;

	Edict *edict = engine->PEntityOfEntIndex( static_cast<int32_t>( entry ) );
	if( edict == nullptr )
		return nullptr;

	ServerUnknown *unknown = edict->unknown;
	if( unknown == nullptr || unknown->GetRefEHandle( ).Get( ) != handle )
		return nullptr;

	return unknown->GetBaseEntity( );
}

IClientEntity *GetClientFromHandle( uint32_t handle )
{
	if( handle == InvalidHandle )
		return nullptr;

	ClientEntityList *entity_list = GetClientEntityList( );
	if( entity_list == nullptr )
		return nullptr;

	return entity_list->GetClientEntityFromHandle( BaseHandle( handle ) );
}

uint32_t GetHandleOfServer( CBaseEntity *entity )
{
	if( entity == nullptr )
		return InvalidHandle;

	return reinterpret_cast<const HandleEntity *>( entity )->GetRefEHandle( ).Get( );
}

uint32_t GetHandleOfClient( IClientEntity *entity )
{
	if( entity == nullptr )
		return InvalidHandle;

	return reinterpret_cast<const HandleEntity *>( entity )->GetRefEHandle( ).Get( );
}

void PushByIndex( ILuaBase *LUA, int32_t entry_index )
{
	LUA->PushSpecial( SPECIAL_GLOB );
	LUA->GetField( -1, "Entity" );
	if( !LUA->IsType( -1, Type::Function ) )
	{
		LUA->Pop( 2 );
		LUA->PushNil( );
		return;
	}

	LUA->PushNumber( static_cast<double>( entry_index ) );
	LUA->Call( 1, 1 );
	LUA->Remove( -2 );
}

void PushServer( ILuaBase *LUA, CBaseEntity *entity )
{
	const uint32_t handle = GetHandleOfServer( entity );
	if( handle == InvalidHandle || ( handle & entry_mask ) >= max_edicts )
	{
		LUA->PushNil( );
		return;
	}

	PushHandle( LUA, handle );
}

void PushClient( ILuaBase *LUA, IClientEntity *entity )
{
	const uint32_t handle = GetHandleOfClient( entity );
	if( handle == InvalidHandle )
	{
		LUA->PushNil( );
		return;
	}

	PushHandle( LUA, handle );
}

}

}

}
