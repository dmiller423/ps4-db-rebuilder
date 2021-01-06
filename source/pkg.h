#pragma once
#include "pkg_s.h"
#include "sfo.h"



#ifdef _WIN32 // || _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 
#define open	_open
#define close	_close
#define lseek	_lseek
#define read	_read
#define write	_write
#else
#define O_BINARY 0	// stupid wintendo
#endif


class Pkg
{
	string filePath;

	PkgHeader hdr;
	vector<PkgEntry> entries;

	vector<u8> sfoData;
public:
	SfoReader sfo;
	u64 pkgSize=0;

	Pkg(const string& pkgPath)
		: filePath(pkgPath)
	{
		memset(&hdr, 0, sizeof(PkgHeader));

		int fdr = 0;
		if ((fdr=open(pkgPath.c_str(), O_RDONLY|O_BINARY)) <= 0) {
			klog("open(src): got fdr %d , errno 0x%08X\n", fdr, errno);
			return;
		}
		pkgSize = lseek(fdr, 0, SEEK_END);
		klog("-src size: %ld bytes!\n", pkgSize);
		lseek(fdr, 0, SEEK_SET);
		if (pkgSize < sizeof(PkgHeader))
			return;

		u64 actual=0;
		if (sizeof(PkgHeader)!=(actual=read(fdr, &hdr, sizeof(PkgHeader)))) {
			klog("@@ bad read A, read %ld / %ld bytes : errno: 0x%08X\n", actual, sizeof(PkgHeader), errno);
			return ;
		}

		byteSwapHdr();
		if (PKG_MAGIC != hdr.pkg_magic) {
			klog("Pkg Invalid (bad magic: %08X)\n", hdr.pkg_magic);
			return ;
		}

		lseek(fdr, hdr.pkg_entry_offset, SEEK_SET);

		size_t readSize = sizeof(PkgEntry) * hdr.pkg_entry_count;
		vector<u8> fileBuff(AlignUp<size_t>(readSize, 4096), 0);
		if (readSize!=(actual=read(fdr, &fileBuff[0], readSize))) {
			klog("@@ bad read B, read %ld / %ld bytes : errno: 0x%08X\n", actual, readSize, errno);
			return;
		}


		for (u32 ei = 0; ei < hdr.pkg_entry_count; ei++) {
			PkgEntry pe = ((PkgEntry*)&fileBuff[0])[ei];
			byteSwapEntry(pe);
			entries.push_back(pe);

			if (pe.id==ParamSFO) {
				klog("Found Param.SFO @ 0x%08X + %d \n", pe.dataOffs, pe.size); //sizeStr().c_str());
				if (pe.size && (pe.dataOffs+pe.size) < pkgSize) {
					readSize = pe.size;
					sfoData.resize(readSize);
					memset(&sfoData[0], 0, readSize);

					if(-1==lseek(fdr, pe.dataOffs, SEEK_SET)) klog("lseek fail? wtf errno=%d\n", errno);

					if (readSize!=(actual=read(fdr, &sfoData[0], readSize))) {
						klog("@@ bad read SFO, read %ld / %ld bytes : errno: %d\n", actual, readSize, errno);
					//	return;
					}
					sfo.setData(sfoData);
				}
			}
		}

		close(fdr);
	}


	bool isValid() {
		return (PKG_MAGIC == hdr.pkg_magic)
				&& (entries.size() > 0);
	}









