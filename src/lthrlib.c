/*
** $Id: lthrlib.c $
** Multithreading library
** See Copyright Notice in lua.h
*/

#define lthrlib_c
#define LUA_LIB

#include "lprefix.h"


#include <errno.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#include "lstate.h"


static struct timespec timeout2abstime (lua_State *L, int idx) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  if (lua_isinteger(L, idx)) /* timeout is integer */
    ts.tv_sec += lua_tointeger(L, idx);
  else { /* timeout is float */
    lua_Number whole;
    lua_Number part = l_mathop(modf)(luaL_checknumber(L, idx), &whole);
    ts.tv_sec += (time_t)whole;
    ts.tv_nsec += (long)(1e9 * part);
    if (ts.tv_nsec >= 1000000000) {
      ts.tv_sec++;
      ts.tv_nsec -= 1000000000;
    }
    else if (ts.tv_nsec < 0) { /* with negative timeouts */
      ts.tv_sec--;
      ts.tv_nsec += 1000000000;
    }
  }
  return ts;
}

static int handle_timed_op_err (lua_State *L, int err) {
  switch (err) {
    case 0:
      lua_pushboolean(L, 1);
      return 1;
    case ETIMEDOUT:
      luaL_pushfail(L);
      return 1;
    default:
      return luaL_error(L, "%s", strerror(err));
  }
}



static int thread_join (lua_State *L) {
  OSThread *p = *(OSThread **)luaL_checkudata(L, 1, "thread");
  int err;
  if (lua_isnoneornil(L, 2)) /* no timeout */
    err = luaE_joinosthread(L, p, NULL);
  else { /* with timeout */
    struct timespec ts = timeout2abstime(L, 2);
    err = luaE_joinosthread(L, p, &ts);
  }
  return handle_timed_op_err(L, err);
}

static int thread_gc (lua_State *L) {
  OSThread *p = *(OSThread **)luaL_checkudata(L, 1, "thread");
  luaE_unrefosthread(L, p);
  return 0;
}

static const luaL_Reg threadmeth[] = {
  {"join", thread_join},
  {"__gc", thread_gc},
  {NULL, NULL}
};



static int lock_acquire (lua_State *L) {
  pthread_mutex_t *p = luaL_checkudata(L, 1, "lock");
  int err;
  if (lua_isnoneornil(L, 2)) /* no timeout */
    err = pthread_mutex_lock(p);
  else { /* with timeout */
    struct timespec ts = timeout2abstime(L, 2);
    err = pthread_mutex_timedlock(p, &ts);
  }
  return handle_timed_op_err(L, err);
}

static int lock_release (lua_State *L) {
  pthread_mutex_t *p = luaL_checkudata(L, 1, "lock");
  int err = pthread_mutex_unlock(p);
  if (err != 0)
    return luaL_error(L, "%s", strerror(err));
  return 0;
}

static int lock_gc (lua_State *L) {
  pthread_mutex_t *p = luaL_checkudata(L, 1, "lock");
  pthread_mutex_destroy(p);
  return 0;
}

static const luaL_Reg lockmeth[] = {
  {"acquire", lock_acquire},
  {"release", lock_release},
  {"__gc", lock_gc},
  {NULL, NULL}
};



static void *start_routine (void *arg) {
  lua_State *L = arg;
  lua_pcall(L, lua_gettop(L) - 1, 0, 0); /* pcall(func, ...) */
  lua_pushthread(L);
  lua_pushnil(L);
  lua_settable(L, LUA_REGISTRYINDEX); /* registry[L] = nil */
  return NULL;
}

static int new (lua_State *L) {
  int narg = lua_gettop(L);
  lua_State *L2 = lua_newthread(L); /* create Lua thread */
  lua_rotate(L, 1, 1); /* arg1, ..., argn, L2 -> L2, arg1, ..., argn */
  lua_xmove(L, L2, narg); /* move all args into L2 */
  lua_pushvalue(L, -1);
  lua_settable(L, LUA_REGISTRYINDEX); /* registry[L2] = L2, to prevent it from being GCed */
  OSThread **pp = lua_newuserdatauv(L, sizeof(OSThread*), 0); /* create Lua object for OS thread */
  luaL_setmetatable(L, "thread");
  OSThread *p = *pp = luaM_new(L, OSThread); /* initialize OS thread */
  pthread_mutex_init(&p->joinlock, NULL);
  p->joined = 0;
  p->nrefs = 2; /* only two refs at first: userdata and osthreads list */
  p->prev = p->next = NULL;
  pthread_create(&p->id, NULL, start_routine, (void *)L2); /* start OS thread */
  global_State *g = G(L); /* append to global join queue */
  pthread_mutex_lock(&g->osthreadslock);
  p->next = g->osthreads;
  if (p->next != NULL)
    p->next->prev = p;
  g->osthreads = p;
  pthread_mutex_unlock(&g->osthreadslock);
  return 1;
}

static int sleep_ (lua_State *L) {
  useconds_t micros;
  if (lua_isinteger(L, 1)) /* duration is integer */
    micros = 1000000 * (useconds_t)lua_tointeger(L, 1);
  else { /* duration is float */
    micros = 1e6 * luaL_checknumber(L, 1);
  }
  usleep(micros);
  return 0;
}

static int newlock (lua_State *L) {
  pthread_mutex_t *p = lua_newuserdatauv(L, sizeof(pthread_mutex_t), 0);
  luaL_setmetatable(L, "lock");
  pthread_mutex_init(p, NULL);
  return 1;
}

static int newrelock (lua_State *L) {
  pthread_mutex_t *p = lua_newuserdatauv(L, sizeof(pthread_mutex_t), 0);
  luaL_setmetatable(L, "lock");
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(p, &attr);
  pthread_mutexattr_destroy(&attr);
  return 1;
}

static const luaL_Reg thrlib[] = {
  {"new", new},
  {"sleep", sleep_},
  {"newlock", newlock},
  {"newrelock", newrelock},
  {NULL, NULL}
};



static void init_class (lua_State *L, const char *name, const luaL_Reg *meth) {
  luaL_newmetatable(L, name); /* create metatable */
  luaL_setfuncs(L, meth, 0); /* add regular and meta- methods to metatable */
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index"); /* metatable.__index = metatable */
  lua_pop(L, 1); /* pop metatable */
}

LUAMOD_API int luaopen_thread (lua_State *L) {
  init_class(L, "thread", threadmeth);
  init_class(L, "lock", lockmeth);
  luaL_newlib(L, thrlib); /* create 'thread' table */
  return 1;
}
