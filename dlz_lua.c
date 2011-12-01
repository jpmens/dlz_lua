
/* dlz_lua.c (C)2011 by Jan-Piet Mens <jpmens@gmail.com>
 * A dlz_dlopen() driver for BIND's named which implements a Lua back-end.
 * Handle with care. ;-)
 *
 * Most of this code is swiped from
 *	$Id: dlz_example.c,v 1.3 2011-10-20 22:01:48 each Exp $ 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "dlz_minimal.h"

struct dlz_example_data {
	char *zone_name;

	isc_boolean_t transaction_started;

	/* Helper functions from the dlz_dlopen driver */
	log_t *log;
	dns_sdlz_putrr_t *putrr;
	dns_sdlz_putnamedrr_t *putnamedrr;
	dns_dlz_writeablezone_t *writeable_zone;

	/* The Lua interpreter */
	lua_State *L;

	/* The Lua script filename, passed as argv[2] */
	char *lua_script;
};

static isc_result_t
fmt_address(isc_sockaddr_t *addr, char *buffer, size_t size) {
	char addr_buf[100];
	const char *ret;

	switch (addr->type.sa.sa_family) {
	case AF_INET:
		ret = inet_ntop(AF_INET, &addr->type.sin.sin_addr, addr_buf,
				sizeof(addr_buf));
		break;
	case AF_INET6:
		ret = inet_ntop(AF_INET6, &addr->type.sin6.sin6_addr, addr_buf,
				sizeof(addr_buf));
		break;
	default:
		return (ISC_R_FAILURE);
	}

	if (ret == NULL)
		return (ISC_R_FAILURE);

	snprintf(buffer, size, "%s", addr_buf);
	return (ISC_R_SUCCESS);
}

/*
 * Return the version of the API
 */
int
dlz_version(unsigned int *flags) {
	UNUSED(flags);
	return (DLZ_DLOPEN_VERSION);
}

/*
 * Remember a helper function from the bind9 dlz_dlopen driver
 */
static void
b9_add_helper(struct dlz_example_data *state,
	      const char *helper_name, void *ptr)
{
	if (strcmp(helper_name, "log") == 0)
		state->log = (log_t *)ptr;
	if (strcmp(helper_name, "putrr") == 0)
		state->putrr = (dns_sdlz_putrr_t *)ptr;
	if (strcmp(helper_name, "putnamedrr") == 0)
		state->putnamedrr = (dns_sdlz_putnamedrr_t *)ptr;
	// if (strcmp(helper_name, "writeable_zone") == 0)
	// 	state->writeable_zone = (dns_dlz_writeablezone_t *)ptr;
}

int
dlz_lua_bindLog(lua_State *lua)
{
        const char *str;

        if (lua_gettop(lua) >= 1) {
                str =  lua_tostring(lua, 1);
                fprintf(stderr, "+++ LUA SAYS: %s\n", str);
        }
        return (0);
}

static const char *dlz_lua_fromtable(lua_State *L, char *key)
{
	const char *value = NULL;

	lua_pushstring(L, key);
	lua_gettable(L, -2);
	if (!lua_isnil(L, -1))
		value = lua_tostring(L, -1);
	lua_pop(L, 1);
	return (value);
}


/*
 * Called to initialize the driver
 */
isc_result_t
dlz_create(const char *dlzname, unsigned int argc, char *argv[],
	   void **dbdata, ...)
{
	struct dlz_example_data *state;
	const char *helper_name;
	va_list ap;
	int error;

	UNUSED(dlzname);


	state = calloc(1, sizeof(struct dlz_example_data));
	if (state == NULL)
		return (ISC_R_NOMEMORY);

	/* Fill in the helper functions */
	va_start(ap, dbdata);
	while ((helper_name = va_arg(ap, const char *)) != NULL) {
		b9_add_helper(state, helper_name, va_arg(ap, void*));
	}
	va_end(ap);

	if (argc < 3) {
		state->log(ISC_LOG_ERROR,
			   "dlz_lua: please specify a zone name, and a Lua script file");
		return (ISC_R_FAILURE);
	}

	state->zone_name = strdup(argv[1]);
	state->lua_script = strdup(argv[2]);

	state->L = lua_open();
	luaL_openlibs(state->L);

        /* Add local functions to Lua interpreter */
	lua_pushcfunction(state->L, dlz_lua_bindLog);
	lua_setglobal(state->L, "bindlog");

	/* Attempt to load the Lua script */
	error = luaL_dofile(state->L, state->lua_script);
	if (error) {
		state->log(ISC_LOG_ERROR,
			   "dlz_lua: failed to load script %s: %s",
			   state->lua_script, lua_tostring(state->L, -1));
		lua_pop(state->L, 1);	/* pop error message */
		return (ISC_R_FAILURE);
	}

	state->log(ISC_LOG_INFO,
		   "dlz_lua: started for zone %s with script %s",
		   state->zone_name, state->lua_script);

	*dbdata = state;
	return (ISC_R_SUCCESS);
}

