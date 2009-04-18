/*
 * Copyright (c) 2003-2007 by FlashCode <flashcode@flashtux.org>
 * See README for License detail, AUTHORS for developers list.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* weechat-lua.c: Lua plugin support for WeeChat */

#undef _

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../../weechat-plugin.h"
#include "../weechat-script.h"


char plugin_name[]        = "Lua";
char plugin_version[]     = "0.1";
char plugin_description[] = "Lua scripts support";

t_weechat_plugin *lua_plugin;

t_plugin_script *lua_scripts = NULL;
t_plugin_script *lua_current_script = NULL;
char *lua_current_script_filename = NULL;
lua_State *lua_current_interpreter = NULL;


/*
 * weechat_lua_exec: execute a Lua script
 */

void *
weechat_lua_exec (t_weechat_plugin *plugin,
		  t_plugin_script *script,
		  int ret_type,
		  char *function, char *arg1, char *arg2, char *arg3)
{
    void *ret_value;
    int *ret_i;

    lua_current_interpreter = script->interpreter;

    lua_getglobal (lua_current_interpreter, function);
    lua_current_script = script;
    
    if (arg1)
    {
        lua_pushstring (lua_current_interpreter, (arg1) ? arg1 : "");
        if (arg2)
        {
            lua_pushstring (lua_current_interpreter, (arg2) ? arg2 : "");
            if (arg3)
                lua_pushstring (lua_current_interpreter, (arg3) ? arg3 : "");
        }
    }
    
    if (lua_pcall (lua_current_interpreter,
                   (arg1) ? ((arg2) ? ((arg3) ? 3 : 2) : 1) : 0, 1, 0) != 0)
    {
	plugin->print_server (plugin,
                              "Lua error: unable to run function \"%s\"",
                              function);
	plugin->print_server (plugin,
                              "Lua error: %s",
                              lua_tostring (lua_current_interpreter, -1));
        return NULL;
    }
    
    if (ret_type == SCRIPT_EXEC_STRING)
	ret_value = strdup ((char *) lua_tostring (lua_current_interpreter, -1));
    else if (ret_type == SCRIPT_EXEC_INT)
    {
	ret_i = (int *) malloc (sizeof(int));
	if (ret_i)
	    *ret_i = lua_tonumber (lua_current_interpreter, -1);
	ret_value = ret_i;
    }
    else
    {
	lua_plugin->print_server (lua_plugin, 
				  "Lua error: wrong parameters for function \"%s\"",
				  function);
	return NULL;
    }
    
    return ret_value; 
}

/*
 * weechat_lua_cmd_msg_handler: general command/message handler for Lua
 */

int
weechat_lua_cmd_msg_handler (t_weechat_plugin *plugin,
                             int argc, char **argv,
                             char *handler_args, void *handler_pointer)
{
    int *r;
    int ret;
    
    if (argc >= 3)
    {
        r = (int *) weechat_lua_exec (plugin, (t_plugin_script *)handler_pointer,
				      SCRIPT_EXEC_INT,
				      handler_args, argv[0], argv[2], NULL);
	if (r == NULL)
	    ret = PLUGIN_RC_KO;
	else
	{
	    ret = *r;
	    free (r);
	}
	return ret;
    }
    else
        return PLUGIN_RC_KO;
}

/*
 * weechat_lua_timer_handler: general timer handler for Lua
 */

int
weechat_lua_timer_handler (t_weechat_plugin *plugin,
                           int argc, char **argv,
                           char *handler_args, void *handler_pointer)
{
    /* make C compiler happy */
    (void) argc;
    (void) argv;
    int *r;
    int ret;
    
    r = (int *) weechat_lua_exec (plugin, (t_plugin_script *)handler_pointer,
				  SCRIPT_EXEC_INT,
				  handler_args, NULL, NULL, NULL);
    if (r == NULL)
	ret = PLUGIN_RC_KO;
    else
    {
	ret = *r;
	free (r);
    }
    return ret;
}

/*
 * weechat_lua_keyboard_handler: general keyboard handler for Lua
 */

int
weechat_lua_keyboard_handler (t_weechat_plugin *plugin,
                              int argc, char **argv,
                              char *handler_args, void *handler_pointer)
{
    int *r;
    int ret;
    
    if (argc >= 3)
    {
        r = (int *) weechat_lua_exec (plugin, (t_plugin_script *)handler_pointer,
				      SCRIPT_EXEC_INT,
				      handler_args, argv[0], argv[1], argv[2]);
	if (r == NULL)
	    ret = PLUGIN_RC_KO;
	else
	{
	    ret = *r;
	    free (r);
	}
	return ret;
    }
    else
        return PLUGIN_RC_KO;
}

/*
 * weechat_lua_event_handler: general event handler for Lua
 */

int
weechat_lua_event_handler (t_weechat_plugin *plugin,
                           int argc, char **argv,
                           char *handler_args, void *handler_pointer)
{
    int *r;
    int ret;

    if (argc >= 1)
    {
        r = (int *) weechat_lua_exec (plugin, (t_plugin_script *)handler_pointer,
                                      SCRIPT_EXEC_INT,
                                      handler_args,
                                      argv[0],
                                      (argc >= 2) ? argv[1] : NULL,
                                      (argc >= 3) ? argv[2] : NULL);
        if (r == NULL)
            ret = PLUGIN_RC_KO;
        else
        {
            ret = *r;
            free (r);
        }
        return ret;
    }
    else
        return PLUGIN_RC_KO;
}

/*
 * weechat_lua_modifier: general modifier for Lua
 */

char *
weechat_lua_modifier (t_weechat_plugin *plugin,
                      int argc, char **argv,
                      char *modifier_args, void *modifier_pointer)
{
    if (argc >= 2)
        return (char *) weechat_lua_exec (plugin, (t_plugin_script *)modifier_pointer,
					  SCRIPT_EXEC_STRING,
					  modifier_args, argv[0], argv[1], NULL);
    else
        return NULL;
}

/*
 * weechat_lua_register: startup function for all WeeChat Lua scripts
 */

