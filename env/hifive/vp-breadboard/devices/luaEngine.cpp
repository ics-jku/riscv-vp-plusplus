/*
 * scriptloader.cpp
 *
 *  Created on: Apr 29, 2022
 *      Author: pp
 */


#include "luaEngine.hpp"

#include <filesystem>
#include <exception>
#include <QDirIterator>

extern "C"
{
#if __has_include(<lua5.3/lua.h>)
	#include <lua5.3/lua.h>
	#include <lua5.3/lualib.h>
	#include <lua5.3/lauxlib.h>
#elif  __has_include(<lua.h>)
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
#else
	#error("No lua libraries found")
#endif
}

#include <LuaBridge/LuaBridge.h>



using namespace std;
using namespace luabridge;
using std::filesystem::directory_iterator;


static lua_State* L;

/**
 * @return [false, ...] if invalid
 */
// TODO: LoadSCript from c array, to reduce duplicate code between QT-Ressource and actual file
LuaRef loadScriptFromFile(lua_State* L, filesystem::path p) {
	LuaRef scriptloader = getGlobal(L, "scriptloader");
	try {
		LuaResult r = scriptloader(p.c_str());
		if(!r.wasOk()) {
			cerr << p << ": " << r.errorMessage() << endl;
			return LuaRef(L);
		}
		if(r.size() != 1) {
			//cerr << p << " failed." << endl;
			return LuaRef(L);
		}
		if(!r[0].isTable()) {
			cerr << p << ": " << r[0] << endl;
			return LuaRef(L);
		}
		return r[0];

	} catch(LuaException& e)	{
		cerr << "serious shit got down in file " << p << endl;
		cerr << e.what() << endl;
		return LuaRef(L);
	}
}

LuaEngine::LuaEngine(){
	L = luaL_newstate();
	luaL_openlibs(L);

	QFile loader(scriptloader.c_str());
	if (!loader.open(QIODevice::ReadOnly)) {
		throw(runtime_error("Could not open config file " + scriptloader));
	}
	QByteArray loader_content = loader.readAll();
	// TODO: If offering c-functions, do this here


	if( luaL_dostring( L, loader_content.data()) )
	{
		cerr << "Error loading loadscript:\n" <<
				 lua_tostring( L, lua_gettop( L ) ) << endl;
		lua_pop( L, 1 );
		throw(runtime_error("Loadscript not valid"));
	}
}