/*
 * Shut down the backend
 */
void
dlz_destroy(void *dbdata) {
	struct dlz_example_data *state = (struct dlz_example_data *)dbdata;


	state->log(ISC_LOG_INFO,
		   "dlz_lua: shutting down zone %s",
		   state->zone_name);

	lua_close(state->L);
	free(state->zone_name);
	free(state);
}


/*
 * See if we handle a given zone
 */
isc_result_t
dlz_findzonedb(void *dbdata, const char *name) {
	struct dlz_example_data *state = (struct dlz_example_data *)dbdata;

	if (strcasecmp(state->zone_name, name) == 0)
		return (ISC_R_SUCCESS);

	return (ISC_R_NOTFOUND);
}

/*
 * Look up one record in the sample database.
 *
 * If the queryname is "source-addr", we add a TXT record containing
 * the address of the client; this demonstrates the use of 'methods'
 * and 'clientinfo'.
 */
isc_result_t
dlz_lookup(const char *zone, const char *name, void *dbdata,
	   dns_sdlzlookup_t *lookup, dns_clientinfomethods_t *methods,
	   dns_clientinfo_t *clientinfo)
{
	isc_result_t result;
	struct dlz_example_data *state = (struct dlz_example_data *)dbdata;
	isc_boolean_t found = ISC_FALSE;
	isc_sockaddr_t *src;
	char full_name[512], client_addr[128];
	int t, rc;

	UNUSED(zone);

	if (strcmp(name, "@") == 0)
		strcpy(full_name, state->zone_name);
	else
		sprintf(full_name, "%s.%s", name, state->zone_name);

	// fprintf(stderr, "+++++++ dlz: name=[%s]\n", name);

	/* Format client's address */
	strcpy(client_addr, "unknown");
	if (methods != NULL &&
	    methods->version - methods->age >=
		    DNS_CLIENTINFOMETHODS_VERSION)
	{
		methods->sourceip(clientinfo, &src);
		fmt_address(src, client_addr, sizeof(client_addr));
	}

	lua_getglobal(state->L, "lookup");
	lua_pushstring(state->L, name);
	lua_pushstring(state->L, state->zone_name);
	lua_pushstring(state->L, client_addr);
	lua_call(state->L, 3, 2);
	rc = (int)lua_tonumber(state->L, -1);
	if (rc != 0) {
		return (ISC_R_SUCCESS);
	}

	t = -1;

	if (lua_istable(state->L, t)) {
		int tlen = lua_objlen(state->L, t), n;

		// printf("** is table; tlen == %d\n", tlen);

		for (n=1; n <= tlen; ++n) {
			const char *type, *rdata, *ttlstr;

			found = ISC_TRUE;

			lua_pushnumber(state->L, n);
			lua_gettable(state->L, 2);

			type = dlz_lua_fromtable(state->L, "type");
			rdata = dlz_lua_fromtable(state->L, "rdata");
			ttlstr = dlz_lua_fromtable(state->L, "ttl");

			result = state->putrr(lookup, type, 
					strtoul(ttlstr, NULL, 10), rdata);
			if (result != ISC_R_SUCCESS)
				return (result);

			lua_pop(state->L, 1); /* Remove row */
		}
		lua_pop(state->L, 1); // table
	}
	lua_settop(state->L, 0);

	if (!found)
		return (ISC_R_NOTFOUND);

	return (ISC_R_SUCCESS);
}


/*
 * See if a zone transfer is allowed
 */
isc_result_t
dlz_allowzonexfr(void *dbdata, const char *name, const char *client) {
	UNUSED(client);

	/* Just say yes for all our zones */
	return (dlz_findzonedb(dbdata, name));
}

/*
 * Perform a zone transfer
 */
isc_result_t
dlz_allnodes(const char *zone, void *dbdata, dns_sdlzallnodes_t *allnodes) {

	UNUSED(zone);

	return (ISC_R_FAILURE);
}


/*
 * Configure a writeable zone
 */
isc_result_t
dlz_configure(dns_view_t *view, void *dbdata) {

	return (ISC_R_SUCCESS);
}

/*
 * Authorize a zone update
 */
isc_boolean_t
dlz_ssumatch(const char *signer, const char *name, const char *tcpaddr,
	     const char *type, const char *key, uint32_t keydatalen,
	     unsigned char *keydata, void *dbdata)
{
	UNUSED(tcpaddr);
	UNUSED(type);
	UNUSED(key);
	UNUSED(keydatalen);
	UNUSED(keydata);

	return (ISC_FALSE);
}