	void printEntries()	{
		for (const auto& e : entries) {
			klog("Entry: ID 0x%08X Offs: 0x%08X Size: %s \"%s\" \n",
				e.id, e.dataOffs, sizeStr(e.size).c_str(), entryName(static_cast<EntryId>(e.id)).c_str());
		}
	}

	
	void printHdr() {
		klog("%04zX: pkg_magic:               %X\n", offsetof(PkgHeader,pkg_magic),				hdr.pkg_magic);
		klog("%04zX: pkg_type:                %X\n", offsetof(PkgHeader,pkg_type),				hdr.pkg_type);
		klog("%04zX: pkg_0x008:               %X\n", offsetof(PkgHeader,pkg_0x008),				hdr.pkg_0x008);
		klog("%04zX: pkg_file_count:          %X\n", offsetof(PkgHeader,pkg_file_count),		hdr.pkg_file_count);
		klog("%04zX: pkg_entry_count:         %X\n", offsetof(PkgHeader,pkg_entry_count),		hdr.pkg_entry_count);
		klog("%04zX: pkg_sc_entry_count:      %X\n", offsetof(PkgHeader,pkg_sc_entry_count),	hdr.pkg_sc_entry_count);
		klog("%04zX: pkg_entry_count_2:       %X\n", offsetof(PkgHeader,pkg_entry_count_2),		hdr.pkg_entry_count_2);
		klog("%04zX: pkg_table_offset:        %X\n", offsetof(PkgHeader,pkg_entry_offset),		hdr.pkg_entry_offset);
		klog("%04zX: pkg_entry_data_size:     %X\n", offsetof(PkgHeader,pkg_entry_data_size),	hdr.pkg_entry_data_size);

		klog("%04zX: pkg_body_offset:         %IX\n", offsetof(PkgHeader,pkg_body_offset),		hdr.pkg_body_offset);
		klog("%04zX: pkg_body_size:           %IX\n", offsetof(PkgHeader,pkg_body_size),		hdr.pkg_body_size);
		klog("%04zX: pkg_content_offset:      %IX\n", offsetof(PkgHeader,pkg_content_offset),	hdr.pkg_content_offset);
		klog("%04zX: pkg_content_size:        %IX\n", offsetof(PkgHeader,pkg_content_size),		hdr.pkg_content_size);

	//	hdr.pkg_content_id[35] = 0;
		klog("%04zX: pkg_content_id:		\"%s\" \n", offsetof(PkgHeader, pkg_content_id),	hdr.pkg_content_id);

		klog("%04zX: pkg_drm_type:            %X\n", offsetof(PkgHeader,pkg_drm_type),			hdr.pkg_drm_type);
		klog("%04zX: pkg_content_type:        %X\n", offsetof(PkgHeader,pkg_content_type),		hdr.pkg_content_type);
		klog("%04zX: pkg_content_flags:       %X\n", offsetof(PkgHeader,pkg_content_flags),		hdr.pkg_content_flags);
		klog("%04zX: pkg_promote_size:        %X\n", offsetof(PkgHeader,pkg_promote_size),		hdr.pkg_promote_size);
		klog("%04zX: pkg_version_date:        %X\n", offsetof(PkgHeader,pkg_version_date),		hdr.pkg_version_date);
		klog("%04zX: pkg_version_hash:        %X\n", offsetof(PkgHeader,pkg_version_hash),		hdr.pkg_version_hash);
		klog("%04zX: pkg_0x088:               %X\n", offsetof(PkgHeader,pkg_0x088),				hdr.pkg_0x088);
		klog("%04zX: pkg_0x08C:               %X\n", offsetof(PkgHeader,pkg_0x08C),				hdr.pkg_0x08C);
		klog("%04zX: pkg_0x090:               %X\n", offsetof(PkgHeader,pkg_0x090),				hdr.pkg_0x090);
		klog("%04zX: pkg_0x094:               %X\n", offsetof(PkgHeader,pkg_0x094),				hdr.pkg_0x094);
		klog("%04zX: pkg_iro_tag:             %X\n", offsetof(PkgHeader,pkg_iro_tag),			hdr.pkg_iro_tag);
		klog("%04zX: pkg_drm_type_version:    %X\n", offsetof(PkgHeader,pkg_drm_type_version),	hdr.pkg_drm_type_version);

		klog("%04zX: pfs_image_count:          %X\n", offsetof(PkgHeader,pfs_image_count),		hdr.pfs_image_count);                // 0x404 - count of PFS images
		klog("%04zX: pfs_image_flags:         %IX\n", offsetof(PkgHeader,pfs_image_flags),		hdr.pfs_image_flags);                // 0x408 - PFS flags
		klog("%04zX: pfs_image_offset:        %IX\n", offsetof(PkgHeader,pfs_image_offset),		hdr.pfs_image_offset);               // 0x410 - offset to start of external PFS image
		klog("%04zX: pfs_image_size:          %IX\n", offsetof(PkgHeader,pfs_image_size),		hdr.pfs_image_size);                 // 0x418 - size of external PFS image
		klog("%04zX: mount_image_offset:      %IX\n", offsetof(PkgHeader,mount_image_offset),	hdr.mount_image_offset);             // 0x420
		klog("%04zX: mount_image_size:        %IX\n", offsetof(PkgHeader,mount_image_size),		hdr.mount_image_size);               // 0x428
		klog("%04zX: pkg_size:                %IX\n", offsetof(PkgHeader,pkg_size),				hdr.pkg_size);                       // 0x430
		klog("%04zX: pfs_signed_size:          %X\n", offsetof(PkgHeader,pfs_signed_size),		hdr.pfs_signed_size);                // 0x438
		klog("%04zX: pfs_cache_size:           %X\n", offsetof(PkgHeader,pfs_cache_size),		hdr.pfs_cache_size);                 // 0x43C
		klog("%04zX: pfs_split_size_nth_0:    %IX\n", offsetof(PkgHeader,pfs_split_size_nth_0), hdr.pfs_split_size_nth_0);           // 0x480
		klog("%04zX: pfs_split_size_nth_1:    %IX\n", offsetof(PkgHeader,pfs_split_size_nth_1), hdr.pfs_split_size_nth_1);           // 0x488


	//	klog("%04zX: offsTest \n", offsetof(PkgHeader, seed));
	}


private:



