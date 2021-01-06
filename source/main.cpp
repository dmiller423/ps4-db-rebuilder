/*
*	ps4-db-rebuilder 
*/
#include "base.h"
#include "pkg.h"
#include "kernel_utils.h"

using StringList = vector<string>;


#define _LARGE_FILE       1
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1

#define SQLITE_OMIT_TRACE 1
#define SQLITE_OMIT_LOAD_EXTENSION 1
#include "sqlite/sqlite3.h"
//#include "sqlite/sqlite3ext.h"


inline static string JoinStr(const StringList& sl, const string& sep)
{
	string res;
	res.reserve(KB(16));

	int i = accumulate(std::begin(sl), std::end(sl), 0,
					[&](int &, const string &s) {
						if (!res.empty()) {
							res.append(sep);
						}
						res.append(s);
						return 0;
					});
	return res;
}



template<typename T>
inline static void punt(vector<T>& vec, T& val)
{
	vec.push_back(val);
}

// even when templates aren't made for this a little evil sourcery will make them work
template<typename T, typename U>
inline static void punt(T& v, U& c)
{
	klog("ERROR template match fail!\n");
#ifndef _WIN32
	__builtin_trap();	// die
#endif
}


template<typename T>
static bool GetList(sqlite3* db, const string& sql, vector<T>& res)
{
	res.clear();

	bool err=false;
	sqlite3_stmt *stmt=nullptr;
	int rc = sqlite3_prepare_v2(db, sql.c_str(), sql.size()+1, &stmt, nullptr);
	if (SQLITE_OK!=rc) {
		klog("Error, failed to prep stmt rc: 0x%08X (%d)\n", rc, rc);
		return false;
	}

	do {
		rc = sqlite3_step(stmt);
		if (SQLITE_ROW==rc) {
			int colType = sqlite3_column_type(stmt, 0);
			klog("@@ column res type: %d \n", colType);

			// vector<string>
			if (typeid(T)==typeid(string)	&& colType==SQLITE_TEXT) {
				const unsigned char* txtRes = sqlite3_column_text(stmt, 0);
				klog("@@ TEXT column res \"%s\" \n", txtRes);
				string stupid((const char*)txtRes);
				punt(res,stupid);	//res.push_back(string((const char*)txtRes));
			}
			// vector<*{s,u}[8-64]*>	CONVERSION IS IMPLICIT, IF IT DOESNT MATCH ITS YOUR FAULT
			else if (is_integral<T>::value		&& colType==SQLITE_INTEGER) {
				sqlite_int64 iRes = sqlite3_column_int64(stmt, 0);
				klog("@@ INT column res %ld (0x%016lX) \n", iRes, iRes);
				punt(res,iRes);	//res.push_back(iRes);
			}
			// vector<float,double>		^ also, conversion from int TO float/double , not the reverse atm
			else if (is_arithmetic<T>::value	&& (colType==SQLITE_FLOAT || colType==SQLITE_INTEGER)) {
				double dRes = sqlite3_column_double(stmt, 0);
				klog("@@ FLT column res %f \n", dRes);
				punt(res,dRes);	//res.push_back(dRes);
			}
			else if (is_same<T, vector<u8>& >::value && colType==SQLITE_BLOB) {
				//sqlite3_column_blob(stmt, 0);
				klog("@@ BLOB column type! *FIXME* (test) not needed for db-rebuilder \n");
				//memcpy(&res[N][0], ptrBlob, sizeBlob)
			}
			else {
				klog("GetList() mismatch on colType %d\n", colType);
				err=true;
			}

		}
		else if (SQLITE_DONE!=rc) {
			klog("GetList() got rc: %d\n", rc);
			err=true;
		}

	} while(!err && SQLITE_DONE != rc);

	if (err) {
		klog("Error, failed to step stmt rc: 0x%08X (%d)\n", rc, rc);
		return false;
	}


	rc = sqlite3_finalize(stmt);
	if(SQLITE_OK!=rc) {
		klog("Error, failed to finalize stmt rc: 0x%08X (%d)\n", rc, rc);
		return false;
	}

	return true;
}



