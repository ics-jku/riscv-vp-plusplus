#ifndef CHERI_CAPABILITY_H
#define CHERI_CAPABILITY_H

#include <cstdint>

#include "core/rv64_cheriv9/cheri_prelude.h"  // TODO This must be changed when RV32 is implemented

struct Capability {
	union {
		struct {
			uint8_t uperms : cCapUPermsWidth;      // User-defined permissions
			bool permit_set_CID : 1;               // Permission to set compartment ID
			bool access_system_regs : 1;           // Permission to access system registers
			bool permit_unseal : 1;                // Permission to unseal capabilities
			bool permit_cinvoke : 1;               // Permission for capability invocation
			bool permit_seal : 1;                  // Permission to seal capabilities
			bool permit_store_local_cap : 1;       // Permission to store local capabilities
			bool permit_store_cap : 1;             // Permission to store capabilities
			bool permit_load_cap : 1;              // Permission to load capabilities
			bool permit_store : 1;                 // Permission to store
			bool permit_load : 1;                  // Permission to load
			bool permit_execute : 1;               // Permission to execute
			bool global : 1;                       // Global capability flag
			uint8_t reserved : cCapReservedWidth;  // Reserved bits
			bool flag_cap_mode : 1;                // Flag for capability mode
			bool internal_E : 1;                   // Internal E bit
			uint8_t E : cCapEWidth;                // Exponent for encoding bounds
			uint16_t B : cCapMantissaWidth;        // Base of the capability
			uint16_t T : cCapMantissaWidth;        // Top (limit) of the capability
			uint32_t otype : cCapOTypeWidth;       // Object type

			int64_t address;
			bool tag : 1;  // Using bool instead of uint8_t
		} fields;

		uint8_t raw[17];
	};

	// Default constructor
	Capability() noexcept = default;

	// constexpr constructor with all fields
	constexpr Capability(bool tag, uint8_t uperms, bool permit_set_CID, bool access_system_regs, bool permit_unseal,
	                     bool permit_cinvoke, bool permit_seal, bool permit_store_local_cap, bool permit_store_cap,
	                     bool permit_load_cap, bool permit_store, bool permit_load, bool permit_execute, bool global,
	                     uint8_t reserved, bool flag_cap_mode, bool internal_E, uint8_t E, uint16_t B, uint16_t T,
	                     uint32_t otype, int64_t addr)
	    : fields{uperms,
	             permit_set_CID,
	             access_system_regs,
	             permit_unseal,
	             permit_cinvoke,
	             permit_seal,
	             permit_store_local_cap,
	             permit_store_cap,
	             permit_load_cap,
	             permit_store,
	             permit_load,
	             permit_execute,
	             global,
	             reserved,
	             flag_cap_mode,
	             internal_E,
	             E,
	             B,
	             T,
	             otype,
	             addr,
	             tag} {}

	Capability(int64_t addr, uint64_t meta, bool t = false) : fields{} {
		fields.tag = t;
		fields.address = addr;
		unpackMetadata(meta);
	}

	Capability(__uint128_t data, bool tag)
	    : fields(EncCapability::toCapability(tag, uint128ToEncCapability(data ^ cNullCap128)).fields) {}

	// Helper method to unpack metadata from a single uint64_t value
	void unpackMetadata(uint64_t meta) {
		fields.uperms = meta & ((1ULL << cCapUPermsWidth) - 1);
		meta >>= cCapUPermsWidth;

		fields.permit_set_CID = meta & 1;
		meta >>= 1;
		fields.access_system_regs = meta & 1;
		meta >>= 1;
		fields.permit_unseal = meta & 1;
		meta >>= 1;
		fields.permit_cinvoke = meta & 1;
		meta >>= 1;
		fields.permit_seal = meta & 1;
		meta >>= 1;
		fields.permit_store_local_cap = meta & 1;
		meta >>= 1;
		fields.permit_store_cap = meta & 1;
		meta >>= 1;
		fields.permit_load_cap = meta & 1;
		meta >>= 1;
		fields.permit_store = meta & 1;
		meta >>= 1;
		fields.permit_load = meta & 1;
		meta >>= 1;
		fields.permit_execute = meta & 1;
		meta >>= 1;
		fields.global = meta & 1;
		meta >>= 1;
		fields.reserved = meta & ((1ULL << cCapReservedWidth) - 1);
		meta >>= cCapReservedWidth;

		fields.flag_cap_mode = meta & 1;
		meta >>= 1;
		fields.internal_E = meta & 1;
		meta >>= 1;
		fields.E = meta & ((1ULL << cCapEWidth) - 1);
		meta >>= cCapEWidth;

		fields.B = meta & ((1ULL << cCapMantissaWidth) - 1);
		meta >>= cCapMantissaWidth;

		fields.T = meta & ((1ULL << cCapMantissaWidth) - 1);
		meta >>= cCapMantissaWidth;

		fields.otype = meta & ((1ULL << cCapOTypeWidth) - 1);
	}

