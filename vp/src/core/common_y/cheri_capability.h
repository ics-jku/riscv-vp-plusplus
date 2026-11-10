#ifndef CHERI_CAPABILITY_H
#define CHERI_CAPABILITY_H

#include <cstdint>

#include "core/rv64y/cheri_prelude.h"  // TODO This must be changed when RV32 is implemented

struct Capability {
	struct cap {
		struct fields {
			int64_t address{};
			uint16_t B{};
			uint16_t T{};
			uint16_t reserved0{};
			uint8_t sd_perms{};
			ArchPerms AP{};
			uint8_t E{};
			uint8_t reserved1{};
			bool P{};
			bool GL{};
			bool sealed{};
			bool internal_E{};
			bool tag{};
		} fields;
	} cap;

	// Default constructor
	Capability() noexcept : cap{{0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, false}} {}

	constexpr Capability(bool tag, uint8_t sd_perms, bool P, uint8_t AP, bool GL, bool sealed, bool internal_E,
	                     uint8_t E, uint16_t B, uint16_t T, uint16_t reserved0, uint8_t reserved1, int64_t addr)
	    : cap{{addr, B, T, reserved0, sd_perms, AP, E, reserved1, P, GL, sealed, internal_E, tag}} {}

	constexpr Capability(__uint128_t data, bool tag) : cap{{}} {
		decode(data, tag);
	}

	// Ctor for Null Capability with address set to a specific value
	constexpr Capability(int64_t addr)
	    : cap{{addr, 0, cCapResetT, 0, 0, 0, cCapResetE, 0, false, false, false, true, false}} {}

   private:
	static constexpr uint64_t fieldMask(uint8_t width) {
		if (width >= 64) {
			return ~uint64_t{0};
		}
		return (uint64_t{1} << width) - 1;
	}

	static constexpr uint64_t readMetadataField(uint64_t meta, uint8_t rawShift, uint8_t width) {
		return (meta >> (rawShift - cXlen)) & fieldMask(width);
	}

	constexpr void decode(__uint128_t data, bool tag) {
		const uint64_t meta = static_cast<uint64_t>(data >> cXlen);
		uint16_t B = static_cast<uint16_t>(readMetadataField(meta, cEncCapBShift, cCapMantissaWidth));
		uint16_t T = static_cast<uint16_t>(readMetadataField(meta, cEncCapTShift, cCapMantissaWidth - 2));
		const bool internal_E = !readMetadataField(meta, cEncCapEFShift, 1);
		uint8_t E = 0;
		uint8_t lenMSBs = 0;

		if (internal_E) {
			E = static_cast<uint8_t>(((T & 0b111) << cInternalETakeBits) | (B & 0b111));
			E = cCapMaxE - E;  // Conversion to ensure all zero cap has infinite bounds
			lenMSBs = 0b01;
			T &= ~uint16_t{0b111};
			B &= ~uint16_t{0b111};
		}

		const uint8_t carry_out = T < CHERI_BIT_RANGE(B, cCapMantissaWidth - 3, 0);
		const uint16_t Ttop2 =
		    CHERI_BIT_SLICE(B, (cCapMantissaWidth - 1), (cCapMantissaWidth - 2)) + lenMSBs + carry_out;
		T |= static_cast<uint16_t>(Ttop2 << (cCapMantissaWidth - 2));

		cap.fields.address = static_cast<int64_t>(static_cast<uint64_t>(data));
		cap.fields.B = B;
		cap.fields.T = T;
		cap.fields.reserved0 =
		    static_cast<uint16_t>(readMetadataField(meta, cEncCapReserved0Shift, cCapReserved0Width));
		cap.fields.sd_perms = static_cast<uint8_t>(readMetadataField(meta, cEncCapSDPermsShift, cSDPermsWidth));
		cap.fields.AP = ArchPerms(static_cast<uint8_t>(readMetadataField(meta, cEncCapAPShift, cAPWidth)));
		cap.fields.E = E;
		cap.fields.reserved1 = static_cast<uint8_t>(readMetadataField(meta, cEncCapReserved1Shift, cCapReserved1Width));
		cap.fields.P = readMetadataField(meta, cEncCapPShift, 1);
		cap.fields.GL = readMetadataField(meta, cEncCapGLShift, 1);
		cap.fields.sealed = readMetadataField(meta, cEncCapCTShift, 1);
		cap.fields.internal_E = internal_E;
		cap.fields.tag = tag;
	}