static int
weechat_lua_register (lua_State *L)
{
    const char *name, *version, *shutdown_func, *description, *charset;
    int n;
    
    /* make C compiler happy */
    (void) L;

    lua_current_script = NULL;
    
    name = NULL;
    version = NULL;
    shutdown_func = NULL;
    description = NULL;
    charset = NULL;
    
    n = lua_gettop (lua_current_interpreter);

    if ((n < 4) || (n > 5))
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"register\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    switch (n)
    {
        case 4:
            name = lua_tostring (lua_current_interpreter, -4);
            version = lua_tostring (lua_current_interpreter, -3);
            shutdown_func = lua_tostring (lua_current_interpreter, -2);
            description = lua_tostring (lua_current_interpreter, -1);
            break;
        case 5:
            name = lua_tostring (lua_current_interpreter, -5);
            version = lua_tostring (lua_current_interpreter, -4);
            shutdown_func = lua_tostring (lua_current_interpreter, -3);
            description = lua_tostring (lua_current_interpreter, -2);
            charset = lua_tostring (lua_current_interpreter, -1);
            break;
    }
    
    if (weechat_script_search (lua_plugin, &lua_scripts, (char *) name))
    {
        /* error: another scripts already exists with this name! */
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to register "
                                  "\"%s\" script (another script "
                                  "already exists with this name)",
                                  name);
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    /* register script */
    lua_current_script = weechat_script_add (lua_plugin,
					     &lua_scripts,
					     (lua_current_script_filename) ?
					     lua_current_script_filename : "",
					     (char *) name, 
					     (char *) version, 
					     (char *) shutdown_func,
					     (char *) description,
                                             (char *) charset);
    if (lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua: registered script \"%s\", "
                                  "version %s (%s)",
                                  name, version, description);
    }
    else
    {
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_set_charset: set script charset
 */

static int
weechat_lua_set_charset (lua_State *L)
{
    const char *charset;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to set charset, "
                                  "script not initialized");	
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    charset = NULL;
 
    n = lua_gettop (lua_current_interpreter);

    if (n != 1)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"set_charset\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    charset = lua_tostring (lua_current_interpreter, -1);
    
    weechat_script_set_charset (lua_plugin,
                                lua_current_script,
                                (char *) charset);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_print: print message into a buffer (current or specified one)
 */

static int
weechat_lua_print (lua_State *L)
{
    const char *message, *channel_name, *server_name;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to print message, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    message = NULL;
    channel_name = NULL;
    server_name = NULL;
    
    n = lua_gettop (lua_current_interpreter);

    switch (n)
    {
    case 1:
	message = lua_tostring (lua_current_interpreter, -1);
	break;
    case 2:
	channel_name = lua_tostring (lua_current_interpreter, -2);
	message = lua_tostring (lua_current_interpreter, -1);
	break;
    case 3:
	server_name = lua_tostring (lua_current_interpreter, -3);
	channel_name = lua_tostring (lua_current_interpreter, -2);
	message = lua_tostring (lua_current_interpreter, -1);
	break;
    default:
	lua_plugin->print_server (lua_plugin,
				  "Lua error: wrong parameters for "
				  "\"print\" function");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    lua_plugin->print (lua_plugin,
                       (char *) server_name,
                       (char *) channel_name,
                       "%s", (char *) message);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_print_server: print message into a buffer server
 */

static int
weechat_lua_print_server (lua_State *L)
{
    const char *message;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to print message, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    message = NULL;

    n = lua_gettop (lua_current_interpreter);
    
    if (n == 1)
	message = lua_tostring (lua_current_interpreter, -1);
    else
    {
	lua_plugin->print_server (lua_plugin,
				  "Lua error: wrong parameters for "
				  "\"print_server\" function");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    lua_plugin->print_server (lua_plugin, "%s", (char *) message);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_print_infobar: print message to infobar
 */

static int
weechat_lua_print_infobar (lua_State *L)
{
    const char *message;
    int delay, n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to print infobar message, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    delay = 1;
    message = NULL;

    n = lua_gettop (lua_current_interpreter);

    if (n != 2)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"print_infobar\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    delay = lua_tonumber (lua_current_interpreter, -2);
    message = lua_tostring (lua_current_interpreter, -1);
    
    lua_plugin->print_infobar (lua_plugin, delay, "%s", (char *) message);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_remove_infobar: remove message(s) in infobar
 */

static int
weechat_lua_remove_infobar (lua_State *L)
{
    int n, how_many;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to remove infobar message(s), "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    how_many = 0;
    
    n = lua_gettop (lua_current_interpreter);
    
    if (n == 1)
        how_many = lua_tonumber (lua_current_interpreter, -1);
    
    lua_plugin->infobar_remove (lua_plugin, how_many);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_log: log message in server/channel (current or specified ones)
 */

static int
weechat_lua_log (lua_State *L)
{
    const char *message, *channel_name, *server_name;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to print message, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    message = NULL;
    channel_name = NULL;
    server_name = NULL;
    
    n = lua_gettop (lua_current_interpreter);

    switch (n)
    {
        case 1:
            message = lua_tostring (lua_current_interpreter, -1);
            break;
        case 2:
            channel_name = lua_tostring (lua_current_interpreter, -2);
            message = lua_tostring (lua_current_interpreter, -1);
            break;
        case 3:
            server_name = lua_tostring (lua_current_interpreter, -3);
            channel_name = lua_tostring (lua_current_interpreter, -2);
            message = lua_tostring (lua_current_interpreter, -1);
            break;
        default:
            lua_plugin->print_server (lua_plugin,
                                      "Lua error: wrong parameters for "
                                      "\"log\" function");
            lua_pushnumber (lua_current_interpreter, 0);
            return 1;
    }
    
    lua_plugin->log (lua_plugin,
		     (char *) server_name,
		     (char *) channel_name,
		     "%s", (char *) message);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_command: send command to server
 */

static int
weechat_lua_command (lua_State *L)
{
    const char *command, *channel_name, *server_name;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to run command, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    command = NULL;
    channel_name = NULL;
    server_name = NULL;
    
    n = lua_gettop (lua_current_interpreter);
    
    switch (n)
    {
        case 1:
            command = lua_tostring (lua_current_interpreter, -1);
            break;
        case 2:
            channel_name = lua_tostring (lua_current_interpreter, -2);
            command = lua_tostring (lua_current_interpreter, -1);
            break;
        case 3:
            server_name = lua_tostring (lua_current_interpreter, -3);
            channel_name = lua_tostring (lua_current_interpreter, -2);
            command = lua_tostring (lua_current_interpreter, -1);
            break;
        default:
            lua_plugin->print_server (lua_plugin,
                                      "Lua error: wrong parameters for "
                                      "\"command\" function");
            lua_pushnumber (lua_current_interpreter, 0);
            return 1;
    }

    lua_plugin->exec_command (lua_plugin,
			      (char *) server_name,
			      (char *) channel_name,
			      (char *) command);

    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_add_message_handler: add handler for messages
 */

static int
weechat_lua_add_message_handler (lua_State *L)
{
    const char *irc_command, *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to add message handler, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    irc_command = NULL;
    function = NULL;
    
    n = lua_gettop (lua_current_interpreter);

    if (n != 2)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"add_message_handler\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    irc_command = lua_tostring (lua_current_interpreter, -2);
    function = lua_tostring (lua_current_interpreter, -1);
    
    if (!lua_plugin->msg_handler_add (lua_plugin, (char *) irc_command,
				     weechat_lua_cmd_msg_handler,
                                      (char *) function,
				     (void *)lua_current_script))
    {
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_add_command_handler: define/redefines commands
 */

static int
weechat_lua_add_command_handler (lua_State *L)
{
    const char *command, *function, *description, *arguments, *arguments_description;
    const char *completion_template;
    int n;
    
    /* make C compiler happy */
    (void) L;
        
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to add command handler, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    command = NULL;
    function = NULL;
    description = NULL;
    arguments = NULL;
    arguments_description = NULL;
    completion_template = NULL;

    n = lua_gettop (lua_current_interpreter);
    
    switch (n)
    {
        case 2:
            command = lua_tostring (lua_current_interpreter, -2);
            function = lua_tostring (lua_current_interpreter, -1);
            break;
        case 3:
            command = lua_tostring (lua_current_interpreter, -3);
            function = lua_tostring (lua_current_interpreter, -2);
            description = lua_tostring (lua_current_interpreter, -1);
            break;
        case 4:
            command = lua_tostring (lua_current_interpreter, -4);
            function = lua_tostring (lua_current_interpreter, -3);
            description = lua_tostring (lua_current_interpreter, -2);
            arguments = lua_tostring (lua_current_interpreter, -1);
            break;
        case 5:
            command = lua_tostring (lua_current_interpreter, -5);
            function = lua_tostring (lua_current_interpreter, -4);
            description = lua_tostring (lua_current_interpreter, -3);
            arguments = lua_tostring (lua_current_interpreter, -2);
            arguments_description = lua_tostring (lua_current_interpreter, -1);
            break;
        case 6:
            command = lua_tostring (lua_current_interpreter, -6);
            function = lua_tostring (lua_current_interpreter, -5);
            description = lua_tostring (lua_current_interpreter, -4);
            arguments = lua_tostring (lua_current_interpreter, -3);
            arguments_description = lua_tostring (lua_current_interpreter, -2);
            completion_template = lua_tostring (lua_current_interpreter, -1);
            break;
        default:
            lua_plugin->print_server (lua_plugin,
                                      "Lua error: wrong parameters for "
                                      "\"add_command_handler\" function");
            lua_pushnumber (lua_current_interpreter, 0);
            return 1;
    }
    
    if (!lua_plugin->cmd_handler_add (lua_plugin,
				      (char *) command,
				      (char *) description,
				      (char *) arguments,
				      (char *) arguments_description,
				      (char *) completion_template,
				      weechat_lua_cmd_msg_handler,
				      (char *) function,
				      (void *)lua_current_script))
    {
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_add_timer_handler: add a timer handler
 */

static int
weechat_lua_add_timer_handler (lua_State *L)
{
    int interval;
    const char *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to add timer handler, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    interval = 10;
    function = NULL;
    
    n = lua_gettop (lua_current_interpreter);

    if (n != 2)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"add_timer_handler\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    interval = lua_tonumber (lua_current_interpreter, -2);
    function = lua_tostring (lua_current_interpreter, -1);
    
    if (!lua_plugin->timer_handler_add (lua_plugin, interval,
                                        weechat_lua_timer_handler,
                                        (char *) function,
                                        (void *)lua_current_script))
    {
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_add_keyboard_handler: add a keyboard handler
 */

static int
weechat_lua_add_keyboard_handler (lua_State *L)
{
    const char *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to add keyboard handler, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    function = NULL;
    
    n = lua_gettop (lua_current_interpreter);

    if (n != 1)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"add_keyboard_handler\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    function = lua_tostring (lua_current_interpreter, -1);
    
    if (!lua_plugin->keyboard_handler_add (lua_plugin,
                                           weechat_lua_keyboard_handler,
                                           (char *) function,
                                           (void *)lua_current_script))
    {
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_add_event_handler: add handler for events
 */

static int
weechat_lua_add_event_handler (lua_State *L)
{
    const char *event, *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to add event handler, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    event = NULL;
    function = NULL;
    
    n = lua_gettop (lua_current_interpreter);

    if (n != 2)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"add_event_handler\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    event = lua_tostring (lua_current_interpreter, -2);
    function = lua_tostring (lua_current_interpreter, -1);
    
    if (!lua_plugin->event_handler_add (lua_plugin, (char *) event,
                                        weechat_lua_event_handler,
                                        (char *) function,
                                        (void *)lua_current_script))
    {
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_remove_handler: remove a command/message handler
 */

static int
weechat_lua_remove_handler (lua_State *L)
{
    const char *command, *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to remove handler, "
                                  "script not initialized");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    command = NULL;
    function = NULL;
 
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 2)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"remove_handler\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    command = lua_tostring (lua_current_interpreter, -2);
    function = lua_tostring (lua_current_interpreter, -1);
    
    weechat_script_remove_handler (lua_plugin, lua_current_script,
                                   (char *) command, (char *) function);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_remove_timer_handler: remove a timer handler
 */

static int
weechat_lua_remove_timer_handler (lua_State *L)
{
    const char *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to remove timer handler, "
                                  "script not initialized");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    function = NULL;
 
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 1)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"remove_timer_handler\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    function = lua_tostring (lua_current_interpreter, -1);
    
    weechat_script_remove_timer_handler (lua_plugin, lua_current_script,
                                         (char *) function);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_remove_keyboard_handler: remove a keyboard handler
 */

static int
weechat_lua_remove_keyboard_handler (lua_State *L)
{
    const char *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to remove keyboard handler, "
                                  "script not initialized");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    function = NULL;
 
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 1)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"remove_keyboard_handler\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    function = lua_tostring (lua_current_interpreter, -1);
    
    weechat_script_remove_keyboard_handler (lua_plugin, lua_current_script,
                                            (char *) function);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_remove_event_handler: remove an event handler
 */

static int
weechat_lua_remove_event_handler (lua_State *L)
{
    const char *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to remove event handler, "
                                  "script not initialized");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    function = NULL;
 
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 1)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"remove_event_handler\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    function = lua_tostring (lua_current_interpreter, -1);
    
    weechat_script_remove_event_handler (lua_plugin, lua_current_script,
                                         (char *) function);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_add_modifier: add a modifier
 */

static int
weechat_lua_add_modifier (lua_State *L)
{
    const char *type, *command, *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to add modifier, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    type = NULL;
    command = NULL;
    function = NULL;
    
    n = lua_gettop (lua_current_interpreter);

    if (n != 3)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"add_modifier\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    type = lua_tostring (lua_current_interpreter, -3);
    command = lua_tostring (lua_current_interpreter, -2);
    function = lua_tostring (lua_current_interpreter, -1);
    
    if (!lua_plugin->modifier_add (lua_plugin, (char *)type, (char *)command,
                                   weechat_lua_modifier,
                                   (char *)function,
                                   (void *)lua_current_script))
    {
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_remove_modifier: remove a modifier
 */

static int
weechat_lua_remove_modifier (lua_State *L)
{
    const char *type, *command, *function;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to remove modifier, "
                                  "script not initialized");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    type = NULL;
    command = NULL;
    function = NULL;
 
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 3)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"remove_modifier\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }

    type = lua_tostring (lua_current_interpreter, -3);
    command = lua_tostring (lua_current_interpreter, -2);
    function = lua_tostring (lua_current_interpreter, -1);
    
    weechat_script_remove_modifier (lua_plugin, lua_current_script,
                                    (char *)type, (char *)command,
                                    (char *)function);
    
    lua_pushnumber (lua_current_interpreter, 1);
    return 1;
}

/*
 * weechat_lua_get_info: get various infos
 */

static int
weechat_lua_get_info (lua_State *L)
{
    const char *arg, *server_name;
    char *info;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get info, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    arg = NULL;
    server_name = NULL;
    
    n = lua_gettop (lua_current_interpreter);

    switch (n)
    {
        case 1:
            arg = lua_tostring (lua_current_interpreter, -1);
            break;
        case 2:
            arg = lua_tostring (lua_current_interpreter, -2);
            server_name = lua_tostring (lua_current_interpreter, -1);
            break;
        default:
            lua_plugin->print_server (lua_plugin,
                                      "Lua error: wrong parameters for "
                                      "\"get_info\" function");
            lua_pushnumber (lua_current_interpreter, 0);
            return 1;
    }

    info = lua_plugin->get_info (lua_plugin, (char *) arg, (char *) server_name);
    if (info)
	lua_pushstring (lua_current_interpreter, info);
    else
	lua_pushstring (lua_current_interpreter, "");
    
    return  1;
}

/*
 * weechat_lua_get_dcc_info: get infos about DCC
 */

static int
weechat_lua_get_dcc_info (lua_State *L)
{
    t_plugin_dcc_info *dcc_info, *ptr_dcc;
    char timebuffer1[64];
    char timebuffer2[64];
    struct in_addr in;
    int i;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get DCC info, "
                                  "script not initialized");
	lua_pushnil (lua_current_interpreter);
	return 1;
    }
    
    dcc_info = lua_plugin->get_dcc_info (lua_plugin);
    if (!dcc_info)
    {
	lua_pushboolean (lua_current_interpreter, 0);
	return 1;
    }
    
    lua_newtable (lua_current_interpreter);

    for (i = 0, ptr_dcc = dcc_info; ptr_dcc; ptr_dcc = ptr_dcc->next_dcc, i++)
    {
	strftime(timebuffer1, sizeof(timebuffer1), "%F %T",
		 localtime(&ptr_dcc->start_time));
	strftime(timebuffer2, sizeof(timebuffer2), "%F %T",
		 localtime(&ptr_dcc->start_transfer));
	in.s_addr = htonl(ptr_dcc->addr);
	
	lua_pushnumber (lua_current_interpreter, i);
	lua_newtable (lua_current_interpreter);

	lua_pushstring (lua_current_interpreter, "server");
	lua_pushstring (lua_current_interpreter, ptr_dcc->server);
	lua_rawset (lua_current_interpreter, -3);
		    
	lua_pushstring (lua_current_interpreter, "channel");
	lua_pushstring (lua_current_interpreter, ptr_dcc->channel);
	lua_rawset (lua_current_interpreter, -3);
		    
	lua_pushstring (lua_current_interpreter, "type");
	lua_pushnumber (lua_current_interpreter, ptr_dcc->type);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "status");
	lua_pushnumber (lua_current_interpreter, ptr_dcc->status);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "start_time");
	lua_pushstring (lua_current_interpreter, timebuffer1);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "start_transfer");
	lua_pushstring (lua_current_interpreter, timebuffer2);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "address");
	lua_pushstring (lua_current_interpreter, inet_ntoa(in));
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "port");
	lua_pushnumber (lua_current_interpreter, ptr_dcc->port);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "nick");
	lua_pushstring (lua_current_interpreter, ptr_dcc->nick);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "remote_file");
	lua_pushstring (lua_current_interpreter, ptr_dcc->filename);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "local_file");
	lua_pushstring (lua_current_interpreter, ptr_dcc->local_filename);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "filename_suffix");
	lua_pushnumber (lua_current_interpreter, ptr_dcc->filename_suffix);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "size");
	lua_pushnumber (lua_current_interpreter, ptr_dcc->size);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "pos");
	lua_pushnumber (lua_current_interpreter, ptr_dcc->pos);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "start_resume");
	lua_pushnumber (lua_current_interpreter, ptr_dcc->start_resume);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "cps");
	lua_pushnumber (lua_current_interpreter, ptr_dcc->bytes_per_sec);
	lua_rawset (lua_current_interpreter, -3);

	lua_rawset (lua_current_interpreter, -3);
    }
    
    lua_plugin->free_dcc_info (lua_plugin, dcc_info);
    
    return 1;
}

