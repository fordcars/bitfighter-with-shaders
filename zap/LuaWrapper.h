// NOTE: This file includes edits from https://bitbucket.org/alexames/luawrapper/commits
//       through 3c54015

/*
 * Copyright (c) 2010-2013 Alexander Ames
 * Alexander.Ames@gmail.com
 * See Copyright Notice at the end of this file
 */

// API Summary:
//
// LuaWrapper is a library designed to help bridge the gab between Lua and
// C++. It is designed to be small (a single header file), simple, fast,
// and typesafe. It has no external dependencies, and does not need to be
// precompiled; the header can simply be dropped into a project and used
// immediately. It even supports class inheritance to a certain degree. Objects
// can be created in either Lua or C++, and passed back and forth.
//
// The main functions of interest are the following:
//  luaW_is<T>
//  luaW_to<T>
//  luaW_check<T>
//  luaW_push<T>
//  luaW_register<T>
//  luaW_setfuncs<T>
//  luaW_extend<T, U>
//  luaW_hold<T>
//  luaW_release<T>
//
// These functions allow you to manipulate arbitrary classes just like you
// would the primitive types (e.g. numbers or strings). If you are familiar
// with the normal Lua API the behavior of these functions should be very
// intuative.
//
// For more information see the README and the comments below

#ifndef LUA_WRAPPER_H_
#define LUA_WRAPPER_H_

// If you are linking against Lua compiled in C++, define LUAW_NO_EXTERN_C
#ifndef LUAW_NO_EXTERN_C
extern "C"
{
#endif // LUAW_NO_EXTERN_C

#  include <lua.h>
#  include <lauxlib.h>

#ifndef LUAW_NO_EXTERN_C
}
#endif // LUAW_NO_EXTERN_C

#include "LuaBase.h"   
#include "LuaException.h"   

#include <string>
#include <vector>
#include <map>
#include <cstring>

using namespace Zap;


#define LUAW_POSTCTOR_KEY  "__postctor"
#define LUAW_EXTENDS_KEY   "__extends"
#define LUAW_STORAGE_KEY   "storage"
#define LUAW_CACHE_KEY     "cache"
#define LUAW_CACHE_METATABLE_KEY "cachemetatable"
#define LUAW_HOLDS_KEY     "holds"
#define LUAW_WRAPPER_KEY   "LuaWrapper"
#define LUAW_USING_PROXY_KEY "usingproxy"
#define LUAW_USING_PROXY_METATABLE_KEY "usingproxymetatable"

// A simple utility function to adjust a given index
// Useful for when a parameter index needs to be adjusted
// after pushing or popping things off the stack
inline int luaW_correctindex(lua_State* L, int index, int correction)
{
    return index < 0 ? index - correction : index;
}


// Forward declaration
template <class T> class LuaProxy;


// Here we will specify whether to use our proxy system for objects managed in LuaW
// or use (mostly) upstream behavior
inline bool luaW_shouldCreateProxy(lua_State* L)
{
   // We don't want proxies with editor plugins because they end up creating dangling
   // pointers to LuaProxy objects (when copying the GridDatabse in undo/redo)
   return getScriptContext(L) != PluginContext;
}


// These are the default allocator and deallocator. If you would prefer an
// alternative option, you may select a different function when registering
// your class.
template <typename T>
T* luaW_defaultallocator(lua_State *L)
{
    return new T(L);
}
template <typename T>
void luaW_defaultdeallocator(lua_State*, T* obj)
{
    delete obj;
}

// The identifier function is responsible for pushing a value unique to each
// object on to the stack. Most of the time, this can simply be the address
// of the pointer, but sometimes that is not adaquate. For example, if you
// are using shared_ptr you would need to push the address of the object the
// shared_ptr represents, rather than the address of the shared_ptr itself.
template <typename T>
void luaW_defaultidentifier(lua_State* L, T* obj)
{
    lua_pushlightuserdata(L, obj);
}

// This class is what is used by LuaWrapper to contain the userdata. data
// stores a pointer to the object itself, and cast is used to cast toward the
// base class if there is one and it is necessary. Rather than use RTTI and
// typid to compare types, I use the clever trick of using the cast to compare
// types. Because there is at most one cast per type, I can use it to identify
// when and object is the type I want. This is only used internally.
struct luaW_Userdata
{
    luaW_Userdata(void* vptr = NULL, luaW_Userdata (*udcast)(const luaW_Userdata&) = NULL)
        : data(vptr), cast(udcast) {}
    void* data;
    luaW_Userdata (*cast)(const luaW_Userdata&);
};

// This class cannot actually to be instantiated. It is used only hold the
// table name and other information.
template <typename T>
class LuaWrapper
{
public:
    static const char* classname;
    static void (*identifier)(lua_State*, T*);
    static T* (*allocator)(lua_State*);
    static void (*deallocator)(lua_State*, T*);
    static luaW_Userdata (*cast)(const luaW_Userdata&);
private:
    LuaWrapper();
};
template <typename T> const char* LuaWrapper<T>::classname;
template <typename T> void (*LuaWrapper<T>::identifier)(lua_State*, T*);
template <typename T> T* (*LuaWrapper<T>::allocator)(lua_State*);
template <typename T> void (*LuaWrapper<T>::deallocator)(lua_State*, T*);
template <typename T> luaW_Userdata (*LuaWrapper<T>::cast)(const luaW_Userdata&);

// Cast from an object of type T to an object of type U. This template
// function is instantiated by calling luaW_extend<T, U>(L). This is only used
// internally.
template <typename T, typename U>
luaW_Userdata luaW_cast(const luaW_Userdata& obj)
{
    return luaW_Userdata(static_cast<U*>(static_cast<T*>(obj.data)), LuaWrapper<U>::cast);
}

template <typename T, typename U>
void luaW_identify(lua_State* L, T* obj)
{
    LuaWrapper<U>::identifier(L, static_cast<U*>(obj));
}