	operator int64_t() const {
		return fields.address;
	}

	// Set the address field of the Capability
	// This must clear the tag bit and metadata of the capability
	Capability& operator=(int64_t val) {
		fields.tag = false;
		clearMetadata();
		fields.address = val;
		return *this;
	}

	// Overload == operator to compare two Capabilities
	inline bool operator==(const Capability& other) const {
		return fields.address == other.fields.address && fields.otype == other.fields.otype &&
		       fields.T == other.fields.T && fields.B == other.fields.B && fields.E == other.fields.E &&
		       fields.internal_E == other.fields.internal_E && fields.flag_cap_mode == other.fields.flag_cap_mode &&
		       fields.reserved == other.fields.reserved && fields.global == other.fields.global &&
		       fields.permit_execute == other.fields.permit_execute && fields.permit_load == other.fields.permit_load &&
		       fields.permit_store == other.fields.permit_store &&
		       fields.permit_load_cap == other.fields.permit_load_cap &&
		       fields.permit_store_cap == other.fields.permit_store_cap &&
		       fields.permit_store_local_cap == other.fields.permit_store_local_cap &&
		       fields.permit_seal == other.fields.permit_seal && fields.permit_cinvoke == other.fields.permit_cinvoke &&
		       fields.permit_unseal == other.fields.permit_unseal &&
		       fields.access_system_regs == other.fields.access_system_regs &&
		       fields.permit_set_CID == other.fields.permit_set_CID && fields.uperms == other.fields.uperms &&
		       fields.tag == other.fields.tag;
	}

	void clearMetadata() {
		fields.uperms = 0;
		fields.permit_set_CID = false;
		fields.access_system_regs = false;
		fields.permit_unseal = false;
		fields.permit_cinvoke = false;
		fields.permit_seal = false;
		fields.permit_store_local_cap = false;
		fields.permit_store_cap = false;
		fields.permit_load_cap = false;
		fields.permit_store = false;
		fields.permit_load = false;
		fields.permit_execute = false;
		fields.global = false;
		fields.reserved = false;
		fields.flag_cap_mode = false;
		fields.internal_E = true;
		fields.E = cCapResetE;
		fields.B = 0;
		fields.T = cCapResetT;
		fields.otype = cOtypeUnsealedUnsigned;
	}

	uint16_t getCapHardPerms() const {
		return (fields.permit_set_CID << 11) | (fields.access_system_regs << 10) | (fields.permit_unseal << 9) |
		       (fields.permit_cinvoke << 8) | (fields.permit_seal << 7) | (fields.permit_store_local_cap << 6) |
		       (fields.permit_store_cap << 5) | (fields.permit_load_cap << 4) | (fields.permit_store << 3) |
		       (fields.permit_load << 2) | (fields.permit_execute << 1) | fields.global;
	}

	void getCapBounds(CapBase_t* base, CapTop_t* top) const {
		uint8_t E = std::min(cCapMaxE, fields.E);
		CapAddr_t a = fields.address;
		uint8_t a3 = (a >> (E + 11)) & 0b111;   // a[E+13 : E+11]
		uint8_t B3 = (fields.B >> 11) & 0b111;  // B[13:11]
		uint8_t T3 = (fields.T >> 11) & 0b111;  // T[13:11]
		uint8_t R3 = (B3 - 1) & 0b111;          // Intended wrap-around

		uint8_t aHi = a3 < R3;
		uint8_t bHi = B3 < R3;
		uint8_t tHi = T3 < R3;

		auto correctionBase = static_cast<int8_t>(bHi - aHi);
		auto correctionTop = static_cast<int8_t>(tHi - aHi);
		uint64_t a_top = 0;
		if (E + cCapMantissaWidth <= 63) {
			a_top = a >> (E + cCapMantissaWidth);
		}
		*base = 0;
		if (cCapMantissaWidth + E <= 63) {
			*base = (a_top + correctionBase) << (cCapMantissaWidth + E);
		}
		*base |= static_cast<CapBase_t>(fields.B) << E;
		*top = static_cast<CapTop_t>(fields.T) << E;
		*top += static_cast<CapTop_t>(a_top + correctionTop) << (cCapMantissaWidth + E);
		*top &= cCapLenMax;

		// Corner case
		// if ((E < 51) &((t[64 : 63] − b[63]) > 1)) then t[64] =!t[64]
		if ((E < (cCapMaxE - 1)) && ((((*top >> (cCapAddrWidth - 1)) - (*base >> (cCapAddrWidth - 1))) & 0b11) > 1)) {
			*top ^= (static_cast<CapTop_t>(1) << cCapAddrWidth);
		}
	}