/*
 * weechat_lua_get_config: get value of a WeeChat config option
 */

static int
weechat_lua_get_config (lua_State *L)
{
    const char *option;
    char *return_value;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get config option, "
                                  "script not initialized");	
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    option = NULL;
 
    n = lua_gettop (lua_current_interpreter);

    if (n != 1)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"get_config\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    option = lua_tostring (lua_current_interpreter, -1);
    
    return_value = lua_plugin->get_config (lua_plugin, (char *) option);    
    if (return_value)
	lua_pushstring (lua_current_interpreter, return_value);
    else
	lua_pushstring (lua_current_interpreter, "");
    
    return 1;
}

/*
 * weechat_lua_set_config: set value of a WeeChat config option
 */

static int
weechat_lua_set_config (lua_State *L)
{
    const char *option, *value;
    int n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to set config option, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    option = NULL;
    value = NULL;
    
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 2)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"set_config\" function");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    option = lua_tostring (lua_current_interpreter, -2);
    value = lua_tostring (lua_current_interpreter, -1);

    if (lua_plugin->set_config (lua_plugin, (char *) option, (char *) value))
	lua_pushnumber (lua_current_interpreter, 1);
    else
	lua_pushnumber (lua_current_interpreter, 0);
    
    return 1;
}

/*
 * weechat_lua_get_plugin_config: get value of a plugin config option
 */

