#ifndef INC_TT_MATH_HASH_CRC32_H
#define INC_TT_MATH_HASH_CRC32_H

#include <stddef.h>

#include <tt/platform/tt_types.h>


namespace tt {
namespace math {
namespace hash {

class CRC32
{
public:
	CRC32();
	CRC32(const CRC32& p_rhs);
	~CRC32();
	
	CRC32& operator=(const CRC32& p_rhs);
	
	u32 getCRC() const;
	u32 getHash() const;
	void update(const void* p_input, size_t p_length);
	u32 calcCRC(const void* p_data, size_t p_length) const;
	
private:
	enum
	{
		StandardPoly = 0xedb88320,
		StandardInit = 0xffffffff
	};
	
	struct CRCTable
	{
		u32 table[256];
	};
	
	// basic reference counting
	static void addReference();
	static void removeReference();
	static void calcCRC(u32& p_crc, const void* p_data, size_t p_length);
	
	static CRCTable* m_table;
	static int       m_references;
	
	u32 m_context;
};

// Namespace end
}
}
}

#endif // !defined(INC_TT_MATH_HASH_CRC32_H)