	// /* An 'ideal' version of setCapBounds as described in paper. */
	bool setCapBounds(CapAddr_t base, CapLen_t top) {
		CapLen_t length = top - base;

		// Find an exponent that will put the most significant bit of length
		// second from the top as assumed during decoding. We ignore the bottom
		// MW bits because those are handled by the ie = 0 format.

		uint64_t leading_zeros = 0;
		if ((length >> 64 & 0b1) == 0) {
			if (length != 0) {
				leading_zeros = __builtin_clzll(static_cast<uint64_t>(length)) + 1;
			} else {
				leading_zeros = cCapLenWidth;
			}
		}
		// MW bits are ignored!
		if (leading_zeros > (cCapLenWidth - cCapMantissaWidth + 1)) {
			leading_zeros = cCapLenWidth - cCapMantissaWidth + 1;
		}

		uint8_t e = cCapMaxE - leading_zeros;
		bool ie = ((e != 0) | CHERI_BIT_SINGLE_P1(length, (cCapMantissaWidth - 2))) == 0b1;
		// // The non-ie e == 0 case is easy. It is exact so just extract relevant bits.
		uint16_t B = CHERI_BIT_RANGE(base, (cCapMantissaWidth - 1), 0);  // truncates
		uint16_t T = CHERI_BIT_RANGE(top, (cCapMantissaWidth - 1), 0);   // truncates
		bool lostSignificantTop = false;
		bool lostSignificantBase = false;
		bool incE = false;

		if (ie) {
			uint16_t B_ie = CHERI_BIT_RANGE(base >> (e + 3), (cCapMantissaWidth - 1), 0);
			uint16_t T_ie = CHERI_BIT_RANGE(top >> (e + 3), (cCapMantissaWidth - 1), 0);
			lostSignificantBase = CHERI_BIT_RANGE(base, e + 2, 0) != 0;
			lostSignificantTop = CHERI_BIT_RANGE(top, e + 2, 0) != 0;
			T_ie += lostSignificantTop;

			// Has the length overflowed? We chose e so that the top two bits of len would be 0b01,
			// but either because of incrementing T or losing bits of base it might have grown
			uint16_t len_ie = T_ie - B_ie;
			if (CHERI_BIT_SINGLE_P1(len_ie, (cCapMantissaWidth - 4))) {
				incE = true;
				lostSignificantBase |= CHERI_BIT_SINGLE_P1(B_ie, 0);
				lostSignificantTop |= CHERI_BIT_SINGLE_P1(T_ie, 0);
				B_ie = CHERI_BIT_RANGE((base >> (e + 4)), (cCapMantissaWidth - 1), 0);  // truncates
				T_ie = CHERI_BIT_RANGE((top >> (e + 4)), (cCapMantissaWidth - 1), 0);   // truncates
				T_ie += lostSignificantTop;
			}
			B = B_ie << 3;
			T = T_ie << 3;
		}
		uint8_t newE = e + incE;
		fields.address = static_cast<int64_t>(base);
		fields.E = newE;
		fields.B = B;
		fields.T = T;
		fields.internal_E = ie;
		bool exact = !(lostSignificantBase | lostSignificantTop);
		return exact;
	}

	uint32_t getCapPerms() const {
		uint32_t perms = 0;
		perms |= fields.uperms << cCapUPermsShift;
		perms |= getCapHardPerms();
		return perms;
	}

