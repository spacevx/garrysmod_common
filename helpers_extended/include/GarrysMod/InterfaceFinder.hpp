#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace SourceSDK
{

class FactoryLoader;

class InterfaceFinder
{
public:
	typedef void *( *InstantiateInterfaceFn )( );

	struct Entry
	{
		const char *name;
		InstantiateInterfaceFn create;
	};

	explicit InterfaceFinder( const std::string &module_name );

	explicit InterfaceFinder( const FactoryLoader &loader );

	InterfaceFinder( void *module_handle, const void *create_interface );

	bool IsValid( ) const
	{
		return list_address != nullptr;
	}

	const void *GetListAddress( ) const
	{
		return list_address;
	}

	bool Refresh( );

	const std::vector<Entry> &GetEntries( ) const
	{
		return entries;
	}

	template<typename Function>
	void ForEach( Function &&function ) const
	{
		for( const Entry &entry : entries )
			function( entry.name, entry.create );
	}

	const Entry *FindExact( const char *name ) const;

	const Entry *FindByPrefix( const char *prefix ) const;

	void *CreateExact( const char *name ) const;

	void *CreateByPrefix( const char *prefix ) const;

	template<class Interface>
	Interface *GetInterface( const char *prefix ) const
	{
		return static_cast<Interface *>( CreateByPrefix( prefix ) );
	}

private:
	void Initialize( void *module_handle, const void *create_interface );

	const void *module_base = nullptr;
	size_t module_size = 0;
	const void *list_address = nullptr;
	std::vector<Entry> entries;
};

}
