#pragma once

#include "Symbol.hpp"
#include "Platform.hpp"

#include <GarrysMod/FactoryLoader.hpp>

#include <scanning/symbolfinder.hpp>

#include <cstdint>
#include <cstring>

namespace Symbols
{
	namespace Resolution
	{
		inline void *Find(
			SymbolFinder &finder,
			const SourceSDK::FactoryLoader &loader,
			const Symbol &symbol,
			const void *start = nullptr
		)
		{
			if( symbol.type == Symbol::Type::None )
				return nullptr;

			auto pointer = reinterpret_cast<uint8_t *>( finder.Resolve(
				loader.GetModule( ), symbol.name.c_str( ), symbol.length, start
			) );
			return pointer != nullptr ? pointer + symbol.offset : nullptr;
		}

		inline void *FindData( SymbolFinder &finder, const SourceSDK::FactoryLoader &loader, const Symbol &symbol )
		{
			void *pointer = Find( finder, loader, symbol );
			if( pointer == nullptr || symbol.type == Symbol::Type::Name )
				return pointer;

			const uint8_t *operand = static_cast<const uint8_t *>( pointer );

#if defined ARCHITECTURE_X86_64

			int32_t displacement = 0;
			std::memcpy( &displacement, operand, sizeof( displacement ) );
			return const_cast<uint8_t *>( operand + sizeof( displacement ) + displacement );

#else

			uint32_t address = 0;
			std::memcpy( &address, operand, sizeof( address ) );
			return reinterpret_cast<void *>( static_cast<uintptr_t>( address ) );

#endif

		}
	}
}