// Get a field from the LuaWrapper table, put it on top of the stack
template <typename T>
inline void luaW_wrapperfield(lua_State* L, const char* field)
{
    lua_getfield(L, LUA_REGISTRYINDEX, LUAW_WRAPPER_KEY); // ... LuaWrapper
    lua_getfield(L, -1, field);                           // ... LuaWrapper LuaWrapper.field
    lua_getfield(L, -1, LuaWrapper<T>::classname); // ... LuaWrapper LuaWrapper.field LuaWrapper.field.class
    lua_replace(L, -3); // ... LuaWrapper.field.class LuaWrapper.field
    lua_pop(L, 1); // ... LuaWrapper.field.class
}

// Analogous to lua_is(boolean|string|*)
//
// Returns 1 if the value at the given acceptable index is of type T (or if
// strict is false, convertable to type T) and 0 otherwise.
//
// Stack should have two elements: a userdata and a function name (e.g. -- <userdata> 'getRad')
template <typename T>
bool luaW_is(lua_State *L, int index, bool strict = false)
{
    bool equal = false;// lua_isnil(L, index);
    if (!equal && lua_isuserdata(L, index) && lua_getmetatable(L, index))
    {
        // ... ud ... udmt
        luaL_getmetatable(L, LuaWrapper<T>::classname); // ... ud ... udmt Tmt
        equal = lua_rawequal(L, -1, -2) != 0;
        if (!equal && !strict)
        {
            lua_getfield(L, -2, LUAW_EXTENDS_KEY); // ... ud ... udmt Tmt udmt.extends
            for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
            {
                // ... ud ... udmt Tmt udmt.extends k v
                equal = lua_rawequal(L, -1, -4) != 0;
                if (equal)
                {
                    lua_pop(L, 2); // ... ud ... udmt Tmt udmt.extends
                    break;
                }
            }
            lua_pop(L, 1); // ... ud ... udmt Tmt
        }
        lua_pop(L, 2); // ... ud ...
    }
    return equal;
}


// Check to see if an object is using a proxy or not.  The luaW_userdata is passed in here
// and is looked up in the usingproxy table on whether or not it has a proxy associated
template <typename T>
inline bool luaW_isUsingProxy(lua_State* L, void *objptr)
{
   luaW_wrapperfield<T>(L, LUAW_USING_PROXY_KEY);     // -- ... usingproxy_table
   lua_pushlightuserdata(L, objptr);                  // -- ... usingproxy_table, &luaW_Userdata
   lua_rawget(L, -2);                                 // -- ... usingproxy_table, bool

   TNLAssert(!lua_isnil(L, -1) || dumpStack(L), "Expected non-nil value!");
   bool usingProxy = lua_toboolean(L, -1);            // -- ... usingproxy_table, bool

   lua_pop(L, 2);                                     // -- ...

   return usingProxy;
}


// Set whether or not our object uses a proxy
template <typename T>
void luaW_setUsingProxy(lua_State* L, T* obj, bool usingProxy)
{
   luaW_wrapperfield<T>(L, LUAW_USING_PROXY_KEY);     // -- ... usingproxy_table

   // If we're using a proxy, then use that as the usingproxy table key.  This is because
   // we use the luaW_Userdata to find out if an object is using a proxy.  luaW_Userdata
   // will contain the proxy for proxied object otherwise it contains the object itself
   if(usingProxy)
   {
      LuaProxy<T> *proxy = obj->getLuaProxy();
      lua_pushlightuserdata(L, proxy);                // -- ... usingproxy_table, &proxy
   }
   else
      lua_pushlightuserdata(L, obj);                  // -- ... usingproxy_table, &obj

   lua_pushboolean(L, usingProxy);                    // -- ... usingproxy_table, &obj_or_proxy, bool
   lua_rawset(L, -3);                                 // -- ... usingproxy_table
   lua_pop(L, 1);                                     // -- ...
}


// Here we clean-up references in our usingproxy table
template <typename T>
inline void luaW_clearUsingProxy(lua_State* L, void *objptr)
{
   luaW_wrapperfield<T>(L, LUAW_USING_PROXY_KEY);     // -- ... usingproxy_table
   lua_pushlightuserdata(L, objptr);                  // -- ... usingproxy_table, &luaW_Userdata
   lua_pushnil(L);                                    // -- ... usingproxy_table, &luaW_Userdata, nil

   lua_rawset(L, -3);                                 // -- ... usingproxy_table
   lua_pop(L, 1);                                     // -- ...
}


// Analogous to lua_to(boolean|string|*)
//
// Converts the given acceptable index to a T*. That value must be of (or
// convertable to) type T; otherwise, returns NULL.
template <typename T>
T* luaW_to(lua_State* L, int index, bool strict = false)
{
    if (luaW_is<T>(L, index, strict))
    {
        luaW_Userdata* pud = static_cast<luaW_Userdata*>(lua_touserdata(L, index));
        luaW_Userdata ud;
        while (!strict && LuaWrapper<T>::cast != pud->cast)
        {
            ud = pud->cast(*pud);
            pud = &ud;
        }

        if(luaW_isUsingProxy<T>(L, pud->data))
        {
           LuaProxy<T> *proxy = static_cast<LuaProxy<T> *>(pud->data);

           if(!proxy->isDefunct())
              return proxy->getProxiedObject();
        }
        else
           return static_cast<T*>(pud->data);
    }

    return NULL;
}


// As above, but returns the proxy instead of the object itself
template <typename T>
LuaProxy<T>* luaW_toProxy(lua_State* L, int index, bool strict = false)
{
    if (luaW_is<T>(L, index, strict))
    {
        luaW_Userdata* pud = (luaW_Userdata*)lua_touserdata(L, index);
        luaW_Userdata ud;
        while (!strict && LuaWrapper<T>::cast != pud->cast)
        {
            ud = pud->cast(*pud);
            pud = &ud;
        }
        return static_cast<LuaProxy<T> *>(pud->data);
    }

    return NULL;
}