   public:
	operator int64_t() const {
		return cap.fields.address;
	}

	// Set the address field of the Capability
	// This must clear the tag bit and metadata of the capability
	Capability& operator=(int64_t val) {
		cap.fields.tag = false;
		clearMetadata();
		cap.fields.address = val;
		return *this;
	}

	Capability& operator|=(uint64_t val) {
		return *this = static_cast<int64_t>(static_cast<uint64_t>(cap.fields.address) | val);
	}

	Capability& operator&=(uint64_t val) {
		return *this = static_cast<int64_t>(static_cast<uint64_t>(cap.fields.address) & val);
	}

	// Overload == operator to compare two Capabilities
	inline bool operator==(const Capability& other) const {
		return cap.fields.address == other.cap.fields.address && cap.fields.AP == other.cap.fields.AP &&
		       cap.fields.B == other.cap.fields.B && cap.fields.E == other.cap.fields.E &&
		       cap.fields.GL == other.cap.fields.GL && cap.fields.internal_E == other.cap.fields.internal_E &&
		       cap.fields.P == other.cap.fields.P && cap.fields.reserved0 == other.cap.fields.reserved0 &&
		       cap.fields.reserved1 == other.cap.fields.reserved1 && cap.fields.sd_perms == other.cap.fields.sd_perms &&
		       cap.fields.sealed == other.cap.fields.sealed && cap.fields.T == other.cap.fields.T &&
		       cap.fields.tag == other.cap.fields.tag;
	}

	void clearMetadata() {
		cap.fields.AP = cArchNullPerms;
		cap.fields.GL = 0;
		cap.fields.P = 0;
		cap.fields.reserved0 = 0;
		cap.fields.reserved1 = 0;
		cap.fields.sd_perms = 0;
		cap.fields.sealed = 0;
		cap.fields.internal_E = true;
		cap.fields.E = cCapResetE;
		cap.fields.B = 0;
		cap.fields.T = cCapResetT;
	}

	void getCapBounds(CapBase_t* base, CapTop_t* top) const {
		if (boundsMalformed()) {
			*base = 0;
			*top = 0;
			return;
		}

		uint8_t E = std::min(cCapMaxE, cap.fields.E);
		CapAddr_t a = cap.fields.address;

		uint16_t a_mid = (a >> E) & CHERI_MAX(cCapMantissaWidth);
		uint16_t correction_B = cap.fields.B;
		uint16_t correction_T = cap.fields.T;
		uint16_t R = (correction_B - (1 << (cCapMantissaWidth - 2))) & CHERI_MAX(cCapMantissaWidth);

		uint8_t aHi = a_mid < R;
		uint8_t bHi = correction_B < R;
		uint8_t tHi = correction_T < R;

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
		*base |= static_cast<CapBase_t>(cap.fields.B) << E;
		*top = static_cast<CapTop_t>(cap.fields.T) << E;
		*top += static_cast<CapTop_t>(a_top + correctionTop) << (cCapMantissaWidth + E);
		*top &= cCapLenMax;

		// Corner case
		// if ((E < 51) &((t[64 : 63] − b[63]) > 1)) then t[64] =!t[64]
		if ((E < (cCapMaxE - 1)) && ((((*top >> (cCapAddrWidth - 1)) - (*base >> (cCapAddrWidth - 1))) & 0b11) > 1)) {
			*top ^= (static_cast<CapTop_t>(1) << cCapAddrWidth);
		}
	}