static int
weechat_lua_get_plugin_config (lua_State *L)
{
    const char *option;
    char *return_value;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get plugin config option, "
                                  "script not initialized");	
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    option = NULL;
 
    n = lua_gettop (lua_current_interpreter);

    if (n != 1)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"get_plugin_config\" function");
        lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    option = lua_tostring (lua_current_interpreter, -1);
    
    return_value = weechat_script_get_plugin_config (lua_plugin,
						     lua_current_script,
						     (char *) option);
    if (return_value)
	lua_pushstring (lua_current_interpreter, return_value);
    else
	lua_pushstring (lua_current_interpreter, "");
    
    return 1;
}

/*
 * weechat_lua_set_plugin_config: set value of a plugin config option
 */

static int
weechat_lua_set_plugin_config (lua_State *L)
{
    const char *option, *value;
    int n;
    
    /* make C compiler happy */
    (void) L;
 	
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to set plugin config option, "
                                  "script not initialized");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    option = NULL;
    value = NULL;
    
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 2)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"set_plugin_config\" function");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    option = lua_tostring (lua_current_interpreter, -2);
    value = lua_tostring (lua_current_interpreter, -1);

    if (weechat_script_set_plugin_config (lua_plugin,
					  lua_current_script,
					  (char *) option, (char *) value))
	lua_pushnumber (lua_current_interpreter, 1);
    else
	lua_pushnumber (lua_current_interpreter, 0);
    
    return 1;
}

/*
 * weechat_lua_get_server_info: get infos about servers
 */

static int
weechat_lua_get_server_info (lua_State *L)
{
    t_plugin_server_info *server_info, *ptr_server;
    char timebuffer[64];
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get server infos, "
                                  "script not initialized");
	lua_pushnil (lua_current_interpreter);
	return 1;
    }
    
    server_info = lua_plugin->get_server_info (lua_plugin);
    if  (!server_info) {
	lua_pushboolean (lua_current_interpreter, 0);
	return 1;
    }

    lua_newtable (lua_current_interpreter);

    for (ptr_server = server_info; ptr_server; ptr_server = ptr_server->next_server)
    {
	strftime(timebuffer, sizeof(timebuffer), "%F %T",
		 localtime(&ptr_server->away_time));
	
	lua_pushstring (lua_current_interpreter, ptr_server->name);
	lua_newtable (lua_current_interpreter);
	
	lua_pushstring (lua_current_interpreter, "autoconnect");
	lua_pushnumber (lua_current_interpreter, ptr_server->autoconnect);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "autoreconnect");
	lua_pushnumber (lua_current_interpreter, ptr_server->autoreconnect);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "autoreconnect_delay");
	lua_pushnumber (lua_current_interpreter, ptr_server->autoreconnect_delay);
	lua_rawset (lua_current_interpreter, -3);
		
	lua_pushstring (lua_current_interpreter, "temp_server");
	lua_pushnumber (lua_current_interpreter, ptr_server->temp_server);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "address");
	lua_pushstring (lua_current_interpreter, ptr_server->address);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "port");
	lua_pushnumber (lua_current_interpreter, ptr_server->port);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "ipv6");
	lua_pushnumber (lua_current_interpreter, ptr_server->ipv6);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "ssl");
	lua_pushnumber (lua_current_interpreter, ptr_server->ssl);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "password");
	lua_pushstring (lua_current_interpreter, ptr_server->password);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "nick1");
	lua_pushstring (lua_current_interpreter, ptr_server->nick1);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "nick2");
	lua_pushstring (lua_current_interpreter, ptr_server->nick2);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "nick3");
	lua_pushstring (lua_current_interpreter, ptr_server->nick3);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "username");
	lua_pushstring (lua_current_interpreter, ptr_server->username);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "realname");
	lua_pushstring (lua_current_interpreter, ptr_server->realname);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "command");
	lua_pushstring (lua_current_interpreter, ptr_server->command);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "command_delay");
	lua_pushnumber (lua_current_interpreter, ptr_server->command_delay);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "autojoin");
	lua_pushstring (lua_current_interpreter, ptr_server->autojoin);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "autorejoin");
	lua_pushnumber (lua_current_interpreter, ptr_server->autorejoin);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "notify_levels");
	lua_pushstring (lua_current_interpreter, ptr_server->notify_levels);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "is_connected");
	lua_pushnumber (lua_current_interpreter, ptr_server->is_connected);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "ssl_connected");
	lua_pushnumber (lua_current_interpreter, ptr_server->ssl_connected);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "nick");
	lua_pushstring (lua_current_interpreter, ptr_server->nick);
	lua_rawset (lua_current_interpreter, -3);
        
        lua_pushstring (lua_current_interpreter, "nick_modes");
	lua_pushstring (lua_current_interpreter, ptr_server->nick_modes);
	lua_rawset (lua_current_interpreter, -3);
        
	lua_pushstring (lua_current_interpreter, "away_time");
	lua_pushstring (lua_current_interpreter, timebuffer);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "lag");
	lua_pushnumber (lua_current_interpreter, ptr_server->lag);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_rawset (lua_current_interpreter, -3);
    }

    lua_plugin->free_server_info(lua_plugin, server_info);
    
    return 1;
}