	void setCapPerms(uint32_t perms) {
		fields.uperms = CHERI_BIT_RANGE((perms >> cCapUPermsShift), cCapUPermsWidth - 1, 0);
		// 14..12 reserved -- ignore
		fields.permit_set_CID = (perms >> 11) & 1;
		fields.access_system_regs = (perms >> 10) & 1;
		fields.permit_unseal = (perms >> 9) & 1;
		fields.permit_cinvoke = (perms >> 8) & 1;
		fields.permit_seal = (perms >> 7) & 1;
		fields.permit_store_local_cap = (perms >> 6) & 1;
		fields.permit_store_cap = (perms >> 5) & 1;
		fields.permit_load_cap = (perms >> 4) & 1;
		fields.permit_store = (perms >> 3) & 1;
		fields.permit_load = (perms >> 2) & 1;
		fields.permit_execute = (perms >> 1) & 1;
		fields.global = perms & 1;
	}

	// /*!
	//  * Gets the architecture specific capability flags of capability.
	//  */
	uint8_t getFlags() const {
		return fields.flag_cap_mode;
	}

	// /*!
	//  * THIS`(flags)` sets the architecture specific capability flags to `flags`
	//  */
	void setFlags(uint8_t flags) {
		fields.flag_cap_mode = flags & 0b1;
	}

	bool isSealed() const {
		return fields.otype != cOtypeUnsealedUnsigned;
	}

	// /*!
	//  * Tests whether the capability has a reserved otype (larger than [cap_max_otype]).
	//  * Note that this includes both sealed (e.g. sentry) and unsealed (all ones)
	//  * otypes.
	//  */
	bool hasReservedOType() const {
		return fields.otype > cCapMaxOType;
	}

	void seal(uint32_t otype) {
		fields.otype = otype;
	}

	void unseal() {
		fields.otype = cOtypeUnsealedUnsigned;
	}

	CapAddr_t getBase() const {
		CapAddr_t base;
		CapTop_t top;
		getCapBounds(&base, &top);
		return base;
	}

	CapTop_t getTop() const {
		CapAddr_t base;
		CapTop_t top;
		getCapBounds(&base, &top);
		return top;
	}

	CapAddr_t getOffset() const {
		CapAddr_t base = getBase();
		return fields.address - base;
	}

	CapLen_t getLength() const {
		CapAddr_t base;
		CapTop_t top;
		getCapBounds(&base, &top);

		__int128_t mod_val = static_cast<__uint128_t>(1ULL) << cCapLenWidth;
		__uint128_t diff = top - base;
		__uint128_t len = diff % mod_val;
		return len;
	}

	bool inCapBounds(uint64_t addr, uint64_t size) const {
		CapAddr_t base;
		CapTop_t top;
		getCapBounds(&base, &top);
		return addr >= base && (static_cast<__uint128_t>(addr) + static_cast<__uint128_t>(size)) <= top;
	}

	void clearTagIf(bool cond) {
		fields.tag &= !cond;
	}

	void clearTagIfSealed() {
		return clearTagIf(isSealed());
	}

	void clearTag() {
		fields.tag = false;
	}

	bool setCapOffset(uint64_t offset) {
		CapAddr_t base = getBase();
		bool representable =
		    fastRepCheck(base + offset - fields.address);  // Must be done before actually incrementing address
		fields.address = static_cast<int64_t>(base + offset);
		return representable;
	}

	void setCapOffsetChecked(uint64_t offset) {
		bool representable = setCapOffset(offset);
		clearTagIf(!representable || isSealed());
	}

	bool incCapOffset(uint64_t delta) {
		bool representable = fastRepCheck(delta);  // Must be done before actually incrementing address
		fields.address = static_cast<int64_t>(fields.address + delta);
		return representable;
	}

	bool fastRepCheck(uint64_t i) const {
		uint8_t E = fields.E;
		if (E >= cCapMaxE - 2) {
			// in this case representable region is whole address space
			return true;
		}
		int64_t i_top = static_cast<int64_t>(i) >> (E + cCapMantissaWidth);  // arithmetic shift right
		uint64_t i_mid = CHERI_BIT_RANGE((i >> E), cCapMantissaWidth - 1, 0);
		uint64_t a_mid = CHERI_BIT_RANGE((fields.address >> E), cCapMantissaWidth - 1, 0);
		uint8_t B3 = cheri_truncateLsb(fields.B, 3, cCapMantissaWidth);
		uint8_t R3 = (B3 - 0b001) & 0b111;
		uint64_t R = R3 << (cCapMantissaWidth - 3);
		uint64_t diff = (R - a_mid) & ((1ULL << cCapMantissaWidth) - 1);  // truncate to cCapMantissaWidth bits (14)
		uint64_t diff1 = diff - 1;
		if (i_top == 0) {
			return i_mid < diff1;
		}
		if (i_top == -1) {
			return (i_mid >= diff) && (R != a_mid);
		}
		return false;
	}