	// /* An 'ideal' version of setCapBounds as described in paper. */permitAccessSystemRegs()
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
			constexpr uint16_t capIeMask = (1u << (cCapMantissaWidth - cInternalETakeBits)) - 1;
			uint16_t B_ie = CHERI_BIT_RANGE(base >> (e + 3), (cCapMantissaWidth - cInternalETakeBits - 1), 0);
			uint16_t T_ie = CHERI_BIT_RANGE(top >> (e + 3), (cCapMantissaWidth - cInternalETakeBits - 1), 0);
			lostSignificantBase = CHERI_BIT_RANGE(base, e + 2, 0) != 0;
			lostSignificantTop = CHERI_BIT_RANGE(top, e + 2, 0) != 0;
			T_ie = (T_ie + lostSignificantTop) & capIeMask;

			// Has the length overflowed? We chose e so that the top two bits of len would be 0b01,
			// but either because of incrementing T or losing bits of base it might have grown
			uint16_t len_ie = (T_ie - B_ie) & capIeMask;
			if (CHERI_BIT_SINGLE_P1(len_ie, (cCapMantissaWidth - 4))) {
				incE = true;
				lostSignificantBase |= CHERI_BIT_SINGLE_P1(B_ie, 0);
				lostSignificantTop |= CHERI_BIT_SINGLE_P1(T_ie, 0);
				B_ie =
				    CHERI_BIT_RANGE((base >> (e + 4)), (cCapMantissaWidth - cInternalETakeBits - 1), 0);    // truncates
				T_ie = CHERI_BIT_RANGE((top >> (e + 4)), (cCapMantissaWidth - cInternalETakeBits - 1), 0);  // truncates
				T_ie = (T_ie + lostSignificantTop) & capIeMask;
			}
			B = B_ie << 3;
			T = T_ie << 3;
		}
		uint8_t newE = e + incE;
		cap.fields.address = static_cast<int64_t>(base);
		cap.fields.E = newE;
		cap.fields.B = B;
		cap.fields.T = T;
		cap.fields.internal_E = ie;
		bool exact = !(lostSignificantBase | lostSignificantTop);
		return exact;
	}

	bool isSealed() const {
		return cap.fields.sealed;
	}

	void seal() {
		cap.fields.sealed = true;
	}

	void unseal() {
		cap.fields.sealed = false;
	}

	bool capMode() const {
		// Invert because P=0 means capability mode, P=1 means non-capability mode
		return !cap.fields.P;
	}

	void capMode(bool value) {
		// Invert because P=0 means capability mode, P=1 means non-capability mode
		cap.fields.P = !value;
	}

	bool permitExecute() const {
		return cap.fields.AP.X();
	}

	bool permitAccessSystemRegs() {
		return cap.fields.AP.ASR();
	}

	bool permitLoad() const {
		return cap.fields.AP.R();
	}

	bool permitStore() const {
		return cap.fields.AP.W();
	}

	bool permitCap() const {
		return cap.fields.AP.C();
	}

	bool isSubsetOf(const Capability& other) const {
		CapAddr_t c0_base, c1_base;
		CapTop_t c0_top, c1_top;
		getCapBounds(&c0_base, &c0_top);
		other.getCapBounds(&c1_base, &c1_top);
		bool bounds_subset = (c0_base >= c1_base) && (c0_top <= c1_top);

		uint8_t c0_perms = getArchPerms().toUInt8();
		uint8_t c1_perms = other.getArchPerms().toUInt8();
		bool perms_subset = (c0_perms & c1_perms) == c0_perms;
		bool level_subset = cap.fields.GL <= other.cap.fields.GL;  // TODO Check other bits of CapLevel as well?
		bool reserved_valid = reservedValid() && other.reservedValid();
		return bounds_subset && perms_subset && level_subset && reserved_valid;
	}
	ArchPerms getArchPermsLegalized() const {
		ArchPerms archPerms = getArchPerms();

		if (!checkIntegrity()) {
			return cArchNullPerms;
		}
		return archPerms;
	}

	bool checkIntegrity() const {
		if (boundsMalformed()) {
			return false;
		}
		if (!reservedValid()) {
			return false;
		}
		if (permsMalformed()) {
			return false;
		}
		return true;
	}

	bool permsMalformed() const {
		uint64_t perms = packedPerms();
		uint64_t fixed_perms = fixMalformedPerms(perms);
		return perms != fixed_perms;
	}

	ArchPerms getArchPerms() const {
		return cap.fields.AP;
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
		const CapTop_t address_space_size = static_cast<CapTop_t>(1) << cCapAddrWidth;
		// Memory accesses may wrap only if the bounds cover the entire address space.
		if (base == 0 && top >= address_space_size)
			return true;
		const CapTop_t access_end = static_cast<CapTop_t>(addr) + static_cast<CapTop_t>(size);
		return addr >= base && access_end <= std::min(top, address_space_size);
	}

	// Return true if the range [addr, addr+size) is in the cap bounds.
	// The upper bound arithmetic is full-width and isn't wrapped.
	bool inCapBoundsNoWrap(uint64_t addr, uint64_t size) const {
		CapAddr_t base;
		CapTop_t top;
		getCapBounds(&base, &top);
		CapTop_t addr_high = (CapTop_t)addr + (CapTop_t)size;
		return addr >= base && (addr_high <= top);
	}

	void clearTagIf(bool cond) {
		cap.fields.tag &= !cond;
	}

	void clearTag() {
		cap.fields.tag = false;
	}

	void incCapAddrChecked(int64_t delta) {
		setCapAddrChecked(cap.fields.address + delta);
	}

	bool setCapAddr(int64_t addr) {
		CapAddr_t base1;
		CapLen_t top1;
		getCapBounds(&base1, &top1);
		cap.fields.address = addr;
		CapAddr_t base2;
		CapLen_t top2;
		getCapBounds(&base2, &top2);
		return base1 == base2 && top1 == top2;
	}

	void setCapAddrChecked(int64_t addr) {
		bool representable = setCapAddr(addr);
		clearTagIf(!representable || isSealed() || !reservedValid());
	}

	bool boundsMalformed() const {
		if (!cap.fields.internal_E)
			return false;
		bool malformedMSB = (cap.fields.E == cCapMaxE && cap.fields.B != 0) ||
		                    (cap.fields.E == (cCapMaxE - 1) && cap.fields.B >> (cCapMantissaWidth - 1) != 0);
		bool malformedLSB = cap.fields.E > cCapMaxE || (cap.fields.E == 0 && (cap.fields.B & 0b111) != 0);
		return malformedMSB || malformedLSB;
	}

	bool reservedValid() const {
		return cap.fields.reserved1 == 0 && cap.fields.reserved0 == 0;
	}

	EncCapability toEncCap() const {
		return EncCapability(toUint128());
	}

	uint64_t getMetadata() const {
		uint8_t E = cCapMaxE - cap.fields.E;  // Conversion to ensure all zero cap has infinite bounds
		uint16_t t_hi = CHERI_BIT_SLICE(cap.fields.T, cCapMantissaWidth - 3, cInternalETakeBits);
		uint16_t b_hi = CHERI_BIT_SLICE(cap.fields.B, cCapMantissaWidth - 1, cInternalETakeBits);
		uint8_t t_lo, b_lo;

		if (cap.fields.internal_E) {
			t_lo = CHERI_BIT_SLICE(E, cInternalETakeBits * 2 - 1, cInternalETakeBits);
			b_lo = CHERI_BIT_SLICE(E, cInternalETakeBits - 1, 0);
		} else {
			t_lo = CHERI_BIT_SLICE(cap.fields.T, cInternalETakeBits - 1, 0);
			b_lo = CHERI_BIT_SLICE(cap.fields.B, cInternalETakeBits - 1, 0);
		}

		const uint16_t encT = static_cast<uint16_t>((t_hi << cInternalETakeBits) | t_lo);
		const uint16_t encB = static_cast<uint16_t>((b_hi << cInternalETakeBits) | b_lo);

		uint64_t metadata = 0;
		metadata |= (static_cast<uint64_t>(encB) & fieldMask(cCapMantissaWidth)) << (cEncCapBShift - cXlen);
		metadata |= (static_cast<uint64_t>(encT) & fieldMask(cCapMantissaWidth - 2)) << (cEncCapTShift - cXlen);
		metadata |= static_cast<uint64_t>(!cap.fields.internal_E) << (cEncCapEFShift - cXlen);
		metadata |= static_cast<uint64_t>(cap.fields.sealed) << (cEncCapCTShift - cXlen);
		metadata |= (static_cast<uint64_t>(cap.fields.reserved0) & fieldMask(cCapReserved0Width))
		            << (cEncCapReserved0Shift - cXlen);
		metadata |= static_cast<uint64_t>(cap.fields.GL) << (cEncCapGLShift - cXlen);
		metadata |= (static_cast<uint64_t>(cap.fields.AP.toUInt8()) & fieldMask(cAPWidth)) << (cEncCapAPShift - cXlen);
		metadata |= static_cast<uint64_t>(cap.fields.P) << (cEncCapPShift - cXlen);
		metadata |= (static_cast<uint64_t>(cap.fields.sd_perms) & fieldMask(cSDPermsWidth))
		            << (cEncCapSDPermsShift - cXlen);
		metadata |= (static_cast<uint64_t>(cap.fields.reserved1) & fieldMask(cCapReserved1Width))
		            << (cEncCapReserved1Shift - cXlen);
		return metadata;
	}

	__uint128_t toUint128() const {
		return (static_cast<__uint128_t>(getMetadata()) << cXlen) | static_cast<uint64_t>(cap.fields.address);
	}
	uint64_t packedPerms() const {
		ArchPerms arch_perms = getArchPerms();
		uint64_t raw = arch_perms.W() << cCapPermBitFieldWShift;
		raw |= (cap.fields.sd_perms & CHERI_MAX(cSDPermsWidth)) << cCapPermBitFieldSDPShift;
		raw |= (arch_perms.ASR() ? 1 : 0) << cCapPermBitFieldASRShift;
		raw |= (arch_perms.X() ? 1 : 0) << cCapPermBitFieldXShift;
		raw |= (arch_perms.R() ? 1 : 0) << cCapPermBitFieldRShift;
		raw |= (arch_perms.C() ? 1 : 0) << cCapPermBitFieldCShift;
		raw |= (arch_perms.LM() ? 1 : 0) << cCapPermBitFieldLMShift;
		raw |= (arch_perms.SL() ? 1 : 0) << cCapPermBitFieldSLShift;
		raw |= (cap.fields.GL ? 1 : 0) << cCapPermBitFieldGLShift;
		raw |= (arch_perms.LG() ? 1 : 0) << cCapPermBitFieldLGShift;
		// raw |= (cap.fields.reserved0) << cCapPermBitFieldReserved0Shift; // Zeros, disregarding of capability field
		raw |= CHERI_MAX(cCapPermBitFieldReserved1_0Width) << cCapPermBitFieldReserved1_0Shift;
		raw |= CHERI_MAX(cCapPermBitFieldReserved1_1Width) << cCapPermBitFieldReserved1_1Shift;
		return raw;
	}

	uint64_t packedPermsLegalized() const {
		if (!checkIntegrity()) {
			return CHERI_MAX(cCapPermBitFieldReserved1_0Width) << cCapPermBitFieldReserved1_0Shift |
			       CHERI_MAX(cCapPermBitFieldReserved1_1Width) << cCapPermBitFieldReserved1_1Shift;
		}
		ArchPerms arch_perms = getArchPermsLegalized();
		uint64_t raw = arch_perms.W() << cCapPermBitFieldWShift;
		raw |= (cap.fields.sd_perms & CHERI_MAX(cSDPermsWidth)) << cCapPermBitFieldSDPShift;
		raw |= (arch_perms.ASR() ? 1 : 0) << cCapPermBitFieldASRShift;
		raw |= (arch_perms.X() ? 1 : 0) << cCapPermBitFieldXShift;
		raw |= (arch_perms.R() ? 1 : 0) << cCapPermBitFieldRShift;
		raw |= (arch_perms.C() ? 1 : 0) << cCapPermBitFieldCShift;
		raw |= (arch_perms.LM() ? 1 : 0) << cCapPermBitFieldLMShift;
		raw |= (arch_perms.SL() ? 1 : 0) << cCapPermBitFieldSLShift;
		raw |= (cap.fields.GL ? 1 : 0) << cCapPermBitFieldGLShift;
		raw |= (arch_perms.LG() ? 1 : 0) << cCapPermBitFieldLGShift;
		// raw |= (cap.fields.reserved0) << cCapPermBitFieldReserved0Shift; // Zeros, disregarding of capability field
		raw |= CHERI_MAX(cCapPermBitFieldReserved1_0Width) << cCapPermBitFieldReserved1_0Shift;
		raw |= CHERI_MAX(cCapPermBitFieldReserved1_1Width) << cCapPermBitFieldReserved1_1Shift;
		return raw;
	}

	void setPackedPerms(uint64_t raw) {
		uint8_t archPerms = 0;
		archPerms |= ((raw >> cCapPermBitFieldWShift) & 0b1) << cArchPermWShift;
		archPerms |= ((raw >> cCapPermBitFieldASRShift) & 0b1) << cArchPermASRShift;
		archPerms |= ((raw >> cCapPermBitFieldXShift) & 0b1) << cArchPermXShift;
		archPerms |= ((raw >> cCapPermBitFieldRShift) & 0b1) << cArchPermRShift;
		archPerms |= ((raw >> cCapPermBitFieldCShift) & 0b1) << cArchPermCShift;
		archPerms |= ((raw >> cCapPermBitFieldLMShift) & 0b1) << cArchPermLMShift;
		archPerms |= ((raw >> cCapPermBitFieldSLShift) & 0b1) << cArchPermSLShift;
		archPerms |= ((raw >> cCapPermBitFieldLGShift) & 0b1) << cArchPermLGShift;
		cap.fields.AP = ArchPerms(archPerms);
		cap.fields.sd_perms = (raw >> cCapPermBitFieldSDPShift) & CHERI_MAX(cSDPermsWidth);
		cap.fields.GL = (raw >> cCapPermBitFieldGLShift) & 0b1;
		// Ignore reserved bits
	}

	void legalizeMode() {
		// If X permission is not set, then P must be cleared as well
		if (!cap.fields.AP.X()) {
			cap.fields.P = false;
		}
	}
};

