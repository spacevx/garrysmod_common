#include "InterfaceFinder.hpp"
#include "Platform.hpp"

#include <GarrysMod/FactoryLoader.hpp>

#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <utility>

#if defined SYSTEM_WINDOWS

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>

#include <detouring/hde.h>

#endif

namespace SourceSDK
{

#if defined SYSTEM_WINDOWS

namespace
{

struct InterfaceRegNode
{
	InterfaceFinder::InstantiateInterfaceFn create;
	const char *name;
	const InterfaceRegNode *next;
};

static_assert( sizeof( InterfaceRegNode ) == 3 * sizeof( void * ), "InterfaceReg layout mismatch" );
static_assert( offsetof( InterfaceRegNode, create ) == 0, "InterfaceReg layout mismatch" );
static_assert( offsetof( InterfaceRegNode, name ) == sizeof( void * ), "InterfaceReg layout mismatch" );
static_assert( offsetof( InterfaceRegNode, next ) == 2 * sizeof( void * ), "InterfaceReg layout mismatch" );

const size_t max_entries = 4096;
const size_t max_name_length = 256;
const size_t max_instructions = 64;
const size_t max_calls_followed = 4;
const int max_follow_depth = 4;
const uintptr_t page_size = 4096;

#if defined ARCHITECTURE_X86_64

typedef hde64s Instruction;

inline unsigned int Disassemble( const void *code, Instruction &instruction )
{
	return hde64_disasm( code, &instruction );
}

#elif defined ARCHITECTURE_X86

typedef hde32s Instruction;

inline unsigned int Disassemble( const void *code, Instruction &instruction )
{
	return hde32_disasm( code, &instruction );
}

#endif

class ModuleBounds
{
public:
	ModuleBounds( const void *_base, size_t _size ) :
		base( reinterpret_cast<uintptr_t>( _base ) ),
		size( _size )
	{ }