	INLINE void byteSwapHdr() {
		hdr.pkg_magic				= bswap32(hdr.pkg_magic);
		hdr.pkg_type				= bswap32(hdr.pkg_type);
		hdr.pkg_0x008				= bswap32(hdr.pkg_0x008);
		hdr.pkg_file_count			= bswap32(hdr.pkg_file_count);
		hdr.pkg_entry_count			= bswap32(hdr.pkg_entry_count);
		hdr.pkg_sc_entry_count		= bswap16(hdr.pkg_sc_entry_count);
		hdr.pkg_entry_count_2		= bswap16(hdr.pkg_entry_count_2);
		hdr.pkg_entry_offset		= bswap32(hdr.pkg_entry_offset);
		hdr.pkg_entry_data_size		= bswap32(hdr.pkg_entry_data_size);
		hdr.pkg_body_offset			= bswap64(hdr.pkg_body_offset);
		hdr.pkg_body_size			= bswap64(hdr.pkg_body_size);
		hdr.pkg_content_offset		= bswap64(hdr.pkg_content_offset);
		hdr.pkg_content_size		= bswap64(hdr.pkg_content_size);

		hdr.pkg_drm_type			= bswap32(hdr.pkg_drm_type);
		hdr.pkg_content_type		= bswap32(hdr.pkg_content_type);
		hdr.pkg_content_flags		= bswap32(hdr.pkg_content_flags);
		hdr.pkg_promote_size		= bswap32(hdr.pkg_promote_size);
		hdr.pkg_version_date		= bswap32(hdr.pkg_version_date);
		hdr.pkg_version_hash		= bswap32(hdr.pkg_version_hash);
		hdr.pkg_0x088				= bswap32(hdr.pkg_0x088);
		hdr.pkg_0x08C				= bswap32(hdr.pkg_0x08C);
		hdr.pkg_0x090				= bswap32(hdr.pkg_0x090);
		hdr.pkg_0x094				= bswap32(hdr.pkg_0x094);
		hdr.pkg_iro_tag				= bswap32(hdr.pkg_iro_tag);
		hdr.pkg_drm_type_version	= bswap32(hdr.pkg_drm_type_version);

		hdr.pfs_image_count			= bswap32(hdr.pfs_image_count);
		hdr.pfs_image_flags			= bswap64(hdr.pfs_image_flags);
		hdr.pfs_image_offset		= bswap64(hdr.pfs_image_offset);
		hdr.pfs_image_size			= bswap64(hdr.pfs_image_size);
		hdr.mount_image_offset		= bswap64(hdr.mount_image_offset);
		hdr.mount_image_size		= bswap64(hdr.mount_image_size);
		hdr.pkg_size				= bswap64(hdr.pkg_size);
		hdr.pfs_signed_size			= bswap32(hdr.pfs_signed_size);
		hdr.pfs_cache_size			= bswap32(hdr.pfs_cache_size);
		hdr.pfs_split_size_nth_0	= bswap64(hdr.pfs_split_size_nth_0);
		hdr.pfs_split_size_nth_1	= bswap64(hdr.pfs_split_size_nth_1);
	}

	inline static void byteSwapEntry(PkgEntry& e) {
		e.id		= bswap32(e.id);
		e.size		= bswap32(e.size);
		e.flags1	= bswap32(e.flags1);
		e.flags2	= bswap32(e.flags2);
		e.nameOffs	= bswap32(e.nameOffs);
		e.dataOffs	= bswap32(e.dataOffs);
	}

	
	const string entryName(EntryId eid) {
		switch (eid) {
#define _EN(_N_) case _N_: return string(#_N_)
		_EN(Digest);
		_EN(EntryKey);
		_EN(ImageKey);
		_EN(GenDigest);
		_EN(Meta);
		_EN(EntryName);

		_EN(LicenseDat);
		_EN(LicenseInfo);
		_EN(NpTitle);
		_EN(NpBind);
		_EN(SelfInfo);
		_EN(ImageInfo);
		_EN(TargetDelta);
		_EN(OriginDelta);
		_EN(PSReserved);

		_EN(ParamSFO);
		_EN(PlayGoDat);
		_EN(PlayGoHash);
		_EN(PlayGoManif);
		_EN(PronunXml);
		_EN(PronunSig);
		_EN(Pic1_Png);
		_EN(PubToolInfo);
		_EN(AppPGoDat);
		_EN(AppPGoHash);
		_EN(AppPGoManif);
		_EN(ShareParam);
		_EN(ShareImage);
		_EN(SaveDataPng);
		_EN(SharePrivPng);
#undef  _EN
		default: break;
		}
		return string("Entry<Unknown>");
	}
	

};