// Analogous to luaL_check(boolean|string|*)
//
// Converts the given acceptable index to a T*. That value must be of (or
// convertable to) type T; otherwise, an error is raised.
template <typename T>
T* luaW_check(lua_State* L, int index, bool strict = false)
{
    T* obj = NULL;
    if (luaW_is<T>(L, index, strict))
    {
        luaW_Userdata* pud = (luaW_Userdata*)lua_touserdata(L, index);
        luaW_Userdata ud;
        while (!strict && LuaWrapper<T>::cast != pud->cast)
        {
            ud = pud->cast(*pud);
            pud = &ud;
        }

        if(luaW_isUsingProxy<T>(L, pud->data))
        {
           LuaProxy<T> *proxy = static_cast<LuaProxy<T>*>(pud->data);

           if(!proxy->isDefunct())
              obj = proxy->getProxiedObject();
        }
        else
           obj = static_cast<T*>(pud->data);
    }
    else
    {
       const char *msg = lua_pushfstring(L, "%s expected, got %s", LuaWrapper<T>::classname, luaL_typename(L, index));
       luaL_argerror(L, index, msg);
    }
    return obj;
}

template <typename T>
T* luaW_opt(lua_State* L, int index, T* fallback = NULL, bool strict = false)
{
    if (lua_isnil(L, index))
        return fallback;
    else
        return luaW_check<T>(L, index, strict);
}

// Forward declaration
template <typename T>
bool luaW_hold(lua_State* L, T* obj);

// Analogous to lua_push(boolean|string|*)
//
// Pushes a userdata of type T onto the stack. If this object already exists in
// the Lua environment, it will assign the existing storage table to it.
// Otherwise, a new storage table will be created for it.
template <typename T>
void luaW_push(lua_State* L, T* obj)
{
   if(!obj)
   {
      lua_pushnil(L);
      return;
   }

   // Should we be using proxies for our objects?
   if(luaW_shouldCreateProxy(L))
   {
      // Get the object's proxy, or create one if it doesn't yet exist
      LuaProxy<T> *proxy = obj->getLuaProxy();

      if(proxy)         // Retrieve the userdata for this proxy from our cache table
      {
         luaW_wrapperfield<T>(L, LUAW_CACHE_KEY);        // -- cache_table
         LuaWrapper<T>::identifier(L, obj);              // -- cache_table, id

         // lua_gettable pushes onto the stack the value t[k], where t is the value at the given valid
         // index and k is the value at the top of the stack.  Pops k, triggers metamethods.
         // Here: retrieves and pushes cache_table[id]
         lua_gettable(L, -2);                            // -- cache_table, userdata

         TNLAssert(lua_isuserdata(L, -1) ||
               dumpStack(L, "Expect table, userdata") || dumpTable(L, -2, "Cached Userdatas"),
               "Expected userdata!");
         TNLAssert(proxy == luaW_toProxy<T>(L, -1), "Cached object is not the one we expect!");

         // Clean up the stack
         lua_remove(L, -2);                              // -- userdata
      }
      else
      {
         // Create a new proxy
         proxy = new LuaProxy<T>(obj);

         // Add a new entry to our cache table (a weak table; more about those here: http://lua-users.org/wiki/WeakTablesTutorial).
         // Note that from here on down, we'll fall back on the normal LuaW push code, except for the bit at the end where
         // we add it to the table.
         LuaWrapper<T>::identifier(L, obj);                 // -- id
         luaW_wrapperfield<T>(L, LUAW_CACHE_KEY);           // -- id, cache_table

         lua_pushvalue(L, -2);                              // -- id, cache_table, id

         // Create the new luaW_userdata and place it in the cache
         lua_pop(L, 1); // ... id cache
         lua_insert(L, -2); // ... cache id
         luaW_Userdata* ud = static_cast<luaW_Userdata*>(lua_newuserdata(L, sizeof(luaW_Userdata))); // ... cache id obj
         ud->data = proxy;
         ud->cast = LuaWrapper<T>::cast;
         lua_pushvalue(L, -1); // ... cache id obj obj
         lua_insert(L, -4); // ... obj cache id obj
         lua_settable(L, -3); // ... obj cache

         // Set the class metatable on userdata
         luaL_getmetatable(L, LuaWrapper<T>::classname); // ... obj cache mt
         lua_setmetatable(L, -3); // ... obj cache

         // Cleanup
         lua_pop(L, 1); // ... obj
         TNLAssert(lua_isuserdata(L, -1) || dumpStack(L, "Expect userdata"), "Expected userdata!");

         luaW_setUsingProxy(L, obj, true);
         luaW_hold<T>(L, obj);     // Tell luaW to collect the proxy when it's done with it
      }
   }  // useLuaProxy

   // No proxy: Use upstream behavior
   else
   {
      LuaWrapper<T>::identifier(L, obj); // ... id
      luaW_wrapperfield<T>(L, LUAW_CACHE_KEY); // ... id cache
      lua_pushvalue(L, -2); // ... id cache id
      lua_gettable(L, -2); // ... id cache obj
      if (lua_isnil(L, -1))
      {
          // Create the new luaW_userdata and place it in the cache
          lua_pop(L, 1); // ... id cache
          lua_insert(L, -2); // ... cache id
          luaW_Userdata* ud = static_cast<luaW_Userdata*>(lua_newuserdata(L, sizeof(luaW_Userdata))); // ... cache id obj
          ud->data = obj;
          ud->cast = LuaWrapper<T>::cast;
          lua_pushvalue(L, -1); // ... cache id obj obj
          lua_insert(L, -4); // ... obj cache id obj
          lua_settable(L, -3); // ... obj cache

          luaL_getmetatable(L, LuaWrapper<T>::classname); // ... obj cache mt
          lua_setmetatable(L, -3); // ... obj cache

          lua_pop(L, 1); // ... obj

          luaW_setUsingProxy(L, obj, false);
          luaW_hold<T>(L, obj);     // Tell luaW to manage this object
      }
      else
      {
          lua_replace(L, -3); // ... obj cache
          lua_pop(L, 1); // ... obj
      }
   }
}


// Instructs LuaWrapper that it owns the userdata, and can manage its memory.
// When all references to the object are removed, Lua is free to garbage
// collect it and delete the object.
//
// Returns true if luaW_hold took hold of the object, and false if it was
// already held
template <typename T>
bool luaW_hold(lua_State* L, T* obj)
{
    luaW_wrapperfield<T>(L, LUAW_HOLDS_KEY); // ... holds
    LuaWrapper<T>::identifier(L, obj); // ... holds id
    lua_pushvalue(L, -1); // ... holds id id
    lua_gettable(L, -3); // ... holds id hold
    // If it's not held, hold it
    if (!lua_toboolean(L, -1))
    {
        // Apply hold boolean
        lua_pop(L, 1); // ... holds id
        lua_pushboolean(L, true); // ... holds id true
        lua_settable(L, -3); // ... holds
        lua_pop(L, 1); // ...
        return true;
    }
    lua_pop(L, 3); // ...
    return false;
}