/*
 * weechat_lua_get_channel_info: get infos about channels
 */

static int
weechat_lua_get_channel_info (lua_State *L)
{
    t_plugin_channel_info *channel_info, *ptr_channel;
    const char *server;
    int n;
    
    /* make C compiler happy */
    (void) L;
 
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get channel infos, "
                                  "script not initialized");
	lua_pushnil (lua_current_interpreter);
	return 1;
    }
    
    server = NULL;
    
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 1)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"get_channel_info\" function");
        lua_pushnil (lua_current_interpreter);
	return 1;
    }

    server = lua_tostring (lua_current_interpreter, -1);
    
    channel_info = lua_plugin->get_channel_info (lua_plugin, (char *) server);
    if  (!channel_info)
    {
	lua_pushboolean (lua_current_interpreter, 0);
	return 1;
    }

    lua_newtable (lua_current_interpreter);

    for (ptr_channel = channel_info; ptr_channel; ptr_channel = ptr_channel->next_channel)
    {
	lua_pushstring (lua_current_interpreter, ptr_channel->name);
	lua_newtable (lua_current_interpreter);
	
	lua_pushstring (lua_current_interpreter, "type");
	lua_pushnumber (lua_current_interpreter, ptr_channel->type);
	lua_rawset (lua_current_interpreter, -3);

	lua_pushstring (lua_current_interpreter, "topic");
	lua_pushstring (lua_current_interpreter, ptr_channel->topic);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "modes");
	lua_pushstring (lua_current_interpreter, ptr_channel->modes);
	lua_rawset (lua_current_interpreter, -3);

	lua_pushstring (lua_current_interpreter, "limit");
	lua_pushnumber (lua_current_interpreter, ptr_channel->limit);
	lua_rawset (lua_current_interpreter, -3);

	lua_pushstring (lua_current_interpreter, "key");
	lua_pushstring (lua_current_interpreter, ptr_channel->key);
	lua_rawset (lua_current_interpreter, -3);

	lua_pushstring (lua_current_interpreter, "nicks_count");
	lua_pushnumber (lua_current_interpreter, ptr_channel->nicks_count);
	lua_rawset (lua_current_interpreter, -3);

	lua_rawset (lua_current_interpreter, -3);
    }    

    lua_plugin->free_channel_info(lua_plugin, channel_info);
    
    return 1;
}

/*
 * weechat_lua_get_nick_info: get infos about nicks
 */

static int
weechat_lua_get_nick_info (lua_State *L)
{
    t_plugin_nick_info *nick_info, *ptr_nick;
    const char *server, *channel;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get nick infos, "
                                  "script not initialized");
		lua_pushnil (lua_current_interpreter);
	return 1;
    }
    
    server = NULL;
    channel = NULL;
    
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 2)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"get_nick_info\" function");
        lua_pushnil (lua_current_interpreter);
	return 1;
    }

    server = lua_tostring (lua_current_interpreter, -2);
    channel = lua_tostring (lua_current_interpreter, -1);
    
    nick_info = lua_plugin->get_nick_info (lua_plugin, (char *) server, (char *) channel);
    if  (!nick_info)
    {
	lua_pushboolean (lua_current_interpreter, 0);
	return 1;
    }

    lua_newtable (lua_current_interpreter);
    
    for(ptr_nick = nick_info; ptr_nick; ptr_nick = ptr_nick->next_nick)
    {
	lua_pushstring (lua_current_interpreter, ptr_nick->nick);
	lua_newtable (lua_current_interpreter);
	
	lua_pushstring (lua_current_interpreter, "flags");
	lua_pushnumber (lua_current_interpreter, ptr_nick->flags);
	lua_rawset (lua_current_interpreter, -3);

	lua_pushstring (lua_current_interpreter, "host");
	lua_pushstring (lua_current_interpreter,
			ptr_nick->host ? ptr_nick->host : "");
	lua_rawset (lua_current_interpreter, -3);
	
	lua_rawset (lua_current_interpreter, -3);
    }
    
    lua_plugin->free_nick_info(lua_plugin, nick_info);
    
    return 1;
}

/*
 * weechat_lua_get_irc_color:
 *          get the numeric value which identify an irc color by its name
 */

static int
weechat_lua_get_irc_color (lua_State *L)
{
    const char *color;
    int n;
    
    /* make C compiler happy */
    (void) L;
     
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get irc color, "
                                  "script not initialized");
        lua_pushnumber (lua_current_interpreter, -1);
	return 1;
    }
    
    color = NULL;
 
    n = lua_gettop (lua_current_interpreter);
    
    if (n != 1)
    {
	lua_plugin->print_server (lua_plugin,
                                  "Lua error: wrong parameters for "
                                  "\"get_irc_color\" function");
        lua_pushnumber (lua_current_interpreter, -1);
	return 1;
    }

    color = lua_tostring (lua_current_interpreter, -1);
    
    lua_pushnumber (lua_current_interpreter,
		    lua_plugin->get_irc_color (lua_plugin, (char *) color));
    return 1;
}

/*
 * weechat_lua_get_window_info: get infos about windows
 */

static int
weechat_lua_get_window_info (lua_State *L)
{
    t_plugin_window_info *window_info, *ptr_win;
    int i;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get window info, "
                                  "script not initialized");
	lua_pushnil (lua_current_interpreter);
	return 1;
    }
    
    window_info = lua_plugin->get_window_info (lua_plugin);
    if (!window_info)
    {
	lua_pushboolean (lua_current_interpreter, 0);
	return 1;
    }
    
    lua_newtable (lua_current_interpreter);

    i = 0;
    for (ptr_win = window_info; ptr_win; ptr_win = ptr_win->next_window)
    {
	lua_pushnumber (lua_current_interpreter, i);
	lua_newtable (lua_current_interpreter);

	lua_pushstring (lua_current_interpreter, "num_buffer");
	lua_pushnumber (lua_current_interpreter, ptr_win->num_buffer);
	lua_rawset (lua_current_interpreter, -3);
		    
	lua_pushstring (lua_current_interpreter, "win_x");
	lua_pushnumber (lua_current_interpreter, ptr_win->win_x);
	lua_rawset (lua_current_interpreter, -3);
		    
	lua_pushstring (lua_current_interpreter, "win_y");
	lua_pushnumber (lua_current_interpreter, ptr_win->win_y);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "win_width");
	lua_pushnumber (lua_current_interpreter, ptr_win->win_width);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "win_height");
	lua_pushnumber (lua_current_interpreter, ptr_win->win_height);
	lua_rawset (lua_current_interpreter, -3);

	lua_pushstring (lua_current_interpreter, "win_width_pct");
	lua_pushnumber (lua_current_interpreter, ptr_win->win_width_pct);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "win_height_pct");
	lua_pushnumber (lua_current_interpreter, ptr_win->win_height_pct);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_rawset (lua_current_interpreter, -3);
        
        i++;
    }
    
    lua_plugin->free_window_info (lua_plugin, window_info);
    
    return 1;
}

