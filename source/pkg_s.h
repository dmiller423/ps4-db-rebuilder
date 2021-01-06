#pragma once

// pkg_h: Big Endian ( leftover ps3 cruft fun )
//
//PACK(
typedef struct pkg_header {
	uint32_t pkg_magic;                      // 0x000 - 0x7F434E54
	uint32_t pkg_type;                       // 0x004
	uint32_t pkg_0x008;                      // 0x008 - unknown field
	uint32_t pkg_file_count;                 // 0x00C
	uint32_t pkg_entry_count;                // 0x010
	uint16_t pkg_sc_entry_count;             // 0x014
	uint16_t pkg_entry_count_2;              // 0x016 - same as pkg_entry_count
	uint32_t pkg_entry_offset;               // 0x018 - entry table offset
	uint32_t pkg_entry_data_size;            // 0x01C
	uint64_t pkg_body_offset;                // 0x020 - offset of PKG entries
	uint64_t pkg_body_size;                  // 0x028 - length of all PKG entries
	uint64_t pkg_content_offset;             // 0x030
	uint64_t pkg_content_size;               // 0x038
	uint8_t  pkg_content_id[0x24];           // 0x040 - packages' content ID as a 36-byte string
	uint8_t  pkg_padding[0xC];               // 0x064 - padding
	uint32_t pkg_drm_type;                   // 0x070 - DRM type
	uint32_t pkg_content_type;               // 0x074 - Content type
	uint32_t pkg_content_flags;              // 0x078 - Content flags
	uint32_t pkg_promote_size;               // 0x07C
	uint32_t pkg_version_date;               // 0x080
	uint32_t pkg_version_hash;               // 0x084
	uint32_t pkg_0x088;                      // 0x088
	uint32_t pkg_0x08C;                      // 0x08C
	uint32_t pkg_0x090;                      // 0x090
	uint32_t pkg_0x094;                      // 0x094
	uint32_t pkg_iro_tag;                    // 0x098
	uint32_t pkg_drm_type_version;           // 0x09C
											 /* Digest table */
	uint8_t  digest_entries1[0x20];          // 0x100 - sha256 digest for main entry 1
	uint8_t  digest_entries2[0x20];          // 0x120 - sha256 digest for main entry 2
	uint8_t  digest_table_digest[0x20];      // 0x140 - sha256 digest for digest table
	uint8_t  digest_body_digest[0x20];       // 0x160 - sha256 digest for main table
	uint8_t z_pada[0x2E0];

	uint32_t pfs_sbo;						 // 0x400 - should be one ?  image count # 2 ? idk
	uint32_t pfs_image_count;                // 0x404 - count of PFS images
	uint64_t pfs_image_flags;                // 0x408 - PFS flags
	uint64_t pfs_image_offset;               // 0x410 - offset to start of external PFS image
	uint64_t pfs_image_size;                 // 0x418 - size of external PFS image
	uint64_t mount_image_offset;             // 0x420
	uint64_t mount_image_size;               // 0x428
	uint64_t pkg_size;                       // 0x430
	uint32_t pfs_signed_size;                // 0x438
	uint32_t pfs_cache_size;                 // 0x43C
	uint8_t  pfs_image_digest[0x20];         // 0x440
	uint8_t  pfs_signed_digest[0x20];        // 0x460
	uint64_t pfs_split_size_nth_0;           // 0x480
	uint64_t pfs_split_size_nth_1;           // 0x488

											 //	uint64_t z_padb[0xB50]; // [0x1000-offsetof(pkg_header_t, pfs_split_size_nth_1) + 8 -0x20];
											 //	uint8_t  pkg_digest[0x20];          // 0xFE0
} PkgHeader;


//static_assert(0x1000 == sizeof(pkg_header));


#define PKG_MAGIC 0x7F434E54


enum PkgType
{
	Version1 = 0x01,
	Version2 = 0x02,
	Internal = 0x40,
	Final    = 0x80,	// ?
						//	Unknown  = 0x01000000
};


enum EntryId : u32
{
	Digest		= 0x00000001,
	EntryKey	= 0x00000010,
	ImageKey	= 0x00000020,
	GenDigest	= 0x00000080,
	Meta		= 0x00000100,
	EntryName	= 0x00000200,

	LicenseDat	= 0x00000400,
	LicenseInfo	= 0x00000401,
	NpTitle		= 0x00000402,
	NpBind		= 0x00000403,
	SelfInfo	= 0x00000404,
	ImageInfo	= 0x00000406,
	TargetDelta	= 0x00000407,
	OriginDelta	= 0x00000408,
	PSReserved	= 0x00000409,

	ParamSFO	= 0x00001000,
	PlayGoDat	= 0x00001001,
	PlayGoHash	= 0x00001002,
	PlayGoManif	= 0x00001003,
	PronunXml	= 0x00001004,
	PronunSig	= 0x00001005,
	Pic1_Png	= 0x00001006,
	PubToolInfo	= 0x00001007,
	AppPGoDat	= 0x00001008,
	AppPGoHash	= 0x00001009,
	AppPGoManif	= 0x0000100A,
	ShareParam	= 0x0000100B,
	ShareImage	= 0x0000100C,
	SaveDataPng	= 0x0000100D,
	SharePrivPng= 0x0000100E,

	Icon0_Png	= 0x00001200,
	Icon0_Last	= 0x0000121F,

	Pic0_Png	= 0x00001220,
	Snd0_AT9	= 0x00001240,

	Pic1_Png_00	= 0x00001241,
	Pic1_Last	= 0x0000125F,

	ChangeInfo	= 0x00001260,
	ChInfoLast	= 0x0000127F,

	Icon0_DDS_First	= 0x00001280,
	Icon0_DDS_Last	= 0x0000129F,

	// There are more, don't need many of these atm //

	EntryId_Last
};




typedef struct pkg_table_entry {
	uint32_t id;               // File ID, useful for files without a filename entry
	uint32_t nameOffs;  // Offset into the filenames table (ID 0x200) where this file's name is located
	uint32_t flags1;           // Flags including encrypted flag, etc
	uint32_t flags2;           // Flags including encryption key index, etc
	uint32_t dataOffs;           // Offset into PKG to find the file
	uint32_t size;             // Size of the file
	uint64_t padding;          // blank padding
} PkgEntry;