// Releases LuaWrapper's hold on an object. This allows the user to remove
// all references to an object in Lua and ensure that Lua will not attempt to
// garbage collect it.
//
// This function takes the index of the identifier for an object rather than
// the object itself. This is because needs to be able to run after the object
// has already been deallocated. A wrapper is provided for when it is more
// convenient to pass in the object directly.
template <typename T>
void luaW_release(lua_State* L, int index)
{
    luaW_wrapperfield<T>(L, LUAW_HOLDS_KEY); // ... id ... holds
    lua_pushvalue(L, luaW_correctindex(L, index, 1)); // ... id ... holds id
    lua_pushnil(L); // ... id ... holds id nil
    lua_settable(L, -3); // ... id ... holds
    lua_pop(L, 1); // ... id ...
}

template <typename T>
void luaW_release(lua_State* L, T* obj)
{
    LuaWrapper<T>::identifier(L, obj); // ... id
    luaW_release<T>(L, -1); // ... id
    lua_pop(L, 1); // ...
}

// This function is called from Lua, not C++
//
// Calls the lua post-constructor (LUAW_POSTCTOR_KEY or "__postctor") on a
// userdata. Assumes the userdata is on top of the stack, and numargs arguments
// are below it. This runs the LUAW_POSTCTOR_KEY function on T's metatable,
// using the object as the first argument and whatever else is below it as
// the rest of the arguments This exists to allow types to adjust values in
// thier storage table, which can not be created until after the constructor is
// called.
template <typename T>
void luaW_postconstructor(lua_State* L, int numargs)
{
   // ... args... ud
   lua_getfield(L, -1, LUAW_POSTCTOR_KEY); // ... args... ud ud.__postctor
   if (lua_type(L, -1) == LUA_TFUNCTION)
   {
      lua_pushvalue(L, -2);        // ... args... ud ud.__postctor ud
      lua_insert(L, -3 - numargs); // ... ud args... ud ud.__postctor
      lua_insert(L, -3 - numargs); // ... ud.__postctor ud args... ud
      lua_insert(L, -3 - numargs); // ... ud ud.__postctor ud args...
      lua_call(L, numargs + 1, 0); // ... ud
   }
   else
   {
      lua_pop(L, 1); // ... ud
   }
}

// This function is generally called from Lua, not C++
//
// Creates an object of type T using the constructor and subsequently calls the
// post-constructor on it.
template <typename T>
inline int luaW_new(lua_State* L, int args)
{
    T* obj = LuaWrapper<T>::allocator(L);
    luaW_push<T>(L, obj);
//    luaW_hold<T>(L, obj);  // luaW_hold is called in luaW_push with our proxy system in place
    luaW_postconstructor<T>(L, args);
    return 1;
}

template <typename T>
int luaW_new(lua_State* L)
{
    return luaW_new<T>(L, lua_gettop(L));
}

// This function is called from Lua, not C++
//
// The default metamethod to call when indexing into lua userdata representing
// an object of type T. This will first check the userdata's environment table
// and if it's not found there it will check the metatable. This is done so
// individual userdata can be treated as a table, and can hold thier own
// values.
template <typename T>
int luaW_index(lua_State* L)
{
    // obj key
    T* obj = luaW_to<T>(L, 1);
    luaW_wrapperfield<T>(L, LUAW_STORAGE_KEY); // obj key storage
    LuaWrapper<T>::identifier(L, obj); // obj key storage id
    lua_gettable(L, -2); // obj key storage store

    // Check if storage table exists
    if (!lua_isnil(L, -1))
    {
        lua_pushvalue(L, -3); // obj key storage store key
        lua_gettable(L, -2); // obj key storage store store[k]
    }

    // If either there is no storage table or the key wasn't found
    // then fall back to the metatable
    if (lua_isnil(L, -1))
    {
        lua_settop(L, 2); // obj key
        lua_getmetatable(L, -2); // obj key mt
        lua_pushvalue(L, -2); // obj key mt k
        lua_gettable(L, -2); // obj key mt mt[k]
    }
    return 1;
}

// This function is called from Lua, not C++
//
// The default metamethod to call when creating a new index on lua userdata
// representing an object of type T. This will index into the the userdata's
// environment table that it keeps for personal storage. This is done so
// individual userdata can be treated as a table, and can hold thier own
// values.
template <typename T>
int luaW_newindex(lua_State* L)
{
    // obj key value
    T* obj = luaW_check<T>(L, 1);
    luaW_wrapperfield<T>(L, LUAW_STORAGE_KEY); // obj key value storage
    LuaWrapper<T>::identifier(L, obj); // obj key value storage id
    lua_pushvalue(L, -1); // obj key value storage id id
    lua_gettable(L, -3); // obj key value storage id store

    // Add the storage table if there isn't one already
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1); // obj key value storage id
        lua_newtable(L); // obj key value storage id store
        lua_pushvalue(L, -1); // obj key value storage id store store
        lua_insert(L, -3); // obj key value storage store id store
        lua_settable(L, -4); // obj key value storage store
    }

    lua_pushvalue(L, 2); // obj key value ... store key
    lua_pushvalue(L, 3); // obj key value ... store key value
    lua_settable(L, -3); // obj key value ... store

    return 0;
}