/*
 * weechat_lua_get_buffer_info: get infos about buffers
 */

static int
weechat_lua_get_buffer_info (lua_State *L)
{
    t_plugin_buffer_info *buffer_info, *ptr_buffer;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get buffer info, "
                                  "script not initialized");
	lua_pushnil (lua_current_interpreter);
	return 1;
    }
    
    buffer_info = lua_plugin->get_buffer_info (lua_plugin);
    if  (!buffer_info) {
	lua_pushboolean (lua_current_interpreter, 0);
	return 1;
    }

    lua_newtable (lua_current_interpreter);

    for (ptr_buffer = buffer_info; ptr_buffer; ptr_buffer = ptr_buffer->next_buffer)
    {
	lua_pushnumber (lua_current_interpreter, ptr_buffer->number);
	lua_newtable (lua_current_interpreter);
	
	lua_pushstring (lua_current_interpreter, "type");
	lua_pushnumber (lua_current_interpreter, ptr_buffer->type);
	lua_rawset (lua_current_interpreter, -3);
        
	lua_pushstring (lua_current_interpreter, "num_displayed");
	lua_pushnumber (lua_current_interpreter, ptr_buffer->num_displayed);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "server");
	lua_pushstring (lua_current_interpreter, 
			ptr_buffer->server_name == NULL ? "" : ptr_buffer->server_name);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "channel");
	lua_pushstring (lua_current_interpreter, 
			ptr_buffer->channel_name == NULL ? "" : ptr_buffer->channel_name);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "notify_level");
	lua_pushnumber (lua_current_interpreter, ptr_buffer->notify_level);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_pushstring (lua_current_interpreter, "log_filename");
	lua_pushstring (lua_current_interpreter, 
			ptr_buffer->log_filename == NULL ? "" : ptr_buffer->log_filename);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_rawset (lua_current_interpreter, -3);
    }
    
    lua_plugin->free_buffer_info(lua_plugin, buffer_info);
    
    return 1;
}

/*
 * weechat_lua_get_buffer_data: get buffer content
 */

static int
weechat_lua_get_buffer_data (lua_State *L)
{
    t_plugin_buffer_line *buffer_data, *ptr_data;
    const char *server, *channel;
    char timebuffer[64];
    int i, n;
    
    /* make C compiler happy */
    (void) L;
    
    if (!lua_current_script)
    {
        lua_plugin->print_server (lua_plugin,
                                  "Lua error: unable to get buffer data, "
                                  "script not initialized");
	lua_pushnil (lua_current_interpreter);
	return 1;
    }

    server = NULL;
    channel = NULL;
    
    n = lua_gettop (lua_current_interpreter);
    if (n != 2)
    {
	lua_plugin->print_server (lua_plugin,
				  "Lua error: wrong parameters for "
				  "\"get_buffer_data\" function");
	lua_pushnumber (lua_current_interpreter, 0);
	return 1;
    }
    
    server  = lua_tostring (lua_current_interpreter, -2);
    channel = lua_tostring (lua_current_interpreter, -1);
    
    buffer_data = lua_plugin->get_buffer_data (lua_plugin, (char *) server, (char *) channel);
    if (!buffer_data)
    {
	lua_pushboolean (lua_current_interpreter, 0);
	return 1;
    }
    
    lua_newtable (lua_current_interpreter);

    for (i = 0, ptr_data = buffer_data; ptr_data; ptr_data = ptr_data->next_line, i++)
    {
	lua_pushnumber (lua_current_interpreter, i);
	lua_newtable (lua_current_interpreter);

	strftime(timebuffer, sizeof(timebuffer), "%F %T",
		 localtime(&ptr_data->date));

	lua_pushstring (lua_current_interpreter, "date");
	lua_pushstring (lua_current_interpreter, timebuffer);
	lua_rawset (lua_current_interpreter, -3);

	lua_pushstring (lua_current_interpreter, "nick");
	lua_pushstring (lua_current_interpreter,
			ptr_data->nick == NULL ? "" : ptr_data->nick);
	lua_rawset (lua_current_interpreter, -3);
		    
	lua_pushstring (lua_current_interpreter, "data");
	lua_pushstring (lua_current_interpreter,
			ptr_data->data == NULL ? "" : ptr_data->data);
	lua_rawset (lua_current_interpreter, -3);
	
	lua_rawset (lua_current_interpreter, -3);
    }
    
    lua_plugin->free_buffer_data (lua_plugin, buffer_data);
    
    return 1;
}

/*
 * Lua constant as functions
 */

static int
weechat_lua_constant_plugin_rc_ok (lua_State *L)
{
    /* make C compiler happy */
    (void) L;
    
    lua_pushnumber (lua_current_interpreter, PLUGIN_RC_OK);
    return 1;
}

static int
weechat_lua_constant_plugin_rc_ko (lua_State *L)
{
    /* make C compiler happy */
    (void) L;
    
    lua_pushnumber (lua_current_interpreter, PLUGIN_RC_KO);
    return 1;
}
    
static int
weechat_lua_constant_plugin_rc_ok_ignore_weechat (lua_State *L)
{
    /* make C compiler happy */
    (void) L;
    
    lua_pushnumber (lua_current_interpreter, PLUGIN_RC_OK_IGNORE_WEECHAT);
    return 1;
}

static int
weechat_lua_constant_plugin_rc_ok_ignore_plugins (lua_State *L)
{
    /* make C compiler happy */
    (void) L;
    
    lua_pushnumber (lua_current_interpreter, PLUGIN_RC_OK_IGNORE_PLUGINS);
    return 1;
}

static int
weechat_lua_constant_plugin_rc_ok_ignore_all (lua_State *L)
{
    /* make C compiler happy */
    (void) L;
    
    lua_pushnumber (lua_current_interpreter, PLUGIN_RC_OK_IGNORE_ALL);
    return 1;
}

static int
weechat_lua_constant_plugin_rc_ok_with_highlight (lua_State *L)
{
    /* make C compiler happy */
    (void) L;
    
    lua_pushnumber (lua_current_interpreter, PLUGIN_RC_OK_WITH_HIGHLIGHT);
    return 1;
}

/*
 * Lua subroutines
 */

