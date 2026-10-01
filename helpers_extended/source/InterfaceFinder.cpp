#include "InterfaceFinder.hpp"
#include "Platform.hpp"

#include <GarrysMod/FactoryLoader.hpp>

#include <detouring/hde.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <utility>

#if defined SYSTEM_WINDOWS

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>

#elif defined SYSTEM_POSIX

#include <dlfcn.h>

#include <scanning/symbolfinder.hpp>

#if defined SYSTEM_LINUX

#include <link.h>

#elif defined SYSTEM_MACOSX

#include <mach-o/dyld.h>
#include <mach-o/loader.h>

#endif

#endif

namespace SourceSDK
{

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

#if defined SYSTEM_POSIX

const char *const list_symbol_names[] = {
	"_ZN12InterfaceReg16s_pInterfaceRegsE",
	"s_pInterfaceRegs"
};

#endif

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

class MemoryRanges
{
public:
	void Add( uintptr_t start, uintptr_t end )
	{
		if( end > start )
			ranges.push_back( { start, end } );
	}

	bool Contains( const void *address, size_t length = 1 ) const
	{
		if( address == nullptr || length == 0 )
			return false;

		const uintptr_t start = reinterpret_cast<uintptr_t>( address );
		const uintptr_t last = start + length - 1;
		if( last < start )
			return false;

		for( const auto &range : ranges )
			if( start >= range.first && last < range.second )
				return true;

		return false;
	}

private:
	std::vector<std::pair<uintptr_t, uintptr_t>> ranges;
};

bool IsPointerAligned( const void *address )
{
	return ( reinterpret_cast<uintptr_t>( address ) & ( sizeof( void * ) - 1 ) ) == 0;
}

bool IsValidName( const MemoryRanges &memory, const char *name )
{
	if( !memory.Contains( name ) )
		return false;

	for( size_t k = 0; k < max_name_length; ++k )
	{
		const char *current = name + k;
		const bool page_start = ( reinterpret_cast<uintptr_t>( current ) & ( page_size - 1 ) ) == 0;
		if( k != 0 && page_start && !memory.Contains( current ) )
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
	const MemoryRanges &memory, const void *list_address, std::vector<InterfaceFinder::Entry> *output
)
{
	if( !IsPointerAligned( list_address ) || !memory.Contains( list_address, sizeof( void * ) ) )
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

		if( !IsPointerAligned( node ) || !memory.Contains( node, sizeof( InterfaceRegNode ) ) )
			return false;

		if( !memory.Contains( reinterpret_cast<const void *>( node->create ) ) ||
			!IsValidName( memory, node->name ) )
			return false;

		entries.push_back( { node->name, node->create } );
	}

	if( output != nullptr )
		*output = std::move( entries );

	return true;
}

const void *ValidateCandidate( const MemoryRanges &memory, const void *candidate )
{
	if( WalkList( memory, candidate, nullptr ) )
		return candidate;

	if( IsPointerAligned( candidate ) && memory.Contains( candidate, sizeof( void * ) ) )
	{
		const void *indirect = *static_cast<const void *const *>( candidate );
		if( WalkList( memory, indirect, nullptr ) )
			return indirect;
	}

	return nullptr;
}

struct PCBase
{
	bool valid = false;
	uint8_t reg = 0;
	const uint8_t *value = nullptr;
};

bool HasUnsupportedPrefix( const Instruction &instruction )
{
	return ( instruction.flags & ( F_PREFIX_SEG | F_PREFIX_66 | F_PREFIX_67 ) ) != 0;
}

const void *GetMemoryOperand( const Instruction &instruction, const uint8_t *next, const PCBase &pc_base )
{
	if( HasUnsupportedPrefix( instruction ) || ( instruction.flags & F_MODRM ) == 0 )
		return nullptr;

	if( instruction.modrm_mod == 0 && instruction.modrm_rm == 5 && ( instruction.flags & F_DISP32 ) != 0 )
	{

#if defined ARCHITECTURE_X86_64

		return next + static_cast<int32_t>( instruction.disp.disp32 );

#elif defined ARCHITECTURE_X86

		static_cast<void>( next );
		return reinterpret_cast<const void *>( static_cast<uintptr_t>( instruction.disp.disp32 ) );

#endif

	}

	if( pc_base.valid && instruction.modrm_rm == pc_base.reg && instruction.modrm_rm != 4 )
	{
		if( instruction.modrm_mod == 2 && ( instruction.flags & F_DISP32 ) != 0 )
			return pc_base.value + static_cast<int32_t>( instruction.disp.disp32 );

		if( instruction.modrm_mod == 1 && ( instruction.flags & F_DISP8 ) != 0 )
			return pc_base.value + static_cast<int8_t>( instruction.disp.disp8 );
	}

	return nullptr;
}

const void *GetLoadAddress( const Instruction &instruction, const uint8_t *next, const PCBase &pc_base )
{
	if( instruction.opcode == 0x8B )
	{

#if defined ARCHITECTURE_X86_64

		if( instruction.rex_w == 0 )
			return nullptr;

#endif

		return GetMemoryOperand( instruction, next, pc_base );
	}

#if defined ARCHITECTURE_X86

	if( instruction.opcode == 0xA1 && !HasUnsupportedPrefix( instruction ) &&
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
	const MemoryRanges &memory,
	const Instruction &instruction,
	const uint8_t *next,
	const PCBase &pc_base,
	const uint8_t *&target
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
		const void *slot = GetMemoryOperand( instruction, next, pc_base );
		if( slot == nullptr || !IsPointerAligned( slot ) || !memory.Contains( slot, sizeof( void * ) ) )
			return Branch::None;

		target = *static_cast<const uint8_t *const *>( slot );
		return instruction.modrm_reg == 2 ? Branch::Call : Branch::Jump;
	}

	return Branch::None;
}

const void *FindListAddressByDisassembly(
	const MemoryRanges &module, const MemoryRanges &memory, const uint8_t *code, int depth
)
{
	const uint8_t *calls[max_calls_followed] = { };
	size_t call_count = 0;
	PCBase pc_base;
	const uint8_t *pending_pc_base = nullptr;
	for( size_t k = 0; k < max_instructions; ++k )
	{
		if( !module.Contains( code, 15 ) )
			break;

		Instruction instruction;
		const unsigned int length = Disassemble( code, instruction );
		if( length == 0 || ( instruction.flags & F_ERROR ) != 0 )
			break;

		const uint8_t *next = code + length;

		if( pending_pc_base != nullptr )
		{
			if( instruction.opcode >= 0x58 && instruction.opcode <= 0x5F && !HasUnsupportedPrefix( instruction ) )
			{
				pc_base.valid = true;
				pc_base.reg = static_cast<uint8_t>( instruction.opcode - 0x58 );
				pc_base.value = pending_pc_base;
			}

			pending_pc_base = nullptr;
		}

		const void *candidate = GetLoadAddress( instruction, next, pc_base );
		if( candidate != nullptr && module.Contains( candidate, sizeof( void * ) ) )
		{
			const void *list_address = ValidateCandidate( memory, candidate );
			if( list_address != nullptr )
				return list_address;
		}

		const uint8_t *target = nullptr;
		const Branch branch = GetBranch( module, instruction, next, pc_base, target );
		if( branch == Branch::Jump )
		{
			if( target == nullptr || depth >= max_follow_depth )
				break;

			++depth;
			code = target;
			continue;
		}

		if( branch == Branch::Call && target == next )
			pending_pc_base = next;
		else if( branch == Branch::Call && target != nullptr && call_count < max_calls_followed )
			calls[call_count++] = target;

		if( instruction.opcode == 0xC3 || instruction.opcode == 0xC2 )
			break;

		code = next;
	}

	if( depth < max_follow_depth )
		for( size_t k = 0; k < call_count; ++k )
		{
			const void *list_address = FindListAddressByDisassembly( module, memory, calls[k], depth + 1 );
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

#if defined SYSTEM_WINDOWS

bool GetModuleBounds( void *module_handle, const void *, const void *&base, size_t &size )
{
	const uint8_t *image = static_cast<const uint8_t *>( module_handle );
	const IMAGE_DOS_HEADER *dos_header = reinterpret_cast<const IMAGE_DOS_HEADER *>( image );
	if( dos_header->e_magic != IMAGE_DOS_SIGNATURE || dos_header->e_lfanew <= 0 )
		return false;

	const IMAGE_NT_HEADERS *nt_headers =
		reinterpret_cast<const IMAGE_NT_HEADERS *>( image + dos_header->e_lfanew );
	if( nt_headers->Signature != IMAGE_NT_SIGNATURE ||
		nt_headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC )
		return false;

	base = image;
	size = nt_headers->OptionalHeader.SizeOfImage;
	return true;
}

MemoryRanges GetLoadedMemory( const MemoryRanges &module )
{
	return module;
}

#elif defined SYSTEM_LINUX

struct PhdrSearch
{
	uintptr_t load_address;
	uintptr_t start;
	uintptr_t end;
	MemoryRanges *memory;
};

int PhdrCallback( struct dl_phdr_info *info, size_t, void *data )
{
	PhdrSearch *search = static_cast<PhdrSearch *>( data );
	const bool wanted = search->memory == nullptr && info->dlpi_addr == search->load_address;
	for( int k = 0; k < info->dlpi_phnum; ++k )
	{
		const ElfW( Phdr ) &phdr = info->dlpi_phdr[k];
		if( phdr.p_type != PT_LOAD )
			continue;

		const uintptr_t start = info->dlpi_addr + phdr.p_vaddr;
		const uintptr_t end = start + phdr.p_memsz;
		if( search->memory != nullptr )
		{
			search->memory->Add( start, end );
			continue;
		}

		if( !wanted )
			continue;

		if( search->start == 0 || start < search->start )
			search->start = start;

		if( end > search->end )
			search->end = end;
	}

	return wanted ? 1 : 0;
}

bool GetModuleBounds( void *module_handle, const void *, const void *&base, size_t &size )
{
	struct link_map *map = nullptr;
	if( dlinfo( module_handle, RTLD_DI_LINKMAP, &map ) != 0 || map == nullptr )
		return false;

	PhdrSearch search = { static_cast<uintptr_t>( map->l_addr ), 0, 0, nullptr };
	if( dl_iterate_phdr( PhdrCallback, &search ) == 0 || search.end <= search.start )
		return false;

	base = reinterpret_cast<const void *>( search.start );
	size = search.end - search.start;
	return true;
}

MemoryRanges GetLoadedMemory( const MemoryRanges & )
{
	MemoryRanges memory;
	PhdrSearch search = { 0, 0, 0, &memory };
	dl_iterate_phdr( PhdrCallback, &search );
	return memory;
}

#elif defined SYSTEM_MACOSX

#if defined ARCHITECTURE_X86_64

typedef struct mach_header_64 MachHeader;
typedef struct segment_command_64 SegmentCommand;
const uint32_t segment_command_id = LC_SEGMENT_64;

#else

typedef struct mach_header MachHeader;
typedef struct segment_command SegmentCommand;
const uint32_t segment_command_id = LC_SEGMENT;

#endif

bool GetImageBounds( const MachHeader *header, intptr_t slide, uintptr_t &start, uintptr_t &end )
{
	const uint8_t *command = reinterpret_cast<const uint8_t *>( header + 1 );
	start = 0;
	end = 0;
	for( uint32_t k = 0; k < header->ncmds; ++k )
	{
		const struct load_command *load = reinterpret_cast<const struct load_command *>( command );
		if( load->cmd == segment_command_id )
		{
			const SegmentCommand *segment = reinterpret_cast<const SegmentCommand *>( load );
			if( std::strcmp( segment->segname, "__PAGEZERO" ) != 0 && segment->vmsize != 0 )
			{
				const uintptr_t segment_start = segment->vmaddr + slide;
				const uintptr_t segment_end = segment_start + segment->vmsize;
				if( start == 0 || segment_start < start )
					start = segment_start;

				if( segment_end > end )
					end = segment_end;
			}
		}

		command += load->cmdsize;
	}

	return end > start;
}

bool GetModuleBounds( void *, const void *create_interface, const void *&base, size_t &size )
{
	Dl_info info;
	if( dladdr( create_interface, &info ) == 0 || info.dli_fbase == nullptr )
		return false;

	const uint32_t count = _dyld_image_count( );
	for( uint32_t k = 0; k < count; ++k )
	{
		const MachHeader *header = reinterpret_cast<const MachHeader *>( _dyld_get_image_header( k ) );
		if( header != info.dli_fbase )
			continue;

		uintptr_t start = 0, end = 0;
		if( !GetImageBounds( header, _dyld_get_image_vmaddr_slide( k ), start, end ) )
			return false;

		base = reinterpret_cast<const void *>( start );
		size = end - start;
		return true;
	}

	return false;
}

MemoryRanges GetLoadedMemory( const MemoryRanges & )
{
	MemoryRanges memory;
	const uint32_t count = _dyld_image_count( );
	for( uint32_t k = 0; k < count; ++k )
	{
		const MachHeader *header = reinterpret_cast<const MachHeader *>( _dyld_get_image_header( k ) );
		uintptr_t start = 0, end = 0;
		if( header != nullptr && GetImageBounds( header, _dyld_get_image_vmaddr_slide( k ), start, end ) )
			memory.Add( start, end );
	}

	return memory;
}

#endif

#if defined SYSTEM_POSIX

const void *FindListAddressBySymbol( const MemoryRanges &memory, void *module_handle )
{
	SymbolFinder symbol_finder;
	for( const char *name : list_symbol_names )
	{
		const void *symbol = dlsym( module_handle, name );
		if( symbol == nullptr )
			symbol = symbol_finder.FindSymbol( module_handle, name );

		if( symbol != nullptr && WalkList( memory, symbol, nullptr ) )
			return symbol;
	}

	return nullptr;
}

#endif

MemoryRanges GetModuleMemory( const void *base, size_t size )
{
	MemoryRanges module;
	module.Add( reinterpret_cast<uintptr_t>( base ), reinterpret_cast<uintptr_t>( base ) + size );
	return module;
}

}

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

	if( !GetModuleBounds( module_handle, create_interface, module_base, module_size ) )
		return;

	const MemoryRanges module = GetModuleMemory( module_base, module_size );
	if( !module.Contains( create_interface ) )
	{
		module_base = nullptr;
		module_size = 0;
		return;
	}

	const MemoryRanges memory = GetLoadedMemory( module );
	list_address =
		FindListAddressByDisassembly( module, memory, static_cast<const uint8_t *>( create_interface ), 0 );

#if defined SYSTEM_POSIX

	if( list_address == nullptr )
		list_address = FindListAddressBySymbol( memory, module_handle );

#endif

	if( !Refresh( ) )
		list_address = nullptr;
}

bool InterfaceFinder::Refresh( )
{
	entries.clear( );
	if( list_address == nullptr )
		return false;

	return WalkList( GetLoadedMemory( GetModuleMemory( module_base, module_size ) ), list_address, &entries );
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