// This function is called from Lua, not C++
//
// The __gc metamethod handles cleaning up userdata. The userdata's reference
// count is decremented and if this is the final reference to the userdata its
// environment table is nil'd and pointer deleted with the destructor callback.
template <typename T>
int luaW_gc(lua_State* L)
{
   // NOTE
   //
   // We have hijacked the upstream luaW_gc() method to only clean-up proxies. This
   // means that *any* c++ object created through luaW_new() (e.g. calling .new() in
   // a Lua script) MUST be cleaned up in c++ also, otherwise it will leak.
   //
   // Right now all TNL game objects (e.g. Asteroid, TestItem, etc.) are cleaned up
   // automatically at the end of a game.  Other objects, like LuaPlayerInfo, are
   // deleted when its owning ClientInfo object is cleaned up.  This pattern must be
   // followed
   luaW_Userdata* pud = static_cast<luaW_Userdata*>(lua_touserdata(L, 1));

   if(luaW_isUsingProxy<T>(L, pud->data))
   {
      LuaProxy<T>* proxy = luaW_toProxy<T>(L, 1);
      TNLAssert(proxy, "Expected a proxy!");

      if(proxy)
         delete proxy;
   }

   // Else we're not using proxies, handle the object with the upstream code.
   //
   // NOTE
   // The deallocator (delete) is disabled here because we've decided to manage object
   // memory in the c++ classes themselves, not in the LuaW lifecycle.
   else
   {
      // obj
      T* obj = luaW_to<T>(L, 1);
      LuaWrapper<T>::identifier(L, obj); // obj key value storage id
      luaW_wrapperfield<T>(L, LUAW_HOLDS_KEY); // obj id counts count holds
      lua_pushvalue(L, 2); // obj id counts count holds id
      lua_gettable(L, -2); // obj id counts count holds hold
//      if (lua_toboolean(L, -1) && LuaWrapper<T>::deallocator)
//      {
//          LuaWrapper<T>::deallocator(L, obj);
//      }

      luaW_wrapperfield<T>(L, LUAW_STORAGE_KEY); // obj id counts count holds hold storage
      lua_pushvalue(L, 2); // obj id counts count holds hold storage id
      lua_pushnil(L); // obj id counts count holds hold storage id nil
      lua_settable(L, -3); // obj id counts count holds hold storage

      luaW_release<T>(L, 2);
   }

   luaW_clearUsingProxy<T>(L, pud->data);
   return 0;
}

// Takes two tables and registers them with Lua to the table on the top of the
// stack. 
//
// This function is only called from LuaWrapper internally. 
inline void luaW_registerfuncs(lua_State* L, const luaL_Reg defaulttable[], const luaL_Reg table[])
{
    // ... T
#if LUA_VERSION_NUM == 502
    if (defaulttable)
        luaL_setfuncs(L, defaulttable, 0); // ... T
    if (table)
        luaL_setfuncs(L, table, 0); // ... T
#else
    if (defaulttable)
        luaL_register(L, NULL, defaulttable); // ... T
    if (table)
        luaL_register(L, NULL, table); // ... T
#endif
}

// Initializes the LuaWrapper tables used to track internal state. 
//
// This function is only called from LuaWrapper internally. 
inline void luaW_initialize(lua_State* L)
{
    // Ensure that the LuaWrapper table is set up
    lua_getfield(L, LUA_REGISTRYINDEX, LUAW_WRAPPER_KEY); // ... LuaWrapper
    if (lua_isnil(L, -1))
    {
        lua_newtable(L); // ... nil {}
        lua_pushvalue(L, -1); // ... nil {} {}
        lua_setfield(L, LUA_REGISTRYINDEX, LUAW_WRAPPER_KEY); // ... nil LuaWrapper

        // Create a storage table 
        lua_newtable(L); // ... LuaWrapper nil {}
        lua_setfield(L, -2, LUAW_STORAGE_KEY); // ... nil LuaWrapper

        // Create a holds table
        lua_newtable(L); // ... LuaWrapper {}
        lua_setfield(L, -2, LUAW_HOLDS_KEY); // ... nil LuaWrapper
        
        // Create the usingProxy table -- make it a weak table
        lua_newtable(L); // ... nil LuaWrapper {}
        lua_setfield(L, -2, LUAW_USING_PROXY_KEY); // ... nil LuaWrapper

        lua_newtable(L); // ... nil LuaWrapper {}
        lua_pushstring(L, "v"); // ... nil LuaWrapper {} "v"
        lua_setfield(L, -2, "__mode"); // ... nil LuaWrapper {}
        lua_setfield(L, -2, LUAW_USING_PROXY_METATABLE_KEY); // ... nil LuaWrapper

        // Create a cache table, with weak values so that the userdata will not
        // be ref counted
        lua_newtable(L); // ... nil LuaWrapper {}
        lua_setfield(L, -2, LUAW_CACHE_KEY); // ... nil LuaWrapper

        lua_newtable(L); // ... nil LuaWrapper {}
        lua_pushstring(L, "v"); // ... nil LuaWrapper {} "v"
        lua_setfield(L, -2, "__mode"); // ... nil LuaWrapper {}
        lua_setfield(L, -2, LUAW_CACHE_METATABLE_KEY); // ... nil LuaWrapper

        lua_pop(L, 1); // ... nil
    }
    lua_pop(L, 1); // ...
}