static
const struct luaL_reg weechat_lua_funcs[] = {
    { "register", weechat_lua_register },
    { "set_charset", weechat_lua_set_charset },
    { "print", weechat_lua_print },
    { "print_server", weechat_lua_print_server },
    { "print_infobar", weechat_lua_print_infobar },
    { "remove_infobar", weechat_lua_remove_infobar },
    { "log", weechat_lua_log },
    { "command", weechat_lua_command },
    { "add_message_handler", weechat_lua_add_message_handler },
    { "add_command_handler", weechat_lua_add_command_handler },
    { "add_timer_handler", weechat_lua_add_timer_handler },
    { "add_keyboard_handler", weechat_lua_add_keyboard_handler },
    { "add_event_handler", weechat_lua_add_event_handler },
    { "remove_handler", weechat_lua_remove_handler },
    { "remove_timer_handler", weechat_lua_remove_timer_handler },
    { "remove_keyboard_handler", weechat_lua_remove_keyboard_handler },
    { "remove_event_handler", weechat_lua_remove_event_handler },
    { "add_modifier", weechat_lua_add_modifier },
    { "remove_modifier", weechat_lua_remove_modifier },
    { "get_info", weechat_lua_get_info },
    { "get_dcc_info", weechat_lua_get_dcc_info },
    { "get_config", weechat_lua_get_config },
    { "set_config", weechat_lua_set_config },
    { "get_plugin_config", weechat_lua_get_plugin_config },
    { "set_plugin_config", weechat_lua_set_plugin_config },
    { "get_server_info", weechat_lua_get_server_info },
    { "get_channel_info", weechat_lua_get_channel_info },
    { "get_nick_info", weechat_lua_get_nick_info },
    { "get_irc_color", weechat_lua_get_irc_color },
    { "get_window_info", weechat_lua_get_window_info },
    { "get_buffer_info", weechat_lua_get_buffer_info },
    { "get_buffer_data", weechat_lua_get_buffer_data },
    /* define constants as function which returns values */
    { "PLUGIN_RC_OK", weechat_lua_constant_plugin_rc_ok },
    { "PLUGIN_RC_KO", weechat_lua_constant_plugin_rc_ko },
    { "PLUGIN_RC_OK_IGNORE_WEECHAT", weechat_lua_constant_plugin_rc_ok_ignore_weechat },
    { "PLUGIN_RC_OK_IGNORE_PLUGINS", weechat_lua_constant_plugin_rc_ok_ignore_plugins },
    { "PLUGIN_RC_OK_IGNORE_ALL", weechat_lua_constant_plugin_rc_ok_ignore_all },
    { "PLUGIN_RC_OK_WITH_HIGHLIGHT", weechat_lua_constant_plugin_rc_ok_with_highlight },
    { NULL, NULL }
};

int
weechat_lua_load (t_weechat_plugin *plugin, char *filename)
{
    FILE *fp;
    char *weechat_lua_code = {
	"weechat_outputs = {\n"
	"    write = function (self, str)\n"
	"        weechat.print(\"Lua stdout/stderr : \" .. str)\n"
        "    end\n"
	"}\n"
	"io.stdout = weechat_outputs\n"
	"io.stderr = weechat_outputs\n"
    };
    
    plugin->print_server (plugin, "Loading Lua script \"%s\"", filename);
    
    if ((fp = fopen (filename, "r")) == NULL)
    {
        plugin->print_server (plugin, "Lua error: script \"%s\" not found",
                              filename);
        return 0;
    }
    
    lua_current_script = NULL;
    
    lua_current_interpreter = lua_open ();

    if (lua_current_interpreter == NULL)
    {
        plugin->print_server (plugin,
                              "Lua error: unable to create new sub-interpreter");
        fclose (fp);
        return 0;
    }

#ifdef LUA_VERSION_NUM /* LUA_VERSION_NUM is defined only in lua >= 5.1.0 */
    luaL_openlibs (lua_current_interpreter);
#else
    luaopen_base (lua_current_interpreter);
    luaopen_string (lua_current_interpreter);
    luaopen_table (lua_current_interpreter);
    luaopen_math (lua_current_interpreter);
    luaopen_io (lua_current_interpreter);
    luaopen_debug (lua_current_interpreter);
#endif
    
    luaL_openlib (lua_current_interpreter, "weechat", weechat_lua_funcs, 0);

#ifdef LUA_VERSION_NUM
    if (luaL_dostring (lua_current_interpreter, weechat_lua_code) != 0)
#else    
    if (lua_dostring (lua_current_interpreter, weechat_lua_code) != 0)
#endif
        plugin->print_server (plugin,
                              "Lua warning: unable to redirect stdout and stderr");
    
    lua_current_script_filename = filename;
    
    if (luaL_loadfile (lua_current_interpreter, filename) != 0)
    {
        plugin->print_server (plugin,
                              "Lua error: unable to load file \"%s\"",
                              filename);
	plugin->print_server (plugin,
                              "Lua error: %s",
                              lua_tostring (lua_current_interpreter, -1));
        lua_close (lua_current_interpreter);
        fclose (fp);
        return 0;
    }

    if (lua_pcall (lua_current_interpreter, 0, 0, 0) != 0)
    {
        plugin->print_server (plugin,
                              "Lua error: unable to execute file \"%s\"",
                              filename);
	plugin->print_server (plugin,
                              "Lua error: %s",
                              lua_tostring (lua_current_interpreter, -1));
        lua_close (lua_current_interpreter);
        fclose (fp);
	/* if script was registered, removing from list */
	if (lua_current_script != NULL)
	    weechat_script_remove (plugin, &lua_scripts, lua_current_script);
        return 0;
    }
    fclose (fp);
    
    if (lua_current_script == NULL)
    {
        plugin->print_server (plugin,
                              "Lua error: function \"register\" not found "
                              "(or failed) in file \"%s\"",
                              filename);
	lua_close (lua_current_interpreter);
        return 0;
    }
    
    lua_current_script->interpreter = (lua_State *) lua_current_interpreter;
    
    return 1;
}

/*
 * weechat_lua_unload: unload a Lua script
 */

void
weechat_lua_unload (t_weechat_plugin *plugin, t_plugin_script *script)
{
    int *r;
    
    plugin->print_server (plugin,
                          "Unloading Lua script \"%s\"",
                          script->name);
    
    if (script->shutdown_func[0])
    {
        r = weechat_lua_exec (plugin, script, SCRIPT_EXEC_INT,
			      script->shutdown_func, NULL, NULL, NULL);
	if (r)
	    free (r);
    }
    
    lua_close (script->interpreter);
    
    weechat_script_remove (plugin, &lua_scripts, script);
}

/*
 * weechat_lua_unload_name: unload a Lua script by name
 */

void
weechat_lua_unload_name (t_weechat_plugin *plugin, char *name)
{
    t_plugin_script *ptr_script;
    
    ptr_script = weechat_script_search (plugin, &lua_scripts, name);
    if (ptr_script)
    {
        weechat_lua_unload (plugin, ptr_script);
        plugin->print_server (plugin,
                              "Lua script \"%s\" unloaded",
                              name);
    }
    else
    {
        plugin->print_server (plugin,
                              "Lua error: script \"%s\" not loaded",
                              name);
    }
}

/*
 * weechat_lua_unload_all: unload all Lua scripts
 */

void
weechat_lua_unload_all (t_weechat_plugin *plugin)
{
    plugin->print_server (plugin,
                          "Unloading all Lua scripts");
    while (lua_scripts)
        weechat_lua_unload (plugin, lua_scripts);

    plugin->print_server (plugin,
                          "Lua scripts unloaded");
}

/*
 * weechat_lua_cmd: /lua command handler
 */