	bool Contains( const void *address, size_t length = 1 ) const
	{
		if( base == 0 || address == nullptr || length == 0 )
			return false;

		const uintptr_t start = reinterpret_cast<uintptr_t>( address );
		const uintptr_t last = start + length - 1;
		if( last < start )
			return false;

		return start >= base && last < base + size;
	}

private:
	uintptr_t base;
	size_t size;
};

bool IsValidName( const ModuleBounds &bounds, const char *name )
{
	if( !bounds.Contains( name ) )
		return false;

	for( size_t k = 0; k < max_name_length; ++k )
	{
		const char *current = name + k;
		const bool page_start = ( reinterpret_cast<uintptr_t>( current ) & ( page_size - 1 ) ) == 0;
		if( k != 0 && page_start && !bounds.Contains( current ) )
			return false;

		const unsigned char c = static_cast<unsigned char>( *current );
		if( c == '\0' )
			return k != 0;

		if( c < 0x20 || c > 0x7E )
			return false;
	}

	return false;
}

bool WalkList(
	const ModuleBounds &bounds, const void *list_address, std::vector<InterfaceFinder::Entry> *output
)
{
	if( ( reinterpret_cast<uintptr_t>( list_address ) & ( sizeof( void * ) - 1 ) ) != 0 ||
		!bounds.Contains( list_address, sizeof( void * ) ) )
		return false;

	const InterfaceRegNode *node = *static_cast<const InterfaceRegNode *const *>( list_address );
	if( node == nullptr )
		return false;

	std::vector<InterfaceFinder::Entry> entries;
	std::unordered_set<const InterfaceRegNode *> visited;
	for( ; node != nullptr; node = node->next )
	{
		if( entries.size( ) >= max_entries || !visited.insert( node ).second )
			return false;

		if( ( reinterpret_cast<uintptr_t>( node ) & ( sizeof( void * ) - 1 ) ) != 0 ||
			!bounds.Contains( node, sizeof( InterfaceRegNode ) ) )
			return false;

		if( !bounds.Contains( reinterpret_cast<const void *>( node->create ) ) ||
			!IsValidName( bounds, node->name ) )
			return false;

		entries.push_back( { node->name, node->create } );
	}

	if( output != nullptr )
		*output = std::move( entries );

	return true;
}

const void *ValidateCandidate( const ModuleBounds &bounds, const void *candidate )
{
	if( WalkList( bounds, candidate, nullptr ) )
		return candidate;

	return nullptr;
}

const void *GetMemoryOperand( const Instruction &instruction, const uint8_t *next )
{

#if defined ARCHITECTURE_X86

	static_cast<void>( next );

#endif

	if( ( instruction.flags & ( F_PREFIX_SEG | F_PREFIX_66 | F_PREFIX_67 ) ) != 0 ||
		( instruction.flags & F_MODRM ) == 0 ||
		instruction.modrm_mod != 0 ||
		instruction.modrm_rm != 5 ||
		( instruction.flags & F_DISP32 ) == 0 )
		return nullptr;

#if defined ARCHITECTURE_X86_64

	return next + static_cast<int32_t>( instruction.disp.disp32 );

#elif defined ARCHITECTURE_X86

	return reinterpret_cast<const void *>( static_cast<uintptr_t>( instruction.disp.disp32 ) );

#endif

}

const void *GetLoadAddress( const Instruction &instruction, const uint8_t *next )
{
	if( instruction.opcode == 0x8B )
	{

#if defined ARCHITECTURE_X86_64

		if( instruction.rex_w == 0 )
			return nullptr;

#endif

		return GetMemoryOperand( instruction, next );
	}

#if defined ARCHITECTURE_X86

	if( instruction.opcode == 0xA1 &&
		( instruction.flags & ( F_PREFIX_SEG | F_PREFIX_66 | F_PREFIX_67 ) ) == 0 &&
		( instruction.flags & F_IMM32 ) != 0 )
		return reinterpret_cast<const void *>( static_cast<uintptr_t>( instruction.imm.imm32 ) );

#endif

	return nullptr;
}

enum class Branch
{
	None,
	Call,
	Jump
};

Branch GetBranch(
	const ModuleBounds &bounds, const Instruction &instruction, const uint8_t *next, const uint8_t *&target
)
{
	target = nullptr;
	if( instruction.opcode == 0xE8 || instruction.opcode == 0xE9 || instruction.opcode == 0xEB )
	{
		if( ( instruction.flags & F_RELATIVE ) == 0 )
			return Branch::None;

		if( instruction.opcode == 0xEB )
			target = next + static_cast<int8_t>( instruction.imm.imm8 );
		else
			target = next + static_cast<int32_t>( instruction.imm.imm32 );

		return instruction.opcode == 0xE8 ? Branch::Call : Branch::Jump;
	}

	if( instruction.opcode == 0xFF && ( instruction.modrm_reg == 2 || instruction.modrm_reg == 4 ) )
	{
		const void *slot = GetMemoryOperand( instruction, next );
		if( slot == nullptr || ( reinterpret_cast<uintptr_t>( slot ) & ( sizeof( void * ) - 1 ) ) != 0 ||
			!bounds.Contains( slot, sizeof( void * ) ) )
			return Branch::None;

		target = *static_cast<const uint8_t *const *>( slot );
		return instruction.modrm_reg == 2 ? Branch::Call : Branch::Jump;
	}

	return Branch::None;
}

const void *FindListAddressByDisassembly( const ModuleBounds &bounds, const uint8_t *code, int depth )
{
	const uint8_t *calls[max_calls_followed] = { };
	size_t call_count = 0;
	for( size_t k = 0; k < max_instructions; ++k )
	{
		if( !bounds.Contains( code, 15 ) )
			break;

		Instruction instruction;
		const unsigned int length = Disassemble( code, instruction );
		if( length == 0 || ( instruction.flags & F_ERROR ) != 0 )
			break;

		const uint8_t *next = code + length;

		const void *candidate = GetLoadAddress( instruction, next );
		if( candidate != nullptr )
		{
			const void *list_address = ValidateCandidate( bounds, candidate );
			if( list_address != nullptr )
				return list_address;
		}

		const uint8_t *target = nullptr;
		const Branch branch = GetBranch( bounds, instruction, next, target );
		if( branch == Branch::Jump )
		{
			if( target == nullptr || depth >= max_follow_depth )
				break;

			++depth;
			code = target;
			continue;
		}

		if( branch == Branch::Call && target != nullptr && call_count < max_calls_followed )
			calls[call_count++] = target;

		if( instruction.opcode == 0xC3 || instruction.opcode == 0xC2 )
			break;

		code = next;
	}

	if( depth < max_follow_depth )
		for( size_t k = 0; k < call_count; ++k )
		{
			const void *list_address = FindListAddressByDisassembly( bounds, calls[k], depth + 1 );
			if( list_address != nullptr )
				return list_address;
		}

	return nullptr;
}

bool ParseVersion( const char *suffix, unsigned long &version )
{
	version = 0;
	size_t digits = 0;
	for( ; suffix[digits] != '\0'; ++digits )
	{
		const char c = suffix[digits];
		if( c < '0' || c > '9' || digits >= 9 )
			return false;

		version = version * 10 + static_cast<unsigned long>( c - '0' );
	}

	return true;
}

}

#endif

InterfaceFinder::InterfaceFinder( const std::string &module_name )
{
	const FactoryLoader loader( module_name );
	if( loader.IsValid( ) )
		Initialize( loader.GetModule( ), reinterpret_cast<const void *>( loader.GetFactory( ) ) );
}

InterfaceFinder::InterfaceFinder( const FactoryLoader &loader )
{
	if( loader.IsValid( ) )
		Initialize( loader.GetModule( ), reinterpret_cast<const void *>( loader.GetFactory( ) ) );
}

InterfaceFinder::InterfaceFinder( void *module_handle, const void *create_interface )
{
	Initialize( module_handle, create_interface );
}

void InterfaceFinder::Initialize( void *module_handle, const void *create_interface )
{
	if( module_handle == nullptr || create_interface == nullptr )
		return;

#if defined SYSTEM_WINDOWS

	const uint8_t *base = static_cast<const uint8_t *>( module_handle );
	const IMAGE_DOS_HEADER *dos_header = reinterpret_cast<const IMAGE_DOS_HEADER *>( base );
	if( dos_header->e_magic != IMAGE_DOS_SIGNATURE || dos_header->e_lfanew <= 0 )
		return;

	const IMAGE_NT_HEADERS *nt_headers =
		reinterpret_cast<const IMAGE_NT_HEADERS *>( base + dos_header->e_lfanew );
	if( nt_headers->Signature != IMAGE_NT_SIGNATURE ||
		nt_headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC )
		return;

	module_base = base;
	module_size = nt_headers->OptionalHeader.SizeOfImage;

	const ModuleBounds bounds( module_base, module_size );
	if( !bounds.Contains( create_interface ) )
	{
		module_base = nullptr;
		module_size = 0;
		return;
	}

	list_address = FindListAddressByDisassembly( bounds, static_cast<const uint8_t *>( create_interface ), 0 );
	if( !Refresh( ) )
		list_address = nullptr;

#else

	// TODO: Do it for linux & macOS
	static_cast<void>( module_handle );
	static_cast<void>( create_interface );

#endif

}

bool InterfaceFinder::Refresh( )
{
	entries.clear( );
	if( list_address == nullptr )
		return false;

#if defined SYSTEM_WINDOWS

	return WalkList( ModuleBounds( module_base, module_size ), list_address, &entries );

#else

	return false;

#endif

}

const InterfaceFinder::Entry *InterfaceFinder::FindExact( const char *name ) const
{
	if( name == nullptr )
		return nullptr;

	for( const Entry &entry : entries )
		if( std::strcmp( entry.name, name ) == 0 )
			return &entry;

	return nullptr;
}

const InterfaceFinder::Entry *InterfaceFinder::FindByPrefix( const char *prefix ) const
{
	if( prefix == nullptr || prefix[0] == '\0' )
		return nullptr;

	const size_t prefix_length = std::strlen( prefix );
	const Entry *best_entry = nullptr;
	unsigned long best_version = 0;
	for( const Entry &entry : entries )
	{
		if( std::strncmp( entry.name, prefix, prefix_length ) != 0 )
			continue;

		unsigned long version = 0;
		if( !ParseVersion( entry.name + prefix_length, version ) )
			continue;

		if( best_entry == nullptr || version > best_version )
		{
			best_entry = &entry;
			best_version = version;
		}
	}

	return best_entry;
}

void *InterfaceFinder::CreateExact( const char *name ) const
{
	const Entry *entry = FindExact( name );
	return entry != nullptr ? entry->create( ) : nullptr;
}

void *InterfaceFinder::CreateByPrefix( const char *prefix ) const
{
	const Entry *entry = FindByPrefix( prefix );
	return entry != nullptr ? entry->create( ) : nullptr;
}

}