// Run luaW_register or luaW_setfuncs to create a table and metatable for your
// class.  These functions create a table with filled with the function from
// the table argument in addition to the functions new and build (This is
// generally for things you think of as static methods in C++). The given
// metatable argument becomes a metatable for each object of your class. These
// can be thought of as member functions or methods.
//
// You may also supply constructors and destructors for classes that do not
// have a default constructor or that require special set up or tear down. You
// may specify NULL as the constructor, which means that you will not be able
// to call the new function on your class table. You will need to manually push
// objects from C++. By default, the default constructor is used to create
// objects and a simple call to delete is used to destroy them.
//
// By default LuaWrapper uses the address of C++ object to identify unique
// objects. In some cases this is not desired, such as in the case of
// shared_ptrs. Two shared_ptrs may themselves have unique locations in memory
// but still represent the same object. For cases like that, you may specify an
// identifier function which is responsible for pushing a key representing your
// object on to the stack.
// 
// luaW_register will set table as the new value of the global of the given
// name. luaW_setfuncs is identical to luaW_register, but it does not set the
// table globally.  As with luaL_register and luaL_setfuncs, both funcstions
// leave the new table on the top of the stack.
// Allocator -> constructor, Deallocator => destructor
template <typename T>
void luaW_setfuncs(lua_State* L, const char* classname, const luaL_Reg* table, 
                   const luaL_Reg* metatable, 
                   T* (*allocator)(lua_State*)         = luaW_defaultallocator<T>, 
                   void (*deallocator)(lua_State*, T*) = luaW_defaultdeallocator<T>, 
                   void (*identifier)(lua_State*, T*)  = luaW_defaultidentifier<T>)
{
    luaW_initialize(L);

    LuaWrapper<T>::classname   = classname;
    LuaWrapper<T>::identifier  = identifier;
    LuaWrapper<T>::allocator   = allocator;
    LuaWrapper<T>::deallocator = deallocator;

    const luaL_Reg defaulttable[] =
    {
        { "new", luaW_new<T> },
        { NULL, NULL }
    };
    const luaL_Reg defaultmetatable[] = 
    { 
        { "__index",    luaW_index<T> }, 
        { "__newindex", luaW_newindex<T> }, 
        { "__gc",       luaW_gc<T> }, 
        { NULL,         NULL } 
    };

    // Set up per-type tables
    lua_getfield(L, LUA_REGISTRYINDEX, LUAW_WRAPPER_KEY); // ... LuaWrapper

    lua_getfield(L, -1, LUAW_STORAGE_KEY); // ... LuaWrapper LuaWrapper.storage
    lua_newtable(L); // ... LuaWrapper LuaWrapper.storage {}
    lua_setfield(L, -2, LuaWrapper<T>::classname); // ... LuaWrapper LuaWrapper.storage
    lua_pop(L, 1); // ... LuaWrapper

    lua_getfield(L, -1, LUAW_HOLDS_KEY); // ... LuaWrapper LuaWrapper.holds
    lua_newtable(L); // ... LuaWrapper LuaWrapper.holds {}
    lua_setfield(L, -2, LuaWrapper<T>::classname); // ... LuaWrapper LuaWrapper.holds
    lua_pop(L, 1); // ... LuaWrapper

    lua_getfield(L, -1, LUAW_CACHE_KEY); // ... LuaWrapper LuaWrapper.cache
    lua_newtable(L); // ... LuaWrapper LuaWrapper.cache {}
    luaW_wrapperfield<T>(L, LUAW_CACHE_METATABLE_KEY); // ... LuaWrapper LuaWrapper.cache {} cmt
    lua_setmetatable(L, -2); // ... LuaWrapper LuaWrapper.cache {}
    lua_setfield(L, -2, LuaWrapper<T>::classname); // ... LuaWrapper LuaWrapper.cache
    lua_pop(L, 1); // ... LuaWrapper

    lua_getfield(L, -1, LUAW_USING_PROXY_KEY); // ... LuaWrapper LuaWrapper.usingproxy
    lua_newtable(L); // ... LuaWrapper LuaWrapper.usingproxy {}
    luaW_wrapperfield<T>(L, LUAW_USING_PROXY_METATABLE_KEY); // ... LuaWrapper LuaWrapper.usingproxy {} upmt
    lua_setmetatable(L, -2); // ... LuaWrapper LuaWrapper.usingproxy {}
    lua_setfield(L, -2, LuaWrapper<T>::classname); // ... LuaWrapper LuaWrapper.usingproxy

    lua_pop(L, 2); // ...

    // Open table
    lua_newtable(L); // ... T
    luaW_registerfuncs(L, allocator ? defaulttable : NULL, table); // ... T

    // Open metatable, set up extends table
    luaL_newmetatable(L, classname); // ... T mt
    lua_newtable(L); // ... T mt {}
    lua_setfield(L, -2, LUAW_EXTENDS_KEY); // ... T mt
    luaW_registerfuncs(L, defaultmetatable, metatable); // ... T mt
    lua_setfield(L, -2, "metatable"); // ... T
}

template <typename T>
void luaW_register(lua_State* L, const char* classname, const luaL_Reg* table, const luaL_Reg* metatable, T* (*allocator)(lua_State*) = luaW_defaultallocator<T>, void (*deallocator)(lua_State*, T*) = luaW_defaultdeallocator<T>, void (*identifier)(lua_State*, T*) = luaW_defaultidentifier<T>)
{
    luaW_setfuncs(L, classname, table, metatable, allocator, deallocator, identifier); // ... T
    lua_pushvalue(L, -1); // ... T T
    lua_setglobal(L, classname); // ... T
}