int
weechat_lua_cmd (t_weechat_plugin *plugin,
                 int cmd_argc, char **cmd_argv,
                 char *handler_args, void *handler_pointer)
{
    int argc, handler_found, modifier_found;
    char **argv, *path_script;
    t_plugin_script *ptr_script;
    t_plugin_handler *ptr_handler;
    t_plugin_modifier *ptr_modifier;

    /* make C compiler happy */
    (void) handler_args;
    (void) handler_pointer;
    
    if (cmd_argc < 3)
        return PLUGIN_RC_KO;
    
    if (cmd_argv[2])
        argv = plugin->explode_string (plugin, cmd_argv[2], " ", 0, &argc);
    else
    {
        argv = NULL;
        argc = 0;
    }
    
    switch (argc)
    {
        case 0:
            /* list registered Lua scripts */
            plugin->print_server (plugin, "");
            plugin->print_server (plugin, "Registered Lua scripts:");
            if (lua_scripts)
            {
                for (ptr_script = lua_scripts;
                     ptr_script; ptr_script = ptr_script->next_script)
                {
                    plugin->print_server (plugin, "  %s v%s%s%s",
                                          ptr_script->name,
                                          ptr_script->version,
                                          (ptr_script->description[0]) ? " - " : "",
                                          ptr_script->description);
                }
            }
            else
                plugin->print_server (plugin, "  (none)");
            
            /* list Lua message handlers */
            plugin->print_server (plugin, "");
            plugin->print_server (plugin, "Lua message handlers:");
            handler_found = 0;
            for (ptr_handler = plugin->handlers;
                 ptr_handler; ptr_handler = ptr_handler->next_handler)
            {
                if ((ptr_handler->type == PLUGIN_HANDLER_MESSAGE)
                    && (ptr_handler->handler_args))
                {
                    handler_found = 1;
                    plugin->print_server (plugin, "  IRC(%s) => Lua(%s)",
                                          ptr_handler->irc_command,
                                          ptr_handler->handler_args);
                }
            }
            if (!handler_found)
                plugin->print_server (plugin, "  (none)");
            
            /* list Lua command handlers */
            plugin->print_server (plugin, "");
            plugin->print_server (plugin, "Lua command handlers:");
            handler_found = 0;
            for (ptr_handler = plugin->handlers;
                 ptr_handler; ptr_handler = ptr_handler->next_handler)
            {
                if ((ptr_handler->type == PLUGIN_HANDLER_COMMAND)
                    && (ptr_handler->handler_args))
                {
                    handler_found = 1;
                    plugin->print_server (plugin, "  /%s => Lua(%s)",
                                          ptr_handler->command,
                                          ptr_handler->handler_args);
                }
            }
            if (!handler_found)
                plugin->print_server (plugin, "  (none)");
            
            /* list Lua timer handlers */
            plugin->print_server (plugin, "");
            plugin->print_server (plugin, "Lua timer handlers:");
            handler_found = 0;
            for (ptr_handler = plugin->handlers;
                 ptr_handler; ptr_handler = ptr_handler->next_handler)
            {
                if ((ptr_handler->type == PLUGIN_HANDLER_TIMER)
                    && (ptr_handler->handler_args))
                {
                    handler_found = 1;
                    plugin->print_server (plugin, "  %d seconds => Lua(%s)",
                                          ptr_handler->interval,
                                          ptr_handler->handler_args);
                }
            }
            if (!handler_found)
                plugin->print_server (plugin, "  (none)");
            
            /* list Lua keyboard handlers */
            plugin->print_server (plugin, "");
            plugin->print_server (plugin, "Lua keyboard handlers:");
            handler_found = 0;
            for (ptr_handler = plugin->handlers;
                 ptr_handler; ptr_handler = ptr_handler->next_handler)
            {
                if ((ptr_handler->type == PLUGIN_HANDLER_KEYBOARD)
                    && (ptr_handler->handler_args))
                {
                    handler_found = 1;
                    plugin->print_server (plugin, "  Lua(%s)",
                                          ptr_handler->handler_args);
                }
            }
            if (!handler_found)
                plugin->print_server (plugin, "  (none)");
            
            /* list Lua event handlers */
            plugin->print_server (plugin, "");
            plugin->print_server (plugin, "Lua event handlers:");
            handler_found = 0;
            for (ptr_handler = plugin->handlers;
                 ptr_handler; ptr_handler = ptr_handler->next_handler)
            {
                if ((ptr_handler->type == PLUGIN_HANDLER_EVENT)
                    && (ptr_handler->handler_args))
                {
                    handler_found = 1;
                    plugin->print_server (plugin, "  %s => Lua(%s)",
                                          ptr_handler->event,
                                          ptr_handler->handler_args);
                }
            }
            if (!handler_found)
                plugin->print_server (plugin, "  (none)");
            
	    /* list Lua modifiers */
	    plugin->print_server (plugin, "");
            plugin->print_server (plugin, "Lua modifiers:");
            modifier_found = 0;
            for (ptr_modifier = plugin->modifiers;
                 ptr_modifier; ptr_modifier = ptr_modifier->next_modifier)
            {
		modifier_found = 1;
		if (ptr_modifier->type == PLUGIN_MODIFIER_IRC_IN)
		    plugin->print_server (plugin, "  IRC(%s, %s) => Lua(%s)",
					  ptr_modifier->command,
					  PLUGIN_MODIFIER_IRC_IN_STR,
					  ptr_modifier->modifier_args);
		else if (ptr_modifier->type == PLUGIN_MODIFIER_IRC_USER)
		    plugin->print_server (plugin, "  IRC(%s, %s) => Lua(%s)",
					  ptr_modifier->command,
					  PLUGIN_MODIFIER_IRC_USER_STR,
					  ptr_modifier->modifier_args);
		else if (ptr_modifier->type == PLUGIN_MODIFIER_IRC_OUT)
		    plugin->print_server (plugin, "  IRC(%s, %s) => Lua(%s)",
					  ptr_modifier->command,
					  PLUGIN_MODIFIER_IRC_OUT_STR,
					  ptr_modifier->modifier_args);
            }
            if (!modifier_found)
                plugin->print_server (plugin, "  (none)");
	    break;
        case 1:
            if (plugin->ascii_strcasecmp (plugin, argv[0], "autoload") == 0)
                weechat_script_auto_load (plugin, "lua", weechat_lua_load);
            else if (plugin->ascii_strcasecmp (plugin, argv[0], "reload") == 0)
            {
                weechat_lua_unload_all (plugin);
                weechat_script_auto_load (plugin, "lua", weechat_lua_load);
            }
            else if (plugin->ascii_strcasecmp (plugin, argv[0], "unload") == 0)
                weechat_lua_unload_all (plugin);
            break;
        case 2:
            if (plugin->ascii_strcasecmp (plugin, argv[0], "load") == 0)
            {
                /* load Lua script */
                path_script = weechat_script_search_full_name (plugin, "lua", argv[1]);
                weechat_lua_load (plugin, (path_script) ? path_script : argv[1]);
                if (path_script)
                    free (path_script);
            }
            else if (plugin->ascii_strcasecmp (plugin, argv[0], "unload") == 0)
            {
                /* unload Lua script */
                weechat_lua_unload_name (plugin, argv[1]);
            }
            else
            {
                plugin->print_server (plugin,
                                      "Lua error: unknown option for "
                                      "\"lua\" command");
            }
            break;
        default:
            plugin->print_server (plugin,
                                  "Lua error: wrong argument count for \"lua\" command");
    }
    
    if (argv)
        plugin->free_exploded_string (plugin, argv);
    
    return PLUGIN_RC_OK;
}

/*
 * weechat_plugin_init: initialize Lua plugin
 */

int
weechat_plugin_init (t_weechat_plugin *plugin)
{
    
    lua_plugin = plugin;
    
    plugin->print_server (plugin, "Loading Lua module \"weechat\"");
        
    plugin->cmd_handler_add (plugin, "lua",
                             "list/load/unload Lua scripts",
                             "[load filename] | [autoload] | [reload] | [unload [script]]",
                             "filename: Lua script (file) to load\n"
                             "script: script name to unload\n\n"
                             "Without argument, /lua command lists all loaded Lua scripts.",
                             "load|autoload|reload|unload %f",
                             weechat_lua_cmd, NULL, NULL);

    plugin->mkdir_home (plugin, "lua");
    plugin->mkdir_home (plugin, "lua/autoload");
    
    weechat_script_auto_load (plugin, "lua", weechat_lua_load);
    
    /* init ok */
    return PLUGIN_RC_OK;
}

/*
 * weechat_plugin_end: shutdown Lua interface
 */

void
weechat_plugin_end (t_weechat_plugin *plugin)
{
    /* unload all scripts */
    weechat_lua_unload_all (plugin);
    
    lua_plugin->print_server (lua_plugin,
                              "Lua plugin ended");
}