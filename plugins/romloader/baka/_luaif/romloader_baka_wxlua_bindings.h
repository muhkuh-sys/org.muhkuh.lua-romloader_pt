// ---------------------------------------------------------------------------
// romloader_baka_lua.h - headers and tags for wxLua binding
//
// This file was generated by genwxbind.lua 
// Any changes made to this file may be lost when file is regenerated
// ---------------------------------------------------------------------------

#ifndef __HOOK_WXLUA_romloader_baka_lua_H__
#define __HOOK_WXLUA_romloader_baka_lua_H__


#include "wxlua/include/wxlstate.h"
#include "wxlua/include/wxlbind.h"

// ---------------------------------------------------------------------------
// Check if the version of binding generator used to create this is older than
//   the current version of the bindings.
//   See 'bindings/genwxbind.lua' and 'modules/wxlua/include/wxldefs.h'
#if WXLUA_BINDING_VERSION > 18
#   error "The WXLUA_BINDING_VERSION in the bindings is too old, regenerate bindings."
#endif //WXLUA_BINDING_VERSION > 18
// ---------------------------------------------------------------------------

// binding class
class wxLuaBinding_romloader_baka_lua : public wxLuaBinding
{
public:
    wxLuaBinding_romloader_baka_lua();
    virtual wxLuaBinding* Clone() const { return new wxLuaBinding_romloader_baka_lua; }

protected:
    virtual void PreRegister(const wxLuaState& wxlState, int luaTable);
    virtual void PostRegister(const wxLuaState& wxlState, int luaTable);

    DECLARE_DYNAMIC_CLASS(wxLuaBinding_romloader_baka_lua)
};


// bind wxLuaBinding_romloader_baka_lua to a single wxLuaState
extern WXLUA_NO_DLLIMPEXP bool wxLuaBinding_romloader_baka_lua_bind(const wxLuaState& wxlState);
// initialize wxLuaBinding_romloader_baka_lua for all wxLuaStates
extern WXLUA_NO_DLLIMPEXP bool wxLuaBinding_romloader_baka_lua_init();

// ---------------------------------------------------------------------------
// Includes
// ---------------------------------------------------------------------------

#include "../../romloader.h"
#include "../romloader_baka_main.h"


// ---------------------------------------------------------------------------
// Lua Tag Method Values and Tables for each Class
// ---------------------------------------------------------------------------

extern WXLUA_NO_DLLIMPEXP_DATA(int) s_wxluatag_romloader;
extern WXLUA_NO_DLLIMPEXP wxLuaBindMethod romloader_methods[];
extern WXLUA_NO_DLLIMPEXP_DATA(int) romloader_methodCount;



// ---------------------------------------------------------------------------
// Encapsulation Declarations - need to be public for other bindings.
// ---------------------------------------------------------------------------


#endif // __HOOK_WXLUA_romloader_baka_lua_H__