// luaW_extend is used to declare that class T inherits from class U. All
// functions in the base class will be available to the derived class (except
// when they share a function name, in which case the derived class's function
// wins). This also allows luaW_to<T> to cast your object apropriately, as
// casts straight through a void pointer do not work.
template <typename T, typename U>
void luaW_extend(lua_State* L)
{
    if(!LuaWrapper<T>::classname)
        luaL_error(L, "attempting to call extend on a type that has not been registered");

    if(!LuaWrapper<U>::classname)
        luaL_error(L, "attempting to extend %s by a type that has not been registered", LuaWrapper<T>::classname);

    LuaWrapper<T>::cast = luaW_cast<T, U>;
    LuaWrapper<T>::identifier = luaW_identify<T, U>;

    luaL_getmetatable(L, LuaWrapper<T>::classname); // mt
    luaL_getmetatable(L, LuaWrapper<U>::classname); // mt emt

    // Point T's metatable __index at U's metatable for inheritance
    lua_newtable(L); // mt emt {}
    lua_pushvalue(L, -2); // mt emt {} emt
    lua_setfield(L, -2, "__index"); // mt emt {}
    lua_setmetatable(L, -3); // mt emt

    // Set up per-type tables to point at parent type
    lua_getfield(L, LUA_REGISTRYINDEX, LUAW_WRAPPER_KEY); // ... LuaWrapper

    lua_getfield(L, -1, LUAW_STORAGE_KEY); // ... LuaWrapper LuaWrapper.storage
    lua_getfield(L, -1, LuaWrapper<U>::classname); // ... LuaWrapper LuaWrapper.storage U
    lua_setfield(L, -2, LuaWrapper<T>::classname); // ... LuaWrapper LuaWrapper.storage
    lua_pop(L, 1); // ... LuaWrapper

    lua_getfield(L, -1, LUAW_HOLDS_KEY); // ... LuaWrapper LuaWrapper.holds
    lua_getfield(L, -1, LuaWrapper<U>::classname); // ... LuaWrapper LuaWrapper.holds U
    lua_setfield(L, -2, LuaWrapper<T>::classname); // ... LuaWrapper LuaWrapper.holds
    lua_pop(L, 1); // ... LuaWrapper

    lua_getfield(L, -1, LUAW_CACHE_KEY); // ... LuaWrapper LuaWrapper.cache
    lua_getfield(L, -1, LuaWrapper<U>::classname); // ... LuaWrapper LuaWrapper.cache U
    lua_setfield(L, -2, LuaWrapper<T>::classname); // ... LuaWrapper LuaWrapper.cache
    lua_pop(L, 1); // ... LuaWrapper

    lua_getfield(L, -1, LUAW_USING_PROXY_KEY); // ... LuaWrapper LuaWrapper.usingproxy
    lua_getfield(L, -1, LuaWrapper<U>::classname); // ... LuaWrapper LuaWrapper.usingproxy U
    lua_setfield(L, -2, LuaWrapper<T>::classname); // ... LuaWrapper LuaWrapper.usingproxy

    lua_pop(L, 2); // ...

    // Make a list of all types that inherit from U, for type checking
    lua_getfield(L, -2, LUAW_EXTENDS_KEY); // mt emt mt.extends
    lua_pushvalue(L, -2); // mt emt mt.extends emt
    lua_setfield(L, -2, LuaWrapper<U>::classname); // mt emt mt.extends
    lua_getfield(L, -2, LUAW_EXTENDS_KEY); // mt emt mt.extends emt.extends
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
    {
        // mt emt mt.extends emt.extends k v
        lua_pushvalue(L, -2); // mt emt mt.extends emt.extends k v k
        lua_pushvalue(L, -2); // mt emt mt.extends emt.extends k v k
        lua_rawset(L, -6); // mt emt mt.extends emt.extends k v
    }

    lua_pop(L, 4); // mt emt
}


// Class to facilitate the semi-autonomous self-registration of LuaW classes.
// To use this system, classes must implement the following:
//    class name:  const char *luaClassName
//    method list: const luaL_Reg Item::luaMethods[] = { }
// Then, somewhere in the class definition (.cpp file), add the line:
//    REGISTER_LUA_CLASS(className);
// or
//    REGISTER_LUA_SUBCLASS(className, parentClass);
// When you actually create your Lua instance (L), call LuaW_Registrar::registerClasses(L)
// to make your methods available.
// And with that, your class will be registered with LuaWrapper.
//
// A quick overview of how the registration system works is REGISTER_LUA_CLASS and REGISTER_LUA_SUBCLASS 
// are actually dummy declarations that during initialization, build lists of registration functions 
// called registrationFunctions and extensionFunctions.  When you actually create your L object and 
// want to register your methods with it, calling registerClass() uses those lists to call the 
// appropriate LuaW registration/extension methods.  
class LuaW_Registrar
{
private:
   typedef void (*luaW_regFunc)(lua_State *);

   struct ClassParent { 
      const char *name;   
      const char *parent; 
   };

   typedef std::pair<ClassName, luaW_regFunc> NameFunctionPair;
   typedef std::map <ClassName, luaW_regFunc> FunctionMap;     // Map of class name and registration functions

   typedef std::pair<ClassName, const LuaFunctionProfile *> NameArgumentListPair;


   // List of registration functions
   static FunctionMap &getRegistrationFunctions()
   {
      static FunctionMap registrationFunctions;
      return registrationFunctions;
   }


   // List of extension functions
   static FunctionMap &getExtensionFunctions()
   {
      static FunctionMap extensionFunctions;
      return extensionFunctions;
   }


   // Mapping of function name to arguments to various defined functions
   static ArgMap &getArgMap()
   {
      static ArgMap argMap;
      return argMap;
   }


   // Unordered list of classes
   static std::vector<ClassParent> &getUnorderedClassList()
   {
      static std::vector<ClassParent> unorderedClassList;
      return unorderedClassList;
   }


   // List of classes sorted by initialization order
   static std::vector<ClassName> &getOrderedClassList()
   {
      static std::vector<ClassName> orderedClassList;
      return orderedClassList;
   }


   // Helper function -- return true if name is found in orderedClassList
   static bool findInOrderedClassList(const char *name)
   {
      for(int i = 0; i < (int)getOrderedClassList().size(); i++)
         if(strcmp(name, getOrderedClassList()[i]) == 0)
            return true;

      return false;
   }


   // Helper function -- move function from unorderdClassList to orderedClassList list 
   static void moveToOrderedList(int i)
   {
      getOrderedClassList().push_back(getUnorderedClassList()[i].name);
      getUnorderedClassList().erase(getUnorderedClassList().begin() + i);
   }


   // Sort vector of classes so parents of each class are listed before their children
   static void sortClassList()
   {
      std::size_t itemsRemainingInList;

      itemsRemainingInList = getUnorderedClassList().size();     

      // Iterate through unordered objects -- these should all have parents that are already in orderedClassList
      while(itemsRemainingInList > 0)
      {
         bool foundAtLeastOneThisIteration = false;      // For detecting and preventing endless loops due to hiearchy problems

         for(int i = (int)getUnorderedClassList().size() - 1; i >= 0; i--)    // Descending order for greater efficiency
            // If parent is in orderedClassList, we can move the item to the orderedClassList
            if(findInOrderedClassList(getUnorderedClassList()[i].parent))
            {
               moveToOrderedList(i);
               foundAtLeastOneThisIteration = true;
            }

         // Make sure we move at least one item per iteration; if we don't, we're stuck.  This block should nevever run.
         TNLAssert(foundAtLeastOneThisIteration, "Registering items is stuck -- check luaW class/subclass declarations!");

         if(!foundAtLeastOneThisIteration)
            for(int i = (int)getUnorderedClassList().size() - 1; i > -1; i--)
               moveToOrderedList(i);

         itemsRemainingInList = getUnorderedClassList().size();   
      }  // end while
   }