static_assert(sizeof(Capability::cap) <= 24, "Capability metadata storage should stay compact");
static_assert(sizeof(Capability) <= 32, "Capability should stay cheap to copy");
static_assert(alignof(Capability) <= alignof(uint64_t), "Capability alignment should remain register-file friendly");

// constexpr Capability                   (bool tag, uint8_t sd_perms, bool P,uint8_t AP, bool GL, bool sealed, bool
// internal_E, uint8_t E, uint16_t B, uint16_t T, uint16_t reserved0, uint8_t reserved1, int64_t addr)
constexpr Capability cNullCap = Capability(false, 0, false, 0, false, false, true, cCapResetE, 0, cCapResetT, 0, 0,
                                           0);  // TODO Not sure if this is correct (especially bounds!)

constexpr Capability cDefaultCap = Capability(true, CHERI_MAX(cSDPermsWidth), true, CHERI_MAX(cAPWidth), true, false,
                                              true, cCapResetE, 0, cCapResetT, 0, 0, 0);

struct ProgramCounterCapability {
	Capability pcc{};

	// ctor
	ProgramCounterCapability() {
		pcc = cDefaultCap;
	}

	// Return uint64_t value of the program counter
	operator uint64_t() const {
		return static_cast<uint64_t>(pcc.cap.fields.address);
	}

	// Overload = operator to set the program counter
	ProgramCounterCapability& operator=(uint64_t val) {
		pcc.cap.fields.address = static_cast<int64_t>(val);
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
		new_pcc->cap.fields.address = static_cast<int64_t>(val + new_pcc->cap.fields.address);
		return new_pcc;
	}

	// overload + operator for int32_t
	ProgramCounterCapability operator+(int32_t val) const {
		ProgramCounterCapability new_pcc = *this;
		new_pcc->cap.fields.address += val;
		return new_pcc;
	}

	// overload +=
	ProgramCounterCapability& operator+=(uint64_t val) {
		auto pc_val = static_cast<uint64_t>(pcc.cap.fields.address);
		pc_val += val;
		pcc.cap.fields.address = static_cast<int64_t>(pc_val);
		return *this;
	}
};

#endif  // CHERI_CAPABILITY_H