// u want xtra copy ?
template<typename T>
inline static vector<T> GetList(sqlite3* db, const string& sql)
{
	vector<T> res;
	if(!GetList(db,sql,res)) klog("GetList(\"%s\") returns false\n");
	return res;
}



inline static string pstr(const string& s) {
	return string("\"") +s+ string("\"");
}

bool getEntries(const string& dir, StringList& entries, bool wantDots=false);

int main(int argc, char *argv[])
{
#ifndef _WIN32
	jailbreak(get_fw_version());
#endif
	string ssp = string(80,'-') + "\n";
	klog(ssp.c_str());
	klog("ps4-db-rebuilder\n");
	klog(ssp.c_str());


	StringList tidList;
	if(!getEntries("/user/app/", tidList)) {
		klog("Error: failed to get /user/app dirents!\n");
		return -1;
	}


	sqlite3 *db;
	char *errMsg=nullptr;

#if 0
	sqlite3_temp_directory = "/data/tmp"; //sqlite3_mprintf("%s", zPathBuf);
#endif

	string sql("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'tbl_appbrowse_%';");



#ifdef _WIN32
#define VFS_IMPL "win32"
	const string sqlDbPath("./app.sqlite.db");
#else
#define VFS_IMPL "unix-excl"	// unix[-]{dotfile,excl,none,namedsem}  -- Programmers are encouraged to use only "unix" or "unix-excl" unless there is a compelling reason to do otherwise.
	const string sqlDbPath("/system_data/priv/mms/app.db");
#endif


	bool err=false;
	int rc = sqlite3_open_v2(sqlDbPath.c_str(), &db, SQLITE_OPEN_READWRITE, VFS_IMPL); // 
	if (SQLITE_OK!=rc) {
		klog("Error, failed to open db \"%s\" \n", sqlDbPath.c_str());
		goto _finish;
	}





	// scope so goto will stfu
	{
		string tabName;

		vector<string> sqlRes;
		if(!GetList<string>(db, sql, sqlRes))
			klog("Error, query 1 failed  \"%s\" \n", sqlite3_errmsg(db));
		else {
			tabName=sqlRes[0];
		}

		if (tabName.empty()) {
			klog("ERROR, sorry bout ur luck,  didn't get table name ;die;\n");
			goto _close;
		}


		sql = string("SELECT '") + JoinStr(tidList, "' AS titleid UNION SELECT '") + string("' AS titleid");
		sql = string("SELECT T.titleid FROM (") + sql + string(") T WHERE T.titleid NOT IN (SELECT titleid FROM ") + tabName + string(");");

		
		bool magicCookie=true;	// nRows > (NROWS_POR505)

		sqlRes.clear();
		if(!GetList<string>(db, sql, sqlRes))
			klog("Error, query 2 failed  \"%s\" \n", sqlite3_errmsg(db));
		else
		{

			for (const string& ctid : sqlRes) {
				string tid=ctid, cid=ctid, title=ctid;
				u64 tSize=GB<u64>(50);

				string pkgPath = 
#if _WIN32
					string(".")
#else
					string("/user/appmeta/")+ctid
#endif
					+ "/app.pkg" ;

				Pkg pkg(pkgPath);
				if (!pkg.isValid()) {
					klog("Error, failed to get info from pkg, skipping... \"%s\"\n", pkgPath.c_str());
					continue;
				}

				tSize = pkg.pkgSize;
				cid   = pkg.sfo.GetValueFor<string>("CONTENT_ID");
				tid   = pkg.sfo.GetValueFor<string>("TITLE_ID");
				title = pkg.sfo.GetValueFor<string>("TITLE");
#if 1 //_DEBUG
				if (tid != ctid) {
					klog("Error, tids don't match \"%s\" vs \"%s\" \n", tid.c_str(), ctid.c_str());
					continue;
				}
#endif
				string szs(64,'\0');
				sprintf(&szs[0], "%zd", tSize);
				szs=string(&szs[0]);

				sql = string("INSERT INTO ") + tabName + " VALUES (" + pstr(tid)+", "+pstr(cid)+", "+pstr(title)
					+", "+pstr(string("/user/appmeta/")+tid)+", "+pstr("2018-07-27 15:06:46.822")
					+string(", \"0\", \"0\", \"5\", \"1\", \"100\", \"0\", \"151\", \"5\", \"1\", \"gd\", \"0\", \"0\", \"0\", \"0\"")
					+string(", NULL, NULL, NULL, ") + pstr(szs)
					+string(", \"2018-07-27 15:06:46.802\", \"0\", \"game\", NULL, \"0\", \"0\", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, \"0\", NULL, NULL, NULL, NULL, NULL, \"0\", \"0\", NULL, \"2018-07-27 15:06:46.757\", ")
					+(magicCookie ? string("0,0,0,0,0,0") : string(""))
					+string(");")
				;

				StringList sqlRes2;
				if (!GetList<string>(db, sql, sqlRes2)) {
					klog("Error, query 3 failed  \"%s\" \n", sqlite3_errmsg(db));
				}

			}
		}



		sql = string("SELECT DISTINCT T.titleid FROM (SELECT titleid FROM '") + JoinStr(StringList{tabName}, "' UNION SELECT titleid FROM '")
		+string("') T WHERE T.titleid NOT IN (SELECT DISTINCT titleid FROM tbl_appinfo);");

		sqlRes.clear();
		if(!GetList<string>(db, sql, sqlRes))
			klog("Error, query 4 failed  \"%s\" \n", sqlite3_errmsg(db));
		else {
			for (const string& ctid : sqlRes) {

				string pkgPath = 
#if _WIN32
					string(".")
#else
					string("/user/appmeta/")+ctid
#endif
					+ "/app.pkg" ;

				Pkg pkg(pkgPath);
				if (!pkg.isValid()) {
					klog("Error, failed to get info from pkg, skipping... \"%s\"\n", pkgPath.c_str());
					continue;
				}

			std::map<string, string> appMap {

				{ "#_access_index",				"67" },
				{ "#_last_access_time",			"2018-07-27 15:04:39.822" },
				{ "#_contents_status",			"0" },
				{ "#_mtime",					"2018-07-27 15:04:40.635" },
				{ "#_update_index",				"74" },
				{ "#exit_type",					"0" },
				{ "ATTRIBUTE_INTERNAL",			"0" },
				{ "DISP_LOCATION_1",			"0" },
				{ "DISP_LOCATION_2",			"0" },
				{ "DOWNLOAD_DATA_SIZE",			"0" },
				{ "FORMAT",						"obs" },
				{ "PARENTAL_LEVEL",				"1" },
				{ "SERVICE_ID_ADDCONT_ADD_1",	"0" },
				{ "SERVICE_ID_ADDCONT_ADD_2",	"0" },
				{ "SERVICE_ID_ADDCONT_ADD_3",	"0" },
				{ "SERVICE_ID_ADDCONT_ADD_4",	"0" },
				{ "SERVICE_ID_ADDCONT_ADD_5",	"0" },
				{ "SERVICE_ID_ADDCONT_ADD_6",	"0" },
				{ "SERVICE_ID_ADDCONT_ADD_7",	"0" },
				{ "SYSTEM_VER",					"33751040" },
				{ "USER_DEFINED_PARAM_1",		"0" },
				{ "USER_DEFINED_PARAM_2",		"0" },
				{ "USER_DEFINED_PARAM_3",		"0" },
				{ "USER_DEFINED_PARAM_4",		"0" },
				{ "_contents_ext_type",			"0" },
				{ "_contents_location",			"0" },
				{ "_current_slot",				"0" },
				{ "_disable_live_detail",		"0" },
				{ "_hdd_location",				"0" },
				{ "_path_info",					"3113537756987392" },
				{ "_path_info_2",				"0" },
				{ "_size_other_hdd",			"0" },
				{ "_sort_priority",				"100" },
				{ "_uninstallable",				"1" },
				{ "_view_category",				"0" },
				{ "_working_status",			"0" }
			};

				appMap["APP_TYPE"]	= pkg.sfo.GetValueFor<string>("APP_TYPE");
				appMap["APP_VER"]	= pkg.sfo.GetValueFor<string>("APP_VER");
				appMap["CATEGORY"]	= pkg.sfo.GetValueFor<string>("CATEGORY");
				appMap["CONTENT_ID"]= pkg.sfo.GetValueFor<string>("CONTENT_ID");
				appMap["TITLE"]		= pkg.sfo.GetValueFor<string>("TITLE");
				appMap["TITLE_ID"]	= pkg.sfo.GetValueFor<string>("TITLE_ID");
				appMap["VERSION"]	= pkg.sfo.GetValueFor<string>("VERSION");

				appMap["_org_path"]			=	string("/user/app/") + ctid;
				appMap["_metadata_path"]	=	string("/user/appmeta/") + ctid;


				string szs(64,'\0');
				sprintf(&szs[0], "%zd", pkg.pkgSize);
				szs=string(&szs[0]);

				appMap["#_size"] = szs;


				u64 attr = pkg.sfo.GetValueFor<u64>("ATTRIBUTE");	// *INT

				string sattr(64,'\0');
				sprintf(&sattr[0], "%zd", attr);
				sattr=string(&sattr[0]);

				appMap["ATTRIBUTE"] = sattr;


#if __cplusplus > 201700
				for (const auto& [key, value] : appMap) {
#else
				map<string, string>::iterator it;
				for (it = appMap.begin(); it != appMap.end(); it++)
				{
					const auto& key = it->first;
					const auto& value = it->second;
#endif
				//	"INSERT INTO tbl_appinfo (titleid, key, val) VALUES (?, ?, ?);", [game_id, key, value]
					sql = string("INSERT INTO tbl_appinfo (titleid, key, val) VALUES (")
						+pstr(ctid)+", "+pstr(key)+", "+pstr(value) + string(");");

#if 1
					klog(ssp.c_str());
					klog("appinfo insert query:\n");
					klog(sql.c_str()); klog("\n");
					klog(ssp.c_str());
#endif
					StringList sqlRes2;
					if(!GetList<string>(db, sql, sqlRes2))
						klog("Error, query 5 failed  \"%s\" \n", sqlite3_errmsg(db));
				}
			}

		}


	}



_close:

	sqlite3_close(db);


_finish:
	klog(ssp.c_str());
	klog("finished!\n");
	klog(ssp.c_str());

#ifndef _WIN32
	while(1) { usleep(100); }
#endif
	return 0;
}





#ifndef _WIN32

void klog(const char *format, ...)
{
	char buff[1024];
	memset(buff, 0, 1024);

	va_list args;
	va_start(args, format);
	vsprintf(buff, format, args);
	va_end(args);

	int fd = open("/dev/ttyu0", 1 /*O_WRONLY*/, 0600);             // /dev/klog? O_DIRECT | Open the device with read/write access
	if (fd < 0) {
		perror("Failed to open the device...");        // idk if we have perror, doesn't matter we'll find out 
		return;
	}
	char* t = buff;
	while (0 != *t) write(fd, t++, 1);
	close(fd);
}

bool getEntries(const string& dir, StringList& entries, bool wantDots)
{
	static vector<char> dentBuff(0x10000,0);
	entries.clear();

	if (dir.empty())
		return false;

#if _DEBUG
	klog("%s(\"%s\") \n", __func__, dir.c_str());
#endif
	int dfd = open(dir.c_str(), O_RDONLY | O_DIRECTORY);
	if (dfd > 0) {

		dentBuff.resize(0x10000);	// 64k ought to be enough, really... 

		int dsize = 0;
		while (0 < (dsize = sceKernelGetdents(dfd, &dentBuff[0], dentBuff.size())))
		{
			int offs = 0;
			dirent *de = (dirent *)&dentBuff[offs];

			while (offs < dsize && de) {
				de = (dirent *)&dentBuff[offs];
				string dName(de->d_name);
				if(string(".")==dName || string("..")==dName) {
					if(wantDots)
						entries.push_back(de->d_name);
				}
				else {
					entries.push_back(de->d_name);
#if _DEBUG
					u32 mode = _mode(string(dir + "/" + de->d_name).c_str());
					klog("entry fileNo: 0x%08X , mode: %jo , name: \"%s\" \n", de->d_fileno, mode, de->d_name);
#endif
				}
				offs += de->d_reclen;
			}
		}

		close(dfd);
		return true;
	}

	return false;
}



#endif