   // registerClass() helper
   template<class T>
   static void saveRegistration()
   {
      NameFunctionPair regPair(T::luaClassName, &registerClass<T>);
      getRegistrationFunctions().insert(regPair);

      // The following are only used when dumping the lua documentation with -luadoc
      NameArgumentListPair argPair(T::luaClassName, T::functionArgs);
      getArgMap().insert(argPair);
   }

protected:
   template<class T>
   static void registerClass(lua_State *L)
   {
      luaW_register<T>(L, T::luaClassName, NULL, T::luaMethods);
      lua_pop(L, 1);       // Remove metatable from stack
   }


   template<class T>
   static void registerClass()
   {
      getOrderedClassList().push_back(T::luaClassName);        // No parent, so add it to front of ordered list (no sorting needed)
      saveRegistration<T>();
   }

   template<class T, class U>
   static void registerClass()
   {
      ClassParent key = {T::luaClassName, U::luaClassName};    // This class has a parent and needs to be
      getUnorderedClassList().push_back(key);                  // registered after parent (will require sorting)

      saveRegistration<T>();

      // T extends U
      NameFunctionPair extPair(T::luaClassName, &luaW_extend<T, U>);
      getExtensionFunctions()   .insert(extPair);
   }

public:
   static void registerClasses(lua_State *L)
   {
      sortClassList();
      std::vector<ClassName> &orderedClassList = getOrderedClassList();

      // Register all our classes
      for(unsigned int i = 0; i < orderedClassList.size(); i++)
         getRegistrationFunctions()[orderedClassList[i]](L);

      // Extend those that need extending
      for(unsigned int i = 0; i < orderedClassList.size(); i++)
      {
         // Skip base classes
         if(getExtensionFunctions()[orderedClassList[i]] == NULL)
            continue;

         getExtensionFunctions()[orderedClassList[i]](L);
      }
   }
};


// Helper classes that extend LuaW_Registrar.  These are intended for use
// only by one of the two macro definitions below.
template<class T>
class LuaW_Registrar1Arg : public LuaW_Registrar
{
public:
   LuaW_Registrar1Arg() { registerClass<T>(); }
};


template<class T, class U>
class LuaW_Registrar2Args : public LuaW_Registrar
{
public:
   LuaW_Registrar2Args() { registerClass<T, U>(); }
};


#define REGISTER_LUA_CLASS(cls) \
   static LuaW_Registrar1Arg<cls> luaclass_##cls

#define REGISTER_LUA_SUBCLASS(cls, parent) \
   static LuaW_Registrar2Args<cls, parent> luaclass_##cls



template <class T>
class LuaProxy
{
private:
    bool mDefunct;
    T *mProxiedObject;

public:
    // Default constructor
    LuaProxy() { TNLAssert(false, "Not used"); }

    // Typical constructor
    explicit LuaProxy(T *obj)
    {
      mProxiedObject = obj;
      obj->setLuaProxy(this);
      mDefunct = false;
    }

   // Destructor
   ~LuaProxy()
   {
      if(!mDefunct)
         mProxiedObject->mLuaProxy = NULL;
   }


   T   *getProxiedObject() { return mProxiedObject; }
   bool isDefunct()        { return mDefunct;       }

   void setDefunct(bool isDefunct) { mDefunct = isDefunct; }
};


// This goes in the constructor of the "wrapped class"
#define LUAW_CONSTRUCTOR_INITIALIZATIONS \
   mLuaProxy = NULL



// The following macros are used in the header of the 'wrapped class'.  Each one sets up the class
// in a slightly different manner

// This macro is used for a normal class that will have it's own Lua constructor and can be
// instantiated and accessed from Lua (pushed from c++)
#define  LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(className) \
   LuaProxy<className> *mLuaProxy; \
   LuaProxy<className> *getLuaProxy() { return mLuaProxy; } \
   virtual void setLuaProxy(LuaProxy<className> *obj) { mLuaProxy = obj; } \
   virtual void push(lua_State *L) { luaW_push(L, this); }

// This one is for an abstract class and cannot be instantiated or returned as an object in Lua
#define  LUAW_DECLARE_ABSTRACT_CLASS(className) \
   LuaProxy<className> *mLuaProxy; \
   LuaProxy<className> *getLuaProxy() { return mLuaProxy; } \
   virtual void setLuaProxy(LuaProxy<className> *obj) { mLuaProxy = obj; } \
   className(lua_State *L) { THROW_LUA_EXCEPTION(L, "Illegal attempt to instantiate abstract class!"); }

// This is used for a class that you want to access (return as an object) but NOT instantiated (like PlayerInfo)
#define  LUAW_DECLARE_NON_INSTANTIABLE_CLASS(className) \
   LuaProxy<className> *mLuaProxy; \
   LuaProxy<className> *getLuaProxy() { return mLuaProxy; } \
   virtual void setLuaProxy(LuaProxy<className> *obj) { mLuaProxy = obj; } \
   virtual void push(lua_State *L) { luaW_push(L, this); } \
   className(lua_State *L) { THROW_LUA_EXCEPTION(L, "Illegal attempt to instantiate a non-instantiable class!"); }

// This is the same as the CUSTOM_CONSTRUCTOR variant, except it sets up a constructor for you.  It
// allows instantiation and access from Lua
// TODO- Convert everything to use the above, rename it, and get rid of this one -- what? i don't understand ~raptor
#define  LUAW_DECLARE_CLASS(className) \
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(className) \
   className(lua_State *L) { LUAW_CONSTRUCTOR_INITIALIZATIONS; } 



// And this goes in the destructor of the "wrapped class"
#define LUAW_DESTRUCTOR_CLEANUP \
   if(mLuaProxy) mLuaProxy->setDefunct(true)



// Runs a method on a proxied object.  Returns nil if the proxied object no longer exists, so Lua scripts may need to check for this.
// Wraps a standard method (one that takes L as a single parameter) within a proxy check. 
template <typename T, int (T::*methodName)(lua_State * )>
int luaW_doMethod(lua_State *L)
{
   T *w = luaW_check<T>(L, 1);
   if(w) 
   {
      lua_remove(L, 1);
      return (w->*methodName)(L);
   }

   lua_pushnil(L);
   return 1;
}

/*
 * Copyright (c) 2010-2013 Alexander Ames
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#endif // LUA_WRAPPER_H_
