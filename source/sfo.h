#pragma once

struct SfoHeader {
	u32 magic;
	u32 version;
	u32 keyTabOffs;
	u32 dataTabOffs;
	u32 entryCount;
};

struct SfoEntry {
	u16 keyOffs;
	u16 paramFmt;
	u32 paramLen;
	u32 paramMax;
	u32 dataOffs;
};


enum FmtParam : u16 {
	Fmt_StrS	= 0x004,
	Fmt_Utf8	= 0x204,
	Fmt_SInt32	= 0x404,

	Fmt_Invalid = 0x000
};


class SfoReader
{
	u64 size=0;
	const u8* data=nullptr;

	SfoHeader* hdr=nullptr;
	SfoEntry* entries=nullptr;


	static bool isNum (SfoEntry* e) { return (e &&  e->paramFmt==Fmt_SInt32); }
	static bool isText(SfoEntry* e) { return (e && (e->paramFmt==Fmt_StrS || Fmt_Utf8==e->paramFmt)); }

public:

	SfoReader()
	{	}

	void setData(const vector<u8>& sfoData)
	{
		if (sfoData.size()) {
			data=&sfoData[0];
			size=sfoData.size();

			hdr = (SfoHeader*)data;
			entries = (SfoEntry*)(data + sizeof(SfoHeader));
		}
	}



	template<typename T>
	inline T operator[](SfoEntry* e)
	{
		T rv;
		vector<u8> eData;
		if(getEntryData(e,eData)) {
			rv=*(T*)&eData[0];
		}
		return rv;
	}

	// apparently doesn't work 
	template<> inline string operator[](SfoEntry* e)
	{
		string s;
		vector<u8> eData;
		if(getEntryData(e,eData)) {
			s=string((char*)&eData[0]);
		}
		return s;
	}


	inline SfoEntry* _ent(u32 index)	//  operator[]
	{
		if (hdr && entries && index < hdr->entryCount && size > (sizeof(SfoHeader)+sizeof(SfoEntry)*index))
			return &entries[index];

		return nullptr;
	}

#if 0
	template<typename T> T operator[](const string key)
	{
		for (u32 i=0; i<hdr->entryCount; i++) {
			SfoEntry *e = &entries[i];
			string k;
			if (getEntryKey(e, k) && k==key) {
				return (T)this->operator[](e);
			}
		}
		return T();
	}
	template<typename T> T operator[](const char* key)
	{
		return this->operator[](string(key));
	}
#endif

	template<typename T> T GetValueFor(const string key)
	{
		for (u32 i=0; i<hdr->entryCount; i++) {
			SfoEntry *e = _ent(i); // &entries[i];
			string k;
			if (e && getEntryKey(e, k) && k==key) {
				return (T)this->operator[]<T>(e);
			}
		}
		return T();
	}

	template<> u64 GetValueFor(const string key)
	{
		for (u32 i=0; i<hdr->entryCount; i++) {
			SfoEntry *e = _ent(i); // &entries[i];
			string k;
			if (e && getEntryKey(e, k) && k==key) {
				return GetValue(e);
			}
		}
		return ~0;
	}

	// yet same specialization works fine here 
	template<> string GetValueFor(const string key)
	{
		for (u32 i=0; i<hdr->entryCount; i++) {
			SfoEntry *e = _ent(i); // &entries[i];
			string k;
			if (e && getEntryKey(e, k) && k==key) {
				return GetString(e);
			}
}
		return string();
	}
#if 0
	template<typename T> T GetValueFor(const char* key)
	{
		return GetValueFor<string>(string(key));
	}
#endif



	inline bool getEntryKey(SfoEntry* e, string& keyStr)
	{
		unat fileOffs = hdr->keyTabOffs + e->keyOffs;

		keyStr = "INVALID";
		if (fileOffs < size) {	// ParamLen2 ?
			keyStr = string((const char*)(data + fileOffs));
			return true;
		}
		return false;
	}
	inline bool getEntryKey(u32 index, string& keyStr)
	{
		SfoEntry *e = _ent(index);	//this->operator[](index);
		return e && getEntryKey(e, keyStr);
	}

	inline bool getEntryData(SfoEntry* e, vector<u8>& entData)
	{
		entData.clear();
		u64 fileOffs = hdr->dataTabOffs + e->dataOffs;
		if (fileOffs + e->paramLen < size) {
			entData.resize(e->paramLen);
			memcpy(&entData[0], data + fileOffs, e->paramLen);
			return true;
		}
		return false;
	}
	inline bool getEntryData(u32 index, vector<u8>& data)
	{
		SfoEntry *e = _ent(index);	//this->operator[](index);
		return e && getEntryData(e, data);
	}
	
	string GetKey(SfoEntry *e, string defVal=string())
	{
		string s(defVal);

		if(!getEntryKey(e,s)) {
			klog("GetKey() failed!\n");
		}
		return s;
	}

	string GetString(SfoEntry *e, string defVal=string(), bool* res=nullptr)
	{
		if (res) *res=false;
		string s(defVal);

		if (e && isText(e)) {
			vector<u8> eData;
			if(getEntryData(e,eData)) {
				s=string((char*)&eData[0]);
				if (res) *res=true;
			}
		}
		return s;
	}
	string GetString(u32 index, string defVal=string(), bool* res=nullptr)
	{
		SfoEntry *e = _ent(index);	assert(e);	// this->operator[](index);
		return GetString(e,defVal,res);
	}

	u32 GetValue(SfoEntry *e, u32 defVal=~0, bool* res=nullptr)
	{
		if (res) *res=false;
		u32 rv=defVal;

		if (e && isNum(e)) {
			vector<u8> eData;
			if(getEntryData(e,eData)) {
				rv=*(u32*)&eData[0];
				if (res) *res=true;
			}
		}
		return rv;
	}

	// T must be integer
	template<typename T> inline T GetValue(SfoEntry *e, T defVal=~0, bool* res=nullptr)
	{
		return T(GetValue(e,defVal,res));
	}

	template<typename T> inline T GetValue(u32 index, T defVal=~0, bool* res=nullptr)
	{
		SfoEntry *e = _ent(index);	assert(e);	// this->operator[](index);
		return T(GetValue(e,defVal,res));
	}



	void printEntries()
	{
		for (u32 i=0; i<hdr->entryCount; i++) {
			SfoEntry *e=&entries[i];
			klog("SfoEntry[%d]: \"%20s\" fmt: 0x%X len: %d ", i, GetKey(e).c_str(), e->paramFmt, e->paramLen);
			if (isNum(e)) klog("0x%08X\n", GetValue(e));	//u32(this->operator[](e)));
			else if (isText(e)) klog("\"%s\"\n", GetString(e).c_str());
			else klog("ERROR unknown fmt?\n");
		}
	}



};

