	bool setCapAddr(int64_t addr) {
		CapAddr_t base1;
		CapLen_t top1;
		getCapBounds(&base1, &top1);
		fields.address = addr;
		CapAddr_t base2;
		CapLen_t top2;
		getCapBounds(&base2, &top2);
		return base1 == base2 && top1 == top2;
	}

	void setCapAddrChecked(int64_t addr) {
		bool representable = setCapAddr(addr);
		clearTagIf(!representable || isSealed());
	}

	EncCapability toEncCap() const {
		uint16_t t_hi = CHERI_BIT_SLICE(fields.T, cCapMantissaWidth - 3, cInternalETakeBits);
		uint16_t b_hi = CHERI_BIT_SLICE(fields.B, cCapMantissaWidth - 1, cInternalETakeBits);
		uint8_t t_lo, b_lo;

		if (fields.internal_E) {
			t_lo = CHERI_BIT_SLICE(fields.E, cInternalETakeBits * 2 - 1, cInternalETakeBits);
			b_lo = CHERI_BIT_SLICE(fields.E, cInternalETakeBits - 1, 0);
		} else {
			t_lo = CHERI_BIT_SLICE(fields.T, cInternalETakeBits - 1, 0);
			b_lo = CHERI_BIT_SLICE(fields.B, cInternalETakeBits - 1, 0);
		}

		return EncCapability{
		    static_cast<uint16_t>((static_cast<uint16_t>(fields.uperms) << cCapHPermsWidth | getCapHardPerms())),
		    fields.otype,
		    fields.reserved,
		    fields.flag_cap_mode,
		    fields.internal_E,
		    static_cast<uint16_t>((t_hi << cInternalETakeBits) | t_lo),
		    static_cast<uint16_t>((b_hi << cInternalETakeBits) | b_lo),
		    fields.address};
	}

	__uint128_t toUint128() const {
		return encCapToUint128(toEncCap()) ^ cNullCap128;
	}
};

constexpr Capability cNullCap =
    Capability(false, 0, false, false, false, false, false, false, false, false, false, false, false, false, 0, false,
               true, cCapResetE, 0, cCapResetT, cOtypeUnsealedUnsigned, 0);

constexpr Capability cDefaultCap =
    Capability(true, CHERI_MAX(cCapUPermsWidth), true, true, true, true, true, true, true, true, true, true, true, true,
               0, false, true, cCapResetE, 0, cCapResetT, cOtypeUnsealedUnsigned, 0);

struct ProgramCounterCapability {
	Capability pcc{};

	// ctor
	ProgramCounterCapability() {
		pcc = cDefaultCap;
	}

	// Return uint64_t value of the program counter
	operator uint64_t() const {
		return static_cast<uint64_t>(pcc.fields.address);
	}

	// Overload = operator to set the program counter
	ProgramCounterCapability& operator=(uint64_t val) {
		// TODO Check if metadata and tag should be cleared!
		pcc.fields.address = static_cast<int64_t>(val);
		return *this;
	}

	// Overload = operator to set the program counter with a Capability
	ProgramCounterCapability& operator=(const Capability& val) {
		pcc = val;
		return *this;
	}

	// Return Capability value of the program counter
	operator Capability() const {
		return pcc;
	}

	// Provide direct access to pcc fields
	Capability& operator*() {
		return pcc;
	}

	const Capability& operator*() const {
		return pcc;
	}

	// Provide direct pointer-like access to pcc
	Capability* operator->() {
		return &pcc;
	}

	const Capability* operator->() const {
		return &pcc;
	}

	// overload + operator
	ProgramCounterCapability operator+(uint64_t val) const {
		ProgramCounterCapability new_pcc = *this;
		new_pcc->fields.address = static_cast<int64_t>(val + new_pcc->fields.address);
		return new_pcc;
	}

	// overload + operator for int32_t
	ProgramCounterCapability operator+(int32_t val) const {
		ProgramCounterCapability new_pcc = *this;
		new_pcc->fields.address += val;
		return new_pcc;
	}

	// overload +=
	ProgramCounterCapability& operator+=(uint64_t val) {
		auto pc_val = static_cast<uint64_t>(pcc.fields.address);
		pc_val += val;
		pcc.fields.address = static_cast<int64_t>(pc_val);
		return *this;
	}
};

#endif  // CHERI_CAPABILITY_H
