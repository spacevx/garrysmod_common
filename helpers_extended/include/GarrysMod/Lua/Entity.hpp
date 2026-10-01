#pragma once

#include <cstdint>

class CBaseEntity;
class IClientEntity;

namespace GarrysMod
{

namespace Lua
{

class ILuaBase;

namespace Entity
{

constexpr uint32_t InvalidHandle = 0xFFFFFFFF;

#if IS_SERVERSIDE

typedef CBaseEntity RealmEntity;

#else

typedef IClientEntity RealmEntity;

#endif

bool IsEntity( ILuaBase *LUA, int32_t index );
uint32_t GetHandle( ILuaBase *LUA, int32_t index );
int32_t GetEntryIndex( ILuaBase *LUA, int32_t index );

CBaseEntity *GetServer( ILuaBase *LUA, int32_t index );
IClientEntity *GetClient( ILuaBase *LUA, int32_t index );
CBaseEntity *GetServerFromHandle( uint32_t handle );
IClientEntity *GetClientFromHandle( uint32_t handle );

uint32_t GetHandleOfServer( CBaseEntity *entity );
uint32_t GetHandleOfClient( IClientEntity *entity );

void PushByIndex( ILuaBase *LUA, int32_t entry_index );
void PushServer( ILuaBase *LUA, CBaseEntity *entity );
void PushClient( ILuaBase *LUA, IClientEntity *entity );

inline RealmEntity *Get( ILuaBase *LUA, int32_t index )
{

#if IS_SERVERSIDE

	return GetServer( LUA, index );

#else

	return GetClient( LUA, index );

#endif

}

inline RealmEntity *GetFromHandle( uint32_t handle )
{

#if IS_SERVERSIDE

	return GetServerFromHandle( handle );

#else

	return GetClientFromHandle( handle );

#endif

}

inline uint32_t GetHandleOf( RealmEntity *entity )
{

#if IS_SERVERSIDE

	return GetHandleOfServer( entity );

#else

	return GetHandleOfClient( entity );

#endif

}

inline void Push( ILuaBase *LUA, RealmEntity *entity )
{

#if IS_SERVERSIDE

	PushServer( LUA, entity );

#else

	PushClient( LUA, entity );

#endif

}

inline bool IsValid( ILuaBase *LUA, int32_t index )
{
	return Get( LUA, index ) != nullptr;
}

}

}

}
