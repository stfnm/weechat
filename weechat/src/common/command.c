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

/* command.c: WeeChat internal commands */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "weechat.h"
#include "command.h"
#include "alias.h"
#include "fifo.h"
#include "log.h"
#include "session.h"
#include "utf8.h"
#include "util.h"
#include "weelist.h"
#include "weeconfig.h"
#include "../irc/irc.h"
#include "../gui/gui.h"

#ifdef PLUGINS
#include "../plugins/plugins.h"
#endif


/* WeeChat internal commands */

t_weechat_command weechat_commands[] =
{ { "alias", N_("create an alias for a command"),
    N_("[alias_name [command [arguments]]]"),
    N_("alias_name: name of alias\n"
       "   command: command name (WeeChat or IRC command, many commands "
       "can be separated by semicolons)\n"
       " arguments: arguments for command\n\n"
        "Note: in command, special variables $1, $2,..,$9 are replaced by "
       "arguments given by user, and $* is replaced by all arguments.\n"
       "Variables $nick, $channel and $server are replaced by current "
       "nick/channel/server."),
    "%- %A", 0, MAX_ARGS, 1, NULL, weechat_cmd_alias },
  { "buffer", N_("manage buffers"),
    N_("[action [args] | number | [[server] [channel]]]"),
    N_(" action: action to do:\n"
       "   move: move buffer in the list (may be relative, for example -1)\n"
       "  close: close buffer (optional arg is part message, for a channel)\n"
       "   list: list open buffers (no parameter implies this list)\n"
       " notify: set notify level for buffer (0=never, 1=highlight, 2=1+msg, "
       "3=2+join/part)\n"
       "         (when executed on server buffer, this sets default notify "
       "level for whole server)\n"
       " scroll: scroll in history (may be relative, and may end by a letter: "
       "s=sec, m=min, h=hour, d=day, M=month, y=year); if there is "
       "only letter, then scroll to beginning of this item\n\n"
       " number: jump to buffer by number\n"
       "server,\n"
       "channel: jump to buffer by server and/or channel name\n\n"
       "Examples:\n"
       "        move buffer: /buffer move 5\n"
       "       close buffer: /buffer close this is part msg\n"
       "         set notify: /buffer notify 2\n"
       "    scroll 1 day up: /buffer scroll 1d  ==  /buffer scroll -1d  ==  /buffer scroll -24h\n"
       "scroll to beginning\n"
       "        of this day: /buffer scroll d\n"
       " scroll 15 min down: /buffer scroll +15m\n"
       "  scroll 20 msgs up: /buffer scroll -20\n"
       "   jump to #weechat: /buffer #weechat"),
    "move|close|list|notify|scroll|%S|%C %S|%C", 0, MAX_ARGS, 0, NULL, weechat_cmd_buffer },
  { "builtin", N_("launch WeeChat/IRC builtin command (do not look at plugins handlers or aliases)"),
    N_("command"),
    N_("command: command to execute (a '/' is automatically added if not found at beginning of command)\n"),
    "%w|%i", 0, MAX_ARGS, 1, NULL, weechat_cmd_builtin },
  { "clear", N_("clear window(s)"),
    N_("[-all | number [number ...]]"),
    N_("  -all: clear all buffers\n"
       "number: clear buffer by number"),
    "-all", 0, MAX_ARGS, 0, weechat_cmd_clear, NULL },
  { "connect", N_("connect to server(s)"),
    N_("[-all [-nojoin] | servername [servername ...] [-nojoin] | hostname "
       "[-port port] [-ipv6] [-ssl]]"),
    N_("      -all: connect to all servers\n"
       "servername: internal server name to connect\n"
       "   -nojoin: do not join any channel (even if autojoin is enabled on server)\n"
       "  hostname: hostname to connect, creating temporary server\n"
       "      port: port for server (integer, default is 6667)\n"
       "      ipv6: use IPv6 protocol\n"
       "       ssl: use SSL protocol"),
    "%S|-all|-nojoin|%*", 0, MAX_ARGS, 0, weechat_cmd_connect, NULL },
  { "disconnect", N_("disconnect from server(s)"),
    N_("[-all | servername [servername ...]]"),
    N_("      -all: disconnect from all servers\n"
       "servername: server name to disconnect"),
    "%S|-all", 0, MAX_ARGS, 0, weechat_cmd_disconnect, NULL },
  { "dcc", N_("starts DCC (file or chat) or close chat"),
    N_("action [nickname [file]]"),
    N_("  action: 'send' (file) or 'chat' or 'close' (chat)\n"
       "nickname: nickname to send file or chat\n"
       "    file: filename (on local host)"),
    "chat|send|close %n %f", 1, MAX_ARGS, 0, NULL, weechat_cmd_dcc },
  { "debug", N_("print debug messages"),
    N_("dump | windows"),
    N_("   dump: save memory dump in WeeChat log file (same dump is written when WeeChat crashes)\n"
       "windows: display windows tree"),
    "dump|windows", 1, 1, 0, weechat_cmd_debug, NULL },
  { "help", N_("display help about commands"),
    N_("[command]"),
    N_("command: name of a WeeChat or IRC command"),
    "%w|%i|%h", 0, 1, 0, weechat_cmd_help, NULL },
  { "history", N_("show buffer command history"),
    N_("[clear | value]"),
    N_("clear: clear history\n"
       "value: number of history entries to show"),
    "clear", 0, 1, 0, weechat_cmd_history, NULL },
  { "ignore", N_("ignore IRC messages and/or hosts"),
    N_("[mask [[type | command] [channel [server]]]]"),
    N_("   mask: nick or host mask to ignore\n"
       "   type: type of message to ignore (action, ctcp, dcc, pv)\n"
       "command: IRC command\n"
       "channel: name of channel for ignore\n"
       " server: name of server for ignore\n\n"
       "For each argument, '*' means all.\n"
       "Without argument, /ignore command lists all defined ignore."),
    "*|%n *|action|ctcp|dcc|pv|%I *|%c *|%s",
    0, 4, 0, weechat_cmd_ignore, NULL },
  { "key", N_("bind/unbind keys"),
    N_("[key [function/command]] [unbind key] [functions] [call function [\"args\"]] [reset -yes]"),
    N_("      key: display or bind this key to an internal function or a command "
       "(beginning by \"/\")\n"
       "   unbind: unbind a key\n"
       "functions: list internal functions for key bindings\n"
       "     call: call a function by name (with optional arguments)\n"
       "    reset: restore bindings to the default values and delete ALL "
       "personal bindings (use carefully!)"),
    "unbind|functions|call|reset %k", 0, MAX_ARGS, 0, NULL, weechat_cmd_key },
/*  { "panel", N_("manage panels"),
    N_("[list | add type position size | resize # size | close # | move #1 #2]"),
    N_("   list: list open panels (no parameter implies this list)\n"
       "    add: add a panel, type is global|local, position is top|bottom|left|right\n"
       " resize: resize a panel with a new size (may be relative, for example -1)\n"
       "  close: close a panel by number\n"
       "   move: move a panel to another number (may be relative, for example -1)"),
    "list|add|close|move global|local top|bottom|left|right",
    0, MAX_ARGS, 0, weechat_cmd_panel, NULL },*/
  { "plugin", N_("list/load/unload plugins"),
    N_("[list [name]] | [listfull [name]] | [load filename] | [autoload] | [reload [name]] | [unload [name]]"),
    N_("    list: list loaded plugins\n"
       "listfull: list loaded plugins with detailed info for each plugin\n"
       "    load: load a plugin\n"
       "autoload: autoload plugins in system or user directory\n"
       "  reload: reload one plugin (if no name given, unload all plugins, then autoload plugins)\n"
       "  unload: unload one or all plugins\n\n"
       "Without argument, /plugin command lists loaded plugins."),
    "list|listfull|load|autoload|reload|unload %P", 0, 2, 0, weechat_cmd_plugin, NULL },
  { "reconnect", N_("reconnect to server(s)"),
    N_("[-all [-nojoin] | servername [servername ...] [-nojoin]]"),
    N_("      -all: reconnect to all servers\n"
       "servername: server name to reconnect\n"
       "   -nojoin: do not join any channel (even if autojoin is enabled on server)"),
    "%S|-all|-nojoin|%*", 0, MAX_ARGS, 0, weechat_cmd_reconnect, NULL },
  { "save", N_("save config to disk"),
    N_("[file]"), N_("file: filename for writing config"),
    NULL, 0, 1, 0, weechat_cmd_save, NULL },
  { "server", N_("list, add or remove servers"),
    N_("[list [servername]] | [listfull [servername]] | [servername] | "
       "[add servername hostname [-port port] [-temp] [-auto | -noauto] "
       "[-ipv6] [-ssl] [-pwd password] [-nicks nick1 nick2 nick3] "
       "[-username username] [-realname realname] [-command command] "
       "[-autojoin channel[,channel]] ] | [copy servername newservername] | "
       "[rename servername newservername] | [keep servername] | "
       "[del servername]"),
    N_("      list: list servers (no parameter implies this list)\n"
       "  listfull: list servers with detailed info for each server\n"
       "       add: create a new server\n"
       "servername: server name, for internal and display use\n"
       "  hostname: name or IP address of server\n"
       "      port: port for server (integer, default is 6667)\n"
       "      temp: create temporary server (not saved in config file)\n"
       "      auto: automatically connect to server when WeeChat starts\n"
       "    noauto: do not connect to server when WeeChat starts (default)\n"
       "      ipv6: use IPv6 protocol\n"
       "       ssl: use SSL protocol\n"
       "  password: password for server\n"
       "     nick1: first nick for server\n"
       "     nick2: alternate nick for server\n"
       "     nick3: second alternate nick for server\n"
       "  username: user name\n"
       "  realname: real name of user\n"
       "      copy: duplicate a server\n"
       "    rename: rename a server\n"
       "      keep: keep server in config file (for temporary servers only)\n"
       "       del: delete a server"),
    "copy|rename|del|list|listfull %S %S", 0, MAX_ARGS, 0, weechat_cmd_server, NULL },
  { "set", N_("set config options"),
    N_("[option [ = value]]"),
    N_("option: name of an option (if name is full "
       "and no value is given, then help is displayed on option)\n"
       " value: value for option\n\n"
       "Option may be: servername.server_xxx where \"servername\" is an "
       "internal server name and \"xxx\" an option for this server."),
    "%o = %v", 0, MAX_ARGS, 0, NULL, weechat_cmd_set },
  { "setp", N_("set plugin config options"),
    N_("[option [ = value]]"),
    N_("option: name of a plugin option\n"
       " value: value for option\n\n"
       "Option is format: plugin.option, example: perl.myscript.item1"),
    "%O = %V", 0, MAX_ARGS, 0, NULL, weechat_cmd_setp },
  { "unalias", N_("remove an alias"),
    N_("alias_name"), N_("alias_name: name of alias to remove"),
    "%a", 1, 1, 0, NULL, weechat_cmd_unalias },
  { "unignore", N_("unignore IRC messages and/or hosts"),
    N_("[number | [mask [[type | command] [channel [server]]]]]"),
    N_(" number: # of ignore to unignore (number is displayed by list of ignore)\n"
       "   mask: nick or host mask to unignore\n"
       "   type: type of message to unignore (action, ctcp, dcc, pv)\n"
       "command: IRC command\n"
       "channel: name of channel for unignore\n"
       " server: name of server for unignore\n\n"
       "For each argument, '*' means all.\n"
       "Without argument, /unignore command lists all defined ignore."),
    "*|%n *|action|ctcp|dcc|pv|%I *|%c *|%s",
    0, 4, 0, weechat_cmd_unignore, NULL },
  { "upgrade", N_("upgrade WeeChat without disconnecting from servers"),
    N_("[path_to_binary]"),
    N_("path_to_binary: path to WeeChat binary (default is current binary)\n\n"
       "This command run again a WeeChat binary, so it should have been compiled "
       "or installed with a package manager before running this command."),
    "%f", 0, 1, 0, weechat_cmd_upgrade, NULL },
  { "uptime", N_("show WeeChat uptime"),
    N_("[-o]"),
    N_("-o: send uptime on current channel as an IRC message"),
    "-o", 0, 1, 0, weechat_cmd_uptime, NULL },
  { "window", N_("manage windows"),
    N_("[list | -1 | +1 | b# | up | down | left | right | splith [pct] "
       "| splitv [pct] | resize pct | merge [all]]"),
    N_("  list: list open windows (no parameter implies this list)\n"
       "    -1: jump to previous window\n"
       "    +1: jump to next window\n"
       "    b#: jump to next window displaying buffer number #\n"
       "    up: switch to window above current one\n"
       "  down: switch to window below current one\n"
       "  left: switch to window on the left\n"
       " right: switch to window on the right\n"
       "splith: split current window horizontally\n"
       "splitv: split current window vertically\n"
       "resize: resize window size, new size is <pct> pourcentage of parent window\n"
       " merge: merge window with another (all = keep only one window)\n\n"
       "For splith and splitv, pct is a pourcentage which represents "
       "size of new window, computed with current window as size reference. "
       "For example 25 means create a new window with size = current_size / 4"),
    "list|-1|+1|up|down|left|right|splith|splitv|resize|merge all",
    0, 2, 0, weechat_cmd_window, NULL },
  { NULL, NULL, NULL, NULL, NULL, 0, 0, 0, NULL, NULL }
};

t_weelist *index_commands;
t_weelist *last_index_command;


/*
 * command_index_build: build an index of commands (internal, irc and alias)
 *                      This list will be sorted, and used for completion
 */

void
command_index_build ()
{
    int i;
    
    index_commands = NULL;
    last_index_command = NULL;
    i = 0;
    while (weechat_commands[i].command_name)
    {
        (void) weelist_add (&index_commands, &last_index_command,
                            weechat_commands[i].command_name,
                            WEELIST_POS_SORT);
        i++;
    }
    i = 0;
    while (irc_commands[i].command_name)
    {
        if (irc_commands[i].cmd_function_args || irc_commands[i].cmd_function_1arg)
            (void) weelist_add (&index_commands, &last_index_command,
                                irc_commands[i].command_name,
                                WEELIST_POS_SORT);
        i++;
    }
}

/*
 * command_index_free: remove all commands in index
 */

void
command_index_free ()
{
    while (index_commands)
    {
        weelist_remove (&index_commands, &last_index_command, index_commands);
    }
}

/*
 * command_used_by_weechat: return 1 if command is used by weechat
 *                          (weechat command, IRC command or alias)
 */

int
command_used_by_weechat (char *command)
{
    t_weechat_alias *ptr_alias;
    int i;

    /* look for alias */
    for (ptr_alias = weechat_alias; ptr_alias;
         ptr_alias = ptr_alias->next_alias)
    {
        if (ascii_strcasecmp (ptr_alias->alias_name, command) == 0)
            return 1;
    }

    /* look for WeeChat command */
    for (i = 0; weechat_commands[i].command_name; i++)
    {
        if (ascii_strcasecmp (weechat_commands[i].command_name, command) == 0)
            return 1;
    }

    /* look for IRC command */
    for (i = 0; irc_commands[i].command_name; i++)
    {
        if ((ascii_strcasecmp (irc_commands[i].command_name, command) == 0) &&
            ((irc_commands[i].cmd_function_args) ||
             (irc_commands[i].cmd_function_1arg)))
            return 1;
    }

    /* no command/alias found */
    return 0;
}

/*
 * exec_weechat_command: executes a command (WeeChat internal or IRC)
 *                       if only_builtin == 1, then try only WeeChat/IRC commands
 *                       (not plugins neither aliases)
 *                       returns: 1 if command was executed succesfully
 *                                0 if error (command not executed)
 */

int
exec_weechat_command (t_irc_server *server, t_irc_channel *channel, char *string,
                      int only_builtin)
{
    int i, rc, argc, argc2, return_code, length1, length2;
    char *command, *pos, *ptr_args, *ptr_args2;
    char **argv, **argv2, *alias_command;
    char **commands, **ptr_cmd, **ptr_next_cmd;
    char *args_replaced, *vars_replaced, *new_ptr_cmd;
    char *unknown_command;
    int some_args_replaced;
    t_weechat_alias *ptr_alias;
    
    if ((!string) || (!string[0]) || (string[0] != '/'))
        return 0;
    
    command = strdup (string);
    
    /* look for end of command */
    ptr_args = NULL;
    
    pos = &command[strlen (command) - 1];
    if (pos[0] == ' ')
    {
        while ((pos > command) && (pos[0] == ' '))
            pos--;
        pos[1] = '\0';
    }
    
    pos = strchr (command, ' ');
    if (pos)
    {
        pos[0] = '\0';
        pos++;
        while (pos[0] == ' ')
            pos++;
        ptr_args = pos;
        if (!ptr_args[0])
            ptr_args = NULL;
    }
    
#ifdef PLUGINS
    if (only_builtin)
        rc = -1;
    else
    {
        vars_replaced = alias_replace_vars (server, channel, ptr_args);
        rc = plugin_cmd_handler_exec ((server) ? server->name : "", command + 1,
                                      (vars_replaced) ? vars_replaced : ptr_args);
        if (vars_replaced)
            free (vars_replaced);
    }
#else
    rc = -1;
#endif
    switch (rc)
    {
        case 0: /* plugin handler KO */
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf (NULL,
                        _("%s command \"%s\" failed\n"),
                        WEECHAT_ERROR, command + 1);
            break;
        case 1: /* plugin handler OK, executed */
            break;
        default: /* plugin handler not found */
            argv = explode_string (ptr_args, " ", 0, &argc);
            
            /* look for alias */
            if (!only_builtin)
            {
                for (ptr_alias = weechat_alias; ptr_alias;
                     ptr_alias = ptr_alias->next_alias)
                {
                    if (ascii_strcasecmp (ptr_alias->alias_name, command + 1) == 0)
                    {
                        if (ptr_alias->running == 1)
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s circular reference when calling alias \"/%s\"\n"),
                                        WEECHAT_ERROR, ptr_alias->alias_name);
                        }
                        else
                        {		
                            /* an alias can contain many commands separated by ';' */
                            commands = split_multi_command (ptr_alias->alias_command, ';');
                            if (commands)
                            {
                                some_args_replaced = 0;
                                ptr_alias->running = 1;
                                for (ptr_cmd=commands; *ptr_cmd; ptr_cmd++)
                                {
                                    ptr_next_cmd = ptr_cmd;
                                    ptr_next_cmd++;
                                    
                                    vars_replaced = alias_replace_vars (server, channel, *ptr_cmd);
                                    new_ptr_cmd = (vars_replaced) ? vars_replaced : *ptr_cmd;
                                    args_replaced = alias_replace_args (new_ptr_cmd, ptr_args);
                                    if (args_replaced)
                                    {
                                        some_args_replaced = 1;
                                        if (*ptr_cmd[0] == '/')
                                            (void) exec_weechat_command (server, channel, args_replaced, only_builtin);
                                        else
                                        {
                                            alias_command = (char *) malloc (1 + strlen(args_replaced) + 1);
                                            if (alias_command)
                                            {
                                                strcpy (alias_command, "/");
                                                strcat (alias_command, args_replaced);
                                                (void) exec_weechat_command (server, channel, alias_command, only_builtin);
                                                free (alias_command);
                                            }
                                        }
                                        free (args_replaced);
                                    }
                                    else
                                    {
                                        /* if alias has arguments, they are now
                                           arguments of the last command in the list (if no $1,$2,..$*) was found */
                                        if ((*ptr_next_cmd == NULL) && ptr_args && (!some_args_replaced))
                                        {
                                            length1 = strlen (new_ptr_cmd);
                                            length2 = strlen (ptr_args);
                                            
                                            alias_command = (char *) malloc ( 1 + length1 + 1 + length2 + 1);
                                            if (alias_command)
                                            {
                                                if (*ptr_cmd[0] != '/')
                                                    strcpy (alias_command, "/");
                                                else
                                                    strcpy (alias_command, "");
                                                
                                                strcat (alias_command, new_ptr_cmd);
                                                strcat (alias_command, " ");
                                                strcat (alias_command, ptr_args);
                                                
                                                (void) exec_weechat_command (server, channel, alias_command, only_builtin);
                                                free (alias_command);
                                            }
                                        }
                                        else
                                        {
                                            if (*ptr_cmd[0] == '/')
                                                (void) exec_weechat_command (server, channel, new_ptr_cmd, only_builtin);
                                            else
                                            {
                                                alias_command = (char *) malloc (1 + strlen (new_ptr_cmd) + 1);
                                                if (alias_command)
                                                {
                                                    strcpy (alias_command, "/");
                                                    strcat (alias_command, new_ptr_cmd);
                                                    (void) exec_weechat_command (server, channel, alias_command, only_builtin);
                                                    free (alias_command);
                                                }
                                            }
                                        }
                                    }
                                    if (vars_replaced)
                                        free (vars_replaced);
                                }
                                ptr_alias->running = 0;
                                free_multi_command (commands);
                            }
                        }
                        free_exploded_string (argv);
                        free (command);
                        return 1;
                    }
                }
            }
            
            /* look for WeeChat command */
            for (i = 0; weechat_commands[i].command_name; i++)
            {
                if (ascii_strcasecmp (weechat_commands[i].command_name, command + 1) == 0)
                {
                    if ((argc < weechat_commands[i].min_arg)
                        || (argc > weechat_commands[i].max_arg))
                    {
                        if (weechat_commands[i].min_arg ==
                            weechat_commands[i].max_arg)
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        NG_("%s wrong argument count for %s command \"%s\" "
                                            "(expected: %d arg)\n",
                                            "%s wrong argument count for %s command \"%s\" "
                                            "(expected: %d args)\n",
                                            weechat_commands[i].max_arg),
                                        WEECHAT_ERROR, PACKAGE_NAME,
                                        command + 1,
                                        weechat_commands[i].max_arg);
                        }
                        else
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        NG_("%s wrong argument count for %s command \"%s\" "
                                            "(expected: between %d and %d arg)\n",
                                            "%s wrong argument count for %s command \"%s\" "
                                            "(expected: between %d and %d args)\n",
                                            weechat_commands[i].max_arg),
                                        WEECHAT_ERROR, PACKAGE_NAME,
                                        command + 1,
                                        weechat_commands[i].min_arg,
                                        weechat_commands[i].max_arg);
                        }
                    }
                    else
                    {
                        ptr_args2 = (ptr_args) ? (char *)gui_color_encode ((unsigned char *)ptr_args,
                                                                           (weechat_commands[i].conversion
                                                                            && cfg_irc_colors_send)) : NULL;
                        if (weechat_commands[i].cmd_function_args)
                        {
                            argv2 = explode_string ((ptr_args2) ? ptr_args2 : ptr_args,
                                                    " ", 0, &argc2);
                            return_code = (int) (weechat_commands[i].cmd_function_args)
                                (server, channel, argc2, argv2);
                            free_exploded_string (argv2);
                        }
                        else
                            return_code = (int) (weechat_commands[i].cmd_function_1arg)
                                (server, channel, (ptr_args2) ? ptr_args2 : ptr_args);
                        if (return_code < 0)
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s command \"%s\" failed\n"),
                                        WEECHAT_ERROR, command + 1);
                        }
                        if (ptr_args2)
                            free (ptr_args2);
                    }
                    free_exploded_string (argv);
                    free (command);
                    return 1;
                }
            }
            
            /* look for IRC command */
            for (i = 0; irc_commands[i].command_name; i++)
            {
                if ((ascii_strcasecmp (irc_commands[i].command_name, command + 1) == 0) &&
                    ((irc_commands[i].cmd_function_args) ||
                     (irc_commands[i].cmd_function_1arg)))
                {
                    if ((argc < irc_commands[i].min_arg)
                        || (argc > irc_commands[i].max_arg))
                    {
                        if (irc_commands[i].min_arg == irc_commands[i].max_arg)
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf
                                (NULL,
                                 NG_("%s wrong argument count for IRC command \"%s\" "
                                     "(expected: %d arg)\n",
                                     "%s wrong argument count for IRC command \"%s\" "
                                     "(expected: %d args)\n",
                                     irc_commands[i].max_arg),
                                 WEECHAT_ERROR,
                                 command + 1,
                                 irc_commands[i].max_arg);
                        }
                        else
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf
                                (NULL,
                                 NG_("%s wrong argument count for IRC command \"%s\" "
                                     "(expected: between %d and %d arg)\n",
                                     "%s wrong argument count for IRC command \"%s\" "
                                     "(expected: between %d and %d args)\n",
                                     irc_commands[i].max_arg),
                                 WEECHAT_ERROR,
                                 command + 1,
                                 irc_commands[i].min_arg, irc_commands[i].max_arg);
                        }
                    }
                    else
                    {
                        if ((irc_commands[i].needs_connection) &&
                            ((!server) || (!server->is_connected)))
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s command \"%s\" needs a server connection!\n"),
                                        WEECHAT_ERROR, irc_commands[i].command_name);
                            free (command);
                            return 0;
                        }
                        if (channel && channel->dcc_chat)
                        {
                            irc_display_prefix (server, channel->buffer, GUI_PREFIX_ERROR);
                            gui_printf (channel->buffer,
                                        _("%s command \"%s\" can not be "
                                          "executed on DCC CHAT buffer\n"),
                                        WEECHAT_ERROR,
                                        irc_commands[i].command_name);
                            free (command);
                            return 0;
                        }
                        ptr_args2 = (ptr_args) ? (char *)gui_color_encode ((unsigned char *)ptr_args,
                                                                           (irc_commands[i].conversion
                                                                            && cfg_irc_colors_send)) : NULL;
                        if (irc_commands[i].cmd_function_args)
                        {
                            argv2 = explode_string ((ptr_args2) ? ptr_args2 : ptr_args,
                                                    " ", 0, &argc2);
                            return_code = (int) (irc_commands[i].cmd_function_args)
                                (server, channel, argc2, argv2);
                            free_exploded_string (argv2);
                        }
                        else
                            return_code = (int) (irc_commands[i].cmd_function_1arg)
                                (server, channel, (ptr_args2) ? ptr_args2 : ptr_args);
                        if (return_code < 0)
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s command \"%s\" failed\n"),
                                        WEECHAT_ERROR, command + 1);
                        }
                        if (ptr_args2)
                            free (ptr_args2);
                    }
                    free_exploded_string (argv);
                    free (command);
                    return 1;
                }
            }

            /* should we send unknown command to IRC server? */
            if (cfg_irc_send_unknown_commands)
            {
                if (ptr_args)
                    unknown_command = (char *)malloc (strlen (command + 1) + 1 + strlen (ptr_args) + 1);
                else
                    unknown_command = (char *)malloc (strlen (command + 1) + 1);
                
                if (unknown_command)
                {
                    strcpy (unknown_command, command + 1);
                    if (ptr_args)
                    {
                        strcat (unknown_command, " ");
                        strcat (unknown_command, ptr_args);
                    }
                    irc_send_cmd_quote (server, channel, unknown_command);
                    free (unknown_command);
                }
            }
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s unknown command \"%s\" (type /help for help). "
                              "To send unknown commands to IRC server, enable option "
                              "irc_send_unknown_commands.\n"),
                            WEECHAT_ERROR,
                            command + 1);
            }
            
            free_exploded_string (argv);
    }
    free (command);
    return 0;
}

/*
 * user_message_display: display user message
 */

void
user_message_display (t_irc_server *server, t_gui_buffer *buffer, char *text)
{
    t_irc_nick *ptr_nick;
    
    if ((GUI_CHANNEL(buffer)->type == IRC_CHANNEL_TYPE_PRIVATE)
        || (GUI_CHANNEL(buffer)->type == IRC_CHANNEL_TYPE_DCC_CHAT))
    {
        irc_display_nick (buffer, NULL, server->nick,
                          GUI_MSG_TYPE_NICK, 1, GUI_COLOR_WIN_NICK_SELF, 0);
        gui_printf_type (buffer,
                         GUI_MSG_TYPE_MSG,
                         "%s%s\n",
                         GUI_COLOR(GUI_COLOR_WIN_CHAT),
                         text);
    }
    else
    {
        ptr_nick = irc_nick_search (GUI_CHANNEL(buffer), server->nick);
        if (ptr_nick)
        {
            irc_display_nick (buffer, ptr_nick, NULL,
                              GUI_MSG_TYPE_NICK, 1, -1, 0);
            gui_printf_type (buffer,
                             GUI_MSG_TYPE_MSG,
                             "%s%s\n",
                             GUI_COLOR(GUI_COLOR_WIN_CHAT),
                             text);
        }
        else
        {
            irc_display_prefix (server, server->buffer, GUI_PREFIX_ERROR);
            gui_printf (server->buffer,
                        _("%s cannot find nick for sending message\n"),
                        WEECHAT_ERROR);
        }
    }
}

/*
 * user_message: send a PRIVMSG message, and split it if > 512 bytes
 */

void
user_message (t_irc_server *server, t_gui_buffer *buffer, char *text)
{
    int max_length;
    char *pos, *pos_next, *pos_max, *next, saved_char, *last_space;

    if (!text || !text[0])
        return;
    
    if (!server->is_connected)
    {
        irc_display_prefix (server, buffer, GUI_PREFIX_ERROR);
        gui_printf (buffer, _("%s you are not connected to server\n"),
                    WEECHAT_ERROR);
        return;
    }
    
    next = NULL;
    last_space = NULL;
    saved_char = '\0';
    
    max_length = 512 - 16 - 65 - 10 - strlen (server->nick) -
        strlen (GUI_CHANNEL(buffer)->name);
    
    if (max_length > 0)
    {
        if ((int)strlen (text) > max_length)
        {
            pos = text;
            pos_max = text + max_length;
            while (pos && pos[0])
            {
                if (pos[0] == ' ')
                    last_space = pos;
                pos_next = utf8_next_char (pos);
                if (pos_next > pos_max)
                    break;
                pos = pos_next;
            }
            if (last_space && (last_space < pos))
                pos = last_space + 1;
            saved_char = pos[0];
            pos[0] = '\0';
            next = pos;
        }
    }
    
    irc_server_sendf_queued (server, "PRIVMSG %s :%s",
                             GUI_CHANNEL(buffer)->name, text);
    user_message_display (server, buffer, text);
    
    if (next)
    {
        next[0] = saved_char;
        user_message (server, buffer, next);
    }
}

/*
 * is_command: return 1 if line is a command, 0 otherwise
 */

int
is_command (char *line)
{
    char *pos_slash, *pos_space;

    if (strncmp (line, "/*", 2) == 0)
        return 0;
    
    pos_slash = strchr (line + 1, '/');
    pos_space = strchr (line + 1, ' ');
    
    return (line[0] == '/')
        && (!pos_slash || (pos_space && pos_slash > pos_space));
}

/*
 * user_command: interprets user command (if beginning with '/')
 *               any other text is sent to the server, if connected
 */

void
user_command (t_irc_server *server, t_irc_channel *channel, char *command, int only_builtin)
{
    t_gui_buffer *buffer;
    char *new_cmd, *ptr_cmd, *pos;
    char *command_with_colors;
    
    if ((!command) || (!command[0]) || (command[0] == '\r') || (command[0] == '\n'))
        return;
    
#ifdef PLUGINS
    new_cmd = plugin_modifier_exec (PLUGIN_MODIFIER_IRC_USER,
                                    (server) ? server->name : "",
                                    command);
#else
    new_cmd = NULL;
#endif
    
    /* no changes in new command */
    if (new_cmd && (strcmp (command, new_cmd) == 0))
    {
        free (new_cmd);
        new_cmd = NULL;
    }
    
    /* message not dropped? */
    if (!new_cmd || new_cmd[0])
    {
        /* use new command (returned by plugin) */
        ptr_cmd = (new_cmd) ? new_cmd : command;
        
        while (ptr_cmd && ptr_cmd[0])
        {
            pos = strchr (ptr_cmd, '\n');
            if (pos)
                pos[0] = '\0';
            
            gui_buffer_find_context (server, channel, NULL, &buffer);
            
            if (is_command (ptr_cmd))
            {
                /* WeeChat internal command (or IRC command) */
                (void) exec_weechat_command (server, channel, ptr_cmd, only_builtin);
            }
            else
            {
                if ((ptr_cmd[0] == '/') && (ptr_cmd[1] == '/'))
                    ptr_cmd++;
                
                if (server && (!GUI_BUFFER_IS_SERVER(buffer)))
                {
                    command_with_colors = (char *)gui_color_encode ((unsigned char *)ptr_cmd,
                                                                    cfg_irc_colors_send);
                    
                    if (GUI_CHANNEL(buffer)->dcc_chat)
                    {
                        if (((t_irc_dcc *)(GUI_CHANNEL(buffer)->dcc_chat))->sock < 0)
                        {
                            irc_display_prefix (server, buffer, GUI_PREFIX_ERROR);
                            gui_printf_nolog (buffer, "%s DCC CHAT is closed\n",
                                              WEECHAT_ERROR);
                        }
                        else
                        {
                            irc_dcc_chat_sendf ((t_irc_dcc *)(GUI_CHANNEL(buffer)->dcc_chat),
                                                "%s\r\n",
                                                (command_with_colors) ? command_with_colors : ptr_cmd);
                            user_message_display (server, buffer,
                                                  (command_with_colors) ?
                                                  command_with_colors : ptr_cmd);
                        }
                    }
                    else
                        user_message (server, buffer,
                                      (command_with_colors) ? command_with_colors : ptr_cmd);
                    
                    if (command_with_colors)
                        free (command_with_colors);
                }
                else
                {
                    irc_display_prefix (NULL, (server) ? server->buffer : NULL, GUI_PREFIX_ERROR);
                    gui_printf_nolog ((server) ? server->buffer : NULL,
                                      _("This window is not a channel!\n"));
                }
            }

            if (pos)
            {
                pos[0] = '\n';
                ptr_cmd = pos + 1;
            }
            else
                ptr_cmd = NULL;
        }
    }
}

/*
 * weechat_cmd_alias: display or create alias
 */

int
weechat_cmd_alias (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    char *pos;
    t_weechat_alias *ptr_alias;
    
    /* make C compiler happy */
    (void) server;
    (void) channel;
    
    if (arguments && arguments[0])
    {
        while (arguments[0] == '/')
        {
            arguments++;
        }
        
        /* Define new alias */
        pos = strchr (arguments, ' ');
        if (pos)
        {
            pos[0] = '\0';
            pos++;
            while (pos[0] == ' ')
                pos++;
            if (!pos[0])
            {	
		irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
		gui_printf (NULL, _("%s missing arguments for \"%s\" command\n"),
			    WEECHAT_ERROR, "alias");
                return -1;
            }            
	    if (!alias_new (arguments, pos))
                return -1;
            if (weelist_add (&index_commands, &last_index_command, arguments,
                             WEELIST_POS_SORT))
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                gui_printf (NULL, _("Alias \"%s\" => \"%s\" created\n"),
                            arguments, pos);
            }
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL, _("Failed to create alias \"%s\" => \"%s\" "
                            "(not enough memory)\n"),
                            arguments, pos);
                return -1;
            }
        }
        else
        {
	    ptr_alias = alias_search (arguments);
	    if (ptr_alias)
	    {
		gui_printf (NULL, "\n");
		gui_printf (NULL, _("Alias:\n"));
		gui_printf (NULL, "  %s %s=>%s %s\n",
			    ptr_alias->alias_name,
			    GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
			    GUI_COLOR(GUI_COLOR_WIN_CHAT),
			    ptr_alias->alias_command);
	    }
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                gui_printf (NULL, _("No alias found.\n"));
            }
        }
    }
    else
    {
        /* List all aliases */
        if (weechat_alias)
        {
            gui_printf (NULL, "\n");
            gui_printf (NULL, _("List of aliases:\n"));
            for (ptr_alias = weechat_alias; ptr_alias;
                 ptr_alias = ptr_alias->next_alias)
            {
                gui_printf (NULL, "  %s %s=>%s %s\n",
                            ptr_alias->alias_name,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT),
                            ptr_alias->alias_command);
            }
        }
        else
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
            gui_printf (NULL, _("No alias defined.\n"));
        }
    }
    return 0;
}

/*
 * weechat_cmd_buffer_display_info: display info about a buffer
 */

void
weechat_cmd_buffer_display_info (t_gui_buffer *buffer)
{
    switch (buffer->type)
    {
        case GUI_BUFFER_TYPE_STANDARD:
            if (GUI_BUFFER_IS_SERVER(buffer))
            {
                if (GUI_SERVER(buffer))
                    gui_printf (NULL, _("%sServer: %s%s\n"),
                                GUI_COLOR(GUI_COLOR_WIN_CHAT),
                                GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                                GUI_SERVER(buffer)->name);
                else
                    gui_printf (NULL, _("%snot connected\n"),
                                GUI_COLOR(GUI_COLOR_WIN_CHAT));
            }
            else if (GUI_BUFFER_IS_CHANNEL (buffer))
                gui_printf (NULL, _("%sChannel: %s%s %s(server: %s%s%s)\n"),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                            GUI_CHANNEL(buffer)->name,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                            GUI_SERVER(buffer)->name,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT));
            else if (GUI_BUFFER_IS_PRIVATE (buffer))
                gui_printf (NULL, _("%sPrivate with: %s%s %s(server: %s%s%s)\n"),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_NICK),
                            GUI_CHANNEL(buffer)->name,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                            GUI_SERVER(buffer)->name,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT));
            else
                gui_printf (NULL, _("%sunknown\n"),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT));
            break;
        case GUI_BUFFER_TYPE_DCC:
            gui_printf (NULL, "%sDCC\n",
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL));
            break;
        case GUI_BUFFER_TYPE_RAW_DATA:
            gui_printf (NULL, _("%sraw IRC data\n"),
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL));
            break;
        default:
            gui_printf (NULL, _("%sunknown\n"),
                        GUI_COLOR(GUI_COLOR_WIN_CHAT));
            break;
    }
}

/*
 * weechat_cmd_buffer: manage buffers
 */

int
weechat_cmd_buffer (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    t_gui_window *window;
    t_gui_buffer *buffer, *ptr_buffer;
    t_irc_server *ptr_server;
    t_irc_channel *ptr_channel;
    long number;
    char *error, *pos, **argv;
    int argc, target_buffer, count;
    
    gui_buffer_find_context (server, channel, &window, &buffer);
    
    argv = explode_string (arguments, " ", 0, &argc);
    
    if ((argc == 0) || ((argc == 1) && (ascii_strcasecmp (argv[0], "list") == 0)))
    {
        /* list open buffers */
        
        gui_printf (NULL, "\n");
        gui_printf (NULL, _("Open buffers:\n"));
        
        for (ptr_buffer = gui_buffers; ptr_buffer; ptr_buffer = ptr_buffer->next_buffer)
        {
            gui_printf (NULL, "%s[%s%d%s] ",
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                        GUI_COLOR(GUI_COLOR_WIN_CHAT),
                        ptr_buffer->number,
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
            weechat_cmd_buffer_display_info (ptr_buffer);
        }
    }
    else
    {
        if (ascii_strcasecmp (argv[0], "move") == 0)
        {
            /* move buffer to another number in the list */
            
            if (argc < 2)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL, _("%s missing arguments for \"%s\" command\n"),
                            WEECHAT_ERROR, "buffer");
                free_exploded_string (argv);
                return -1;
            }
            
            error = NULL;
            number = strtol (((argv[1][0] == '+') || (argv[1][0] == '-')) ? argv[1] + 1 : argv[1],
                             &error, 10);
            if ((error) && (error[0] == '\0'))
            {
                if (argv[1][0] == '+')
                    gui_buffer_move_to_number (buffer,
                                               buffer->number + ((int) number));
                else if (argv[1][0] == '-')
                    gui_buffer_move_to_number (buffer,
                                               buffer->number - ((int) number));
                else
                    gui_buffer_move_to_number (buffer, (int) number);
            }
            else
            {
                /* invalid number */
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL, _("%s incorrect buffer number\n"),
                            WEECHAT_ERROR);
                free_exploded_string (argv);
                return -1;
            }
        }
        else if (ascii_strcasecmp (argv[0], "close") == 0)
        {
            /* close buffer (server or channel/private) */
            
            if ((!buffer->next_buffer)
                && (buffer == gui_buffers)
                && ((!buffer->all_servers)
                    || (!GUI_SERVER(buffer))))
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s can not close the single buffer\n"),
                            WEECHAT_ERROR);
                free_exploded_string (argv);
                return -1;
            }
            if (GUI_BUFFER_IS_SERVER(buffer))
            {
                if (GUI_SERVER(buffer))
                {
                    if (GUI_SERVER(buffer)->channels)
                    {
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                        gui_printf (NULL,
                                    _("%s can not close server buffer while channels "
                                      "are open\n"),
                                    WEECHAT_ERROR);
                        free_exploded_string (argv);
                        return -1;
                    }
                    irc_server_disconnect (GUI_SERVER(buffer), 0);
                    ptr_server = GUI_SERVER(buffer);
                    if (!buffer->all_servers)
                    {
                        gui_buffer_free (buffer, 1);
                        ptr_server->buffer = NULL;
                    }
                    else
                    {
                        ptr_server->buffer = NULL;
                        buffer->server = NULL;
                        gui_window_switch_server (window);
                    }
                    gui_status_draw (gui_current_window->buffer, 1);
                    gui_input_draw (gui_current_window->buffer, 1);
                }
            }
            else
            {
                if (GUI_CHANNEL(buffer)
                    && (GUI_CHANNEL(buffer)->type == IRC_CHANNEL_TYPE_DCC_CHAT))
                {
                    ptr_server = GUI_SERVER(buffer);
                    ptr_channel = GUI_CHANNEL(buffer);
                    gui_buffer_free (ptr_channel->buffer, 1);
                    irc_channel_free (ptr_server, ptr_channel);
                    gui_status_draw (gui_current_window->buffer, 1);
                    gui_input_draw (gui_current_window->buffer, 1);
                }
                else
                {
                    if (GUI_SERVER(buffer))
                    {
                        if (GUI_SERVER(buffer)->is_connected
                            && GUI_CHANNEL(buffer)
                            && GUI_CHANNEL(buffer)->nicks)
                        {
                            pos = strstr (arguments, "close ");
                            if (pos)
                                pos += 6;
                            GUI_CHANNEL(buffer)->close = 1;
                            irc_send_cmd_part (GUI_SERVER(buffer),
                                               GUI_CHANNEL(buffer),
                                               pos);
                        }
                        else
                        {
                            ptr_channel = GUI_CHANNEL(buffer);
                            ptr_server = GUI_SERVER(buffer);
                            gui_buffer_free (buffer, 1);
                            if (ptr_channel)
                                irc_channel_free (ptr_server, ptr_channel);
                        }
                    }
                    else
                        gui_buffer_free (buffer, 1);
                    gui_status_draw (gui_current_window->buffer, 1);
                }
            }
        }
        else if (ascii_strcasecmp (argv[0], "notify") == 0)
        {
            if (argc < 2)
            {
                gui_printf (NULL, "\n");

                /* display default notify level for all connected servers */
                gui_printf (NULL, _("Default notify levels for servers:"));
                count = 0;
                for (ptr_server = irc_servers; ptr_server;
                     ptr_server = ptr_server->next_server)
                {
                    if (ptr_server->buffer)
                    {
                        gui_printf (NULL, "  %s:%d",
                                    ptr_server->name,
                                    irc_server_get_default_notify_level (ptr_server));
                        count++;
                    }
                }
                if (count == 0)
                    gui_printf (NULL, "  -");
                gui_printf (NULL, "\n");
                
                /* display notify level for all buffers */
                gui_printf (NULL, _("Notify levels:"));
                for (ptr_buffer = gui_buffers; ptr_buffer;
                     ptr_buffer = ptr_buffer->next_buffer)
                {
                    gui_printf (NULL, "  %d.%s:",
                                ptr_buffer->number,
                                (ptr_buffer->type == GUI_BUFFER_TYPE_DCC) ? "DCC" :
                                ((ptr_buffer->type == GUI_BUFFER_TYPE_RAW_DATA) ? _("Raw IRC data") :
                                 ((GUI_BUFFER_IS_SERVER(ptr_buffer) && GUI_SERVER(ptr_buffer)) ? GUI_SERVER(ptr_buffer)->name :
                                  ((GUI_CHANNEL(ptr_buffer)) ? (GUI_CHANNEL(ptr_buffer)->name) : "-"))));
                    if ((!GUI_BUFFER_IS_CHANNEL(ptr_buffer))
                        && (!GUI_BUFFER_IS_PRIVATE(ptr_buffer)))
                        gui_printf (NULL, "-");
                    else
                        gui_printf (NULL, "%d", ptr_buffer->notify_level);
                }
                gui_printf (NULL, "\n");
            }
            else
            {
                /* set notify level for buffer */
                error = NULL;
                number = strtol (argv[1], &error, 10);
                if ((error) && (error[0] == '\0'))
                {
                    if ((number < GUI_NOTIFY_LEVEL_MIN) || (number > GUI_NOTIFY_LEVEL_MAX))
                    {
                        /* invalid highlight level */
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                        gui_printf (NULL, _("%s incorrect notify level (must be between %d and %d)\n"),
                                    WEECHAT_ERROR, GUI_NOTIFY_LEVEL_MIN, GUI_NOTIFY_LEVEL_MAX);
                        free_exploded_string (argv);
                        return -1;
                    }
                    if (!GUI_SERVER(buffer)
                        || ((!GUI_BUFFER_IS_SERVER(buffer))
                            && (!GUI_BUFFER_IS_CHANNEL(buffer))
                            && (!GUI_BUFFER_IS_PRIVATE(buffer))))
                    {
                        /* invalid buffer type (only ok on server, channel or private) */
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                        gui_printf (NULL,
                                    _("%s incorrect buffer for notify "
                                      "(must be server, channel or private)\n"),
                                    WEECHAT_ERROR);
                        free_exploded_string (argv);
                        return -1;
                    }
                    if (GUI_BUFFER_IS_SERVER(buffer))
                    {
                        irc_server_set_default_notify_level (GUI_SERVER(buffer),
                                                             number);
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                        gui_printf (NULL, _("New default notify level for server %s%s%s: %s%d %s"),
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                                    GUI_SERVER(buffer)->name,
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT),
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                    number,
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT));
                    }
                    else
                    {
                        irc_channel_set_notify_level (GUI_SERVER(buffer),
                                                      GUI_CHANNEL(buffer),
                                                      number);
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                        gui_printf (NULL, _("New notify level for %s%s%s: %s%d %s"),
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                    GUI_CHANNEL(buffer)->name,
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT),
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                    number,
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT));
                    }
                    switch (number)
                    {
                        case 0:
                            gui_printf (NULL, _("(hotlist: never)\n"));
                            break;
                        case 1:
                            gui_printf (NULL, _("(hotlist: highlights)\n"));
                            break;
                        case 2:
                            gui_printf (NULL, _("(hotlist: highlights + messages)\n"));
                            break;
                        case 3:
                            gui_printf (NULL, _("(hotlist: highlights + messages + join/part (all))\n"));
                            break;
                        default:
                            gui_printf (NULL, "\n");
                            break;
                    }
                    config_change_notify_levels ();
                }
                else
                {
                    /* invalid number */
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                    gui_printf (NULL, _("%s incorrect notify level (must be between %d and %d)\n"),
                                WEECHAT_ERROR, GUI_NOTIFY_LEVEL_MIN, GUI_NOTIFY_LEVEL_MAX);
                    free_exploded_string (argv);
                    return -1;
                }
            }
        }
        else if (ascii_strcasecmp (argv[0], "scroll") == 0)
        {
            if (argc >= 2)
                gui_buffer_scroll (window, argv[1]);
        }
        else
        {
            /* jump to buffer by number or server/channel name */
            
            if (argv[0][0] == '-')
            {
                /* relative jump '-' */
                error = NULL;
                number = strtol (argv[0] + 1, &error, 10);
                if ((error) && (error[0] == '\0'))
                {
                    target_buffer = buffer->number - (int) number;
                    if (target_buffer < 1)
                        target_buffer = (last_gui_buffer) ?
                            last_gui_buffer->number + target_buffer : 1;
                    gui_buffer_switch_by_number (window,
                                                 target_buffer);
                }
            }
            else if (argv[0][0] == '+')
            {
                /* relative jump '+' */
                error = NULL;
                number = strtol (argv[0] + 1, &error, 10);
                if ((error) && (error[0] == '\0'))
                {
                    target_buffer = buffer->number + (int) number;
                    if (last_gui_buffer && target_buffer > last_gui_buffer->number)
                        target_buffer -= last_gui_buffer->number;
                    gui_buffer_switch_by_number (window,
                                                 target_buffer);
                }
            }
            else
            {
                /* absolute jump by number, or by server/channel name */
                error = NULL;
                number = strtol (argv[0], &error, 10);
                if ((error) && (error[0] == '\0'))
                    gui_buffer_switch_by_number (window, (int) number);
                else
                {
                    ptr_buffer = NULL;
                    if (argc > 1)
                        ptr_buffer = gui_buffer_search (argv[0], argv[1]);
                    else
                    {
                        ptr_server = irc_server_search (argv[0]);
                        if (ptr_server)
                            ptr_buffer = gui_buffer_search (argv[0], NULL);
                        else
                            ptr_buffer = gui_buffer_search (NULL, argv[0]);
                    }
                    if (ptr_buffer)
                    {
                        gui_window_switch_to_buffer (window, ptr_buffer);
                        gui_window_redraw_buffer (ptr_buffer);
                    }
                }
            }
            
        }
    }
    free_exploded_string (argv);
    return 0;
}

/*
 * weechat_cmd_builtin: launch WeeChat/IRC builtin command
 */

int
weechat_cmd_builtin (t_irc_server *server, t_irc_channel *channel,
                     char *arguments)
{
    char *command;
    int length;
    
    if (arguments && arguments[0])
    {
        if (arguments[0] == '/')
            user_command (server, channel, arguments, 1);
        else
        {
            length = strlen (arguments) + 2;
            command = (char *)malloc (length);
            if (command)
            {
                snprintf (command, length, "/%s", arguments);
                user_command (server, channel, command, 1);
                free (command);
            }
        }
    }
    return 0;
}

/*
 * weechat_cmd_clear: display or create alias
 */

int
weechat_cmd_clear (t_irc_server *server, t_irc_channel *channel,
                   int argc, char **argv)
{
    t_gui_buffer *buffer;
    char *error;
    long number;
    int i;
    
    if (argc > 0)
    {
        if (ascii_strcasecmp (argv[0], "-all") == 0)
            gui_buffer_clear_all ();
        else
        {
            for (i = 0; i < argc; i++)
            {
                error = NULL;
                number = strtol (argv[i], &error, 10);
                if ((error) && (error[0] == '\0'))
                {
                    buffer = gui_buffer_search_by_number (number);
                    if (!buffer)
                    {
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                        gui_printf (NULL,
                                    _("%s buffer number \"%s\" not found for \"%s\" command\n"),
                                    WEECHAT_ERROR, argv[i], "clear");
                        return -1;
                    }
                    gui_buffer_clear (buffer);
                }
                else
                {
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                    gui_printf (NULL,
                                _("%s unknown option for \"%s\" command\n"),
                                WEECHAT_ERROR, "clear");
                    return -1;
                }
            }
        }
    }
    else
    {
        gui_buffer_find_context (server, channel, NULL, &buffer);
        gui_buffer_clear (buffer);
    }
    
    return 0;
}

/*
 * weechat_cmd_connect_one_server: connect to one server
 *                                 return 0 if error, 1 if ok
 */

int
weechat_cmd_connect_one_server (t_gui_window *window, t_irc_server *server,
                                int no_join)
{
    if (server->is_connected)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
        gui_printf (NULL,
                    _("%s already connected to server \"%s\"!\n"),
                    WEECHAT_ERROR, server->name);
        return 0;
    }
    if (server->child_pid > 0)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
        gui_printf (NULL,
                    _("%s currently connecting to server \"%s\"!\n"),
                    WEECHAT_ERROR, server->name);
        return 0;
    }
    if (!server->buffer)
    {
        if (!gui_buffer_new (window, server, NULL,
                             GUI_BUFFER_TYPE_STANDARD, 1))
            return 0;
    }
    if (irc_server_connect (server, no_join))
    {
        server->reconnect_start = 0;
        server->reconnect_join = (server->channels) ? 1 : 0;
        gui_status_draw (server->buffer, 1);
    }
    
    /* connect ok */
    return 1;
}

/*
 * weechat_cmd_connect: connect to server(s)
 */

int
weechat_cmd_connect (t_irc_server *server, t_irc_channel *channel,
                     int argc, char **argv)
{
    t_gui_window *window;
    t_gui_buffer *buffer;
    t_irc_server *ptr_server, server_tmp;
    int i, nb_connect, connect_ok, all_servers, no_join, port, ipv6, ssl;
    char *error;
    long number;
    
    gui_buffer_find_context (server, channel, &window, &buffer);

    nb_connect = 0;
    connect_ok = 1;
    port = IRC_DEFAULT_PORT;
    ipv6 = 0;
    ssl = 0;
    
    all_servers = 0;
    no_join = 0;
    for (i = 0; i < argc; i++)
    {
        if (ascii_strcasecmp (argv[i], "-all") == 0)
            all_servers = 1;
        if (ascii_strcasecmp (argv[i], "-nojoin") == 0)
            no_join = 1;
        if (ascii_strcasecmp (argv[i], "-ipv6") == 0)
            ipv6 = 1;
        if (ascii_strcasecmp (argv[i], "-ssl") == 0)
            ssl = 1;
        if (ascii_strcasecmp (argv[i], "-port") == 0)
        {
            if (i == (argc - 1))
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s missing argument for \"%s\" option\n"),
                            WEECHAT_ERROR, "-port");
                return -1;
            }
            error = NULL;
            number = strtol (argv[++i], &error, 10);
            if ((error) && (error[0] == '\0'))
                port = number;
        }
    }
    
    if (all_servers)
    {
        for (ptr_server = irc_servers; ptr_server;
             ptr_server = ptr_server->next_server)
        {
            nb_connect++;
            if (!ptr_server->is_connected && (ptr_server->child_pid == 0))
            {
                if (!weechat_cmd_connect_one_server (window, ptr_server,
                                                     no_join))
                    connect_ok = 0;
            }
        }
    }
    else
    {
        for (i = 0; i < argc; i++)
        {
            if (argv[i][0] != '-')
            {
                nb_connect++;
                ptr_server = irc_server_search (argv[i]);
                if (ptr_server)
                {
                    if (!weechat_cmd_connect_one_server (window, ptr_server,
                                                         no_join))
                        connect_ok = 0;
                }
                else
                {
                    irc_server_init (&server_tmp);
                    server_tmp.name = strdup (argv[i]);
                    server_tmp.address = strdup (argv[i]);
                    server_tmp.port = port;
                    server_tmp.ipv6 = ipv6;
                    server_tmp.ssl = ssl;
                    ptr_server = irc_server_new (server_tmp.name,
                                                 server_tmp.autoconnect,
                                                 server_tmp.autoreconnect,
                                                 server_tmp.autoreconnect_delay,
                                                 1, /* temp server */
                                                 server_tmp.address,
                                                 server_tmp.port,
                                                 server_tmp.ipv6,
                                                 server_tmp.ssl,
                                                 server_tmp.password,
                                                 server_tmp.nick1,
                                                 server_tmp.nick2,
                                                 server_tmp.nick3,
                                                 server_tmp.username,
                                                 server_tmp.realname,
                                                 server_tmp.hostname,
                                                 server_tmp.command,
                                                 1, /* command_delay */
                                                 server_tmp.autojoin,
                                                 1, /* autorejoin */
                                                 NULL);
                    if (ptr_server)
                    {
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                        gui_printf (NULL, _("Server %s%s%s created (temporary server, NOT SAVED!)\n"),
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                                    server_tmp.name,
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT));
                        if (!weechat_cmd_connect_one_server (window, ptr_server, 0))
                            connect_ok = 0;
                    }
                    else
                    {
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                        gui_printf (NULL,
                                    _("%s unable to create server \"%s\"\n"),
                                    WEECHAT_ERROR, argv[i]);
                    }
                }
            }
            else
            {
                if (ascii_strcasecmp (argv[i], "-port") == 0)
                    i++;
            }
        }
    }
    
    if (nb_connect == 0)
        connect_ok = weechat_cmd_connect_one_server (window, server, no_join);
    
    if (!connect_ok)
        return -1;
    
    return 0;
}

/*
 * weechat_cmd_dcc: DCC control (file or chat)
 */

int
weechat_cmd_dcc (t_irc_server *server, t_irc_channel *channel,
                 char *arguments)
{
    t_gui_buffer *buffer;
    char *pos_nick, *pos_file;
    
    gui_buffer_find_context (server, channel, NULL, &buffer);
    
    /* DCC SEND file */
    if (strncasecmp (arguments, "send", 4) == 0)
    {
        pos_nick = strchr (arguments, ' ');
        if (!pos_nick)
        {
            irc_display_prefix (NULL, server->buffer, GUI_PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s wrong argument count for \"%s\" command\n"),
                              WEECHAT_ERROR, "dcc send");
            return -1;
        }
        while (pos_nick[0] == ' ')
            pos_nick++;
        
        pos_file = strchr (pos_nick, ' ');
        if (!pos_file)
        {
            irc_display_prefix (NULL, server->buffer, GUI_PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s wrong argument count for \"%s\" command\n"),
                              WEECHAT_ERROR, "dcc send");
            return -1;
        }
        pos_file[0] = '\0';
        pos_file++;
        while (pos_file[0] == ' ')
            pos_file++;
        
        irc_dcc_send_request (server, IRC_DCC_FILE_SEND, pos_nick, pos_file);
    }
    /* DCC CHAT */
    else if (strncasecmp (arguments, "chat", 4) == 0)
    {
        pos_nick = strchr (arguments, ' ');
        if (!pos_nick)
        {
            irc_display_prefix (NULL, server->buffer, GUI_PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s wrong argument count for \"%s\" command\n"),
                              WEECHAT_ERROR, "dcc chat");
            return -1;
        }
        while (pos_nick[0] == ' ')
            pos_nick++;
        
        irc_dcc_send_request (server, IRC_DCC_CHAT_SEND, pos_nick, NULL);
    }
    /* close DCC CHAT */
    else if (ascii_strcasecmp (arguments, "close") == 0)
    {
        if (GUI_BUFFER_IS_PRIVATE(buffer) &&
            GUI_CHANNEL(buffer)->dcc_chat)
        {
            irc_dcc_close ((t_irc_dcc *)(GUI_CHANNEL(buffer)->dcc_chat),
                           IRC_DCC_ABORTED);
            irc_dcc_redraw (1);
        }
    }
    /* unknown DCC action */
    else
    {
        irc_display_prefix (NULL, server->buffer, GUI_PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s wrong arguments for \"%s\" command\n"),
                          WEECHAT_ERROR, "dcc");
        return -1;
    }
    
    return 0;
}

/*
 * weechat_cmd_debug_display_windows: display tree of windows
 */

void
weechat_cmd_debug_display_windows (t_gui_window_tree *tree, int indent)
{
    int i;
    
    if (tree)
    {
        for (i = 0; i < indent; i++)
            gui_printf_nolog (NULL, "  ");
        
        if (tree->window)
        {
            /* leaf */
            gui_printf_nolog (NULL, "leaf: %X (parent:%X), win=%X, child1=%X, child2=%X, %d,%d %dx%d, %d%%x%d%%\n",
                              tree, tree->parent_node, tree->window,
                              tree->child1, tree->child2,
                              tree->window->win_x, tree->window->win_y,
                              tree->window->win_width, tree->window->win_height,
                              tree->window->win_width_pct, tree->window->win_height_pct);
        }
        else
        {
            /* node */
            gui_printf_nolog (NULL, "node: %X (parent:%X), win=%X, child1=%X, child2=%X)\n",
                              tree, tree->parent_node, tree->window,
                              tree->child1, tree->child2);
        }
        
        if (tree->child1)
            weechat_cmd_debug_display_windows (tree->child1, indent + 1);
        if (tree->child2)
            weechat_cmd_debug_display_windows (tree->child2, indent + 1);
    }
}

/*
 * weechat_cmd_debug: print debug messages
 */

int
weechat_cmd_debug (t_irc_server *server, t_irc_channel *channel,
                   int argc, char **argv)
{
    t_irc_server *ptr_server;
    
    /* make C compiler happy */
    (void) server;
    (void) channel;
    
    if (argc != 1)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
        gui_printf (NULL,
                    _("%s wrong argument count for \"%s\" command\n"),
                    WEECHAT_ERROR, "debug");
        return -1;
    }
    
    if (ascii_strcasecmp (argv[0], "dump") == 0)
    {
        weechat_dump (0);
    }
    else if (ascii_strcasecmp (argv[0], "windows") == 0)
    {
        gui_printf_nolog (NULL, "\n");
        gui_printf_nolog (NULL, "DEBUG: windows tree:\n");
        weechat_cmd_debug_display_windows (gui_windows_tree, 1);
    }
    else if (ascii_strcasecmp (argv[0], "deloutq") == 0)
    {
        for (ptr_server = irc_servers; ptr_server;
             ptr_server = ptr_server->next_server)
        {
            irc_server_outqueue_free_all (ptr_server);
        }
        gui_printf_nolog (NULL, "\n");
        gui_printf_nolog (NULL, "DEBUG: outqueue DELETED for all servers.\n");
    }
    else
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
        gui_printf (NULL,
                    _("%s unknown option for \"%s\" command\n"),
                    WEECHAT_ERROR, "debug");
        return -1;
    }
    
    return 0;
}

/*
 * weechat_cmd_disconnect_one_server: disconnect from a server
 *                                    return 0 if error, 1 if ok
 */

int
weechat_cmd_disconnect_one_server (t_irc_server *server)
{
    if ((!server->is_connected) && (server->child_pid == 0)
        && (server->reconnect_start == 0))
    {
        irc_display_prefix (NULL, server->buffer, GUI_PREFIX_ERROR);
        gui_printf (server->buffer,
                    _("%s not connected to server \"%s\"!\n"),
                    WEECHAT_ERROR, server->name);
        return 0;
    }
    if (server->reconnect_start > 0)
    {
        irc_display_prefix (NULL, server->buffer, GUI_PREFIX_INFO);
        gui_printf (server->buffer,
                    _("Auto-reconnection is cancelled\n"));
    }
    irc_send_quit_server (server, NULL);
    irc_server_disconnect (server, 0);
    gui_status_draw (server->buffer, 1);
    
    /* disconnect ok */
    return 1;
}

/*
 * weechat_cmd_disconnect: disconnect from server(s)
 */

int
weechat_cmd_disconnect (t_irc_server *server, t_irc_channel *channel,
                        int argc, char **argv)
{
    t_gui_buffer *buffer;
    t_irc_server *ptr_server;
    int i, disconnect_ok;
    
    gui_buffer_find_context (server, channel, NULL, &buffer);
    
    if (argc == 0)
        disconnect_ok = weechat_cmd_disconnect_one_server (server);
    else
    {
        disconnect_ok = 1;
        
        if (ascii_strcasecmp (argv[0], "-all") == 0)
        {
            for (ptr_server = irc_servers; ptr_server;
                 ptr_server = ptr_server->next_server)
            {
                if ((ptr_server->is_connected) || (ptr_server->child_pid != 0)
                    || (ptr_server->reconnect_start != 0))
                {
                    if (!weechat_cmd_disconnect_one_server (ptr_server))
                        disconnect_ok = 0;
                }
            }
        }
        else
        {
            for (i = 0; i < argc; i++)
            {
                ptr_server = irc_server_search (argv[i]);
                if (ptr_server)
                {
                    if (!weechat_cmd_disconnect_one_server (ptr_server))
                        disconnect_ok = 0;
                }
                else
                {
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                    gui_printf (NULL, _("%s server \"%s\" not found\n"),
                                WEECHAT_ERROR, argv[i]);
                    disconnect_ok = 0;
                }
            }
        }
    }
    
    if (!disconnect_ok)
        return -1;
    
    return 0;
}

/*
 * weechat_cmd_help: display help
 */

int
weechat_cmd_help (t_irc_server *server, t_irc_channel *channel,
                  int argc, char **argv)
{
    int i;
#ifdef PLUGINS
    t_weechat_plugin *ptr_plugin;
    t_plugin_handler *ptr_handler;
#endif
    
    /* make C compiler happy */
    (void) server;
    (void) channel;
    
    switch (argc)
    {
        case 0:
            gui_printf (NULL, "\n");
            gui_printf (NULL, _("%s internal commands:\n"), PACKAGE_NAME);
            for (i = 0; weechat_commands[i].command_name; i++)
            {
                gui_printf (NULL, "   %s%s %s- %s\n",
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                            weechat_commands[i].command_name,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT),
                            _(weechat_commands[i].command_description));
            }
            gui_printf (NULL, "\n");
            gui_printf (NULL, _("IRC commands:\n"));
            for (i = 0; irc_commands[i].command_name; i++)
            {
                if (irc_commands[i].cmd_function_args || irc_commands[i].cmd_function_1arg)
                {
                    gui_printf (NULL, "   %s%s %s- %s\n",
                                GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                irc_commands[i].command_name,
                                GUI_COLOR(GUI_COLOR_WIN_CHAT),
                                _(irc_commands[i].command_description));
                }
            }
#ifdef PLUGINS
            gui_printf (NULL, "\n");
            gui_printf (NULL, _("Plugin commands:\n"));
            for (ptr_plugin = weechat_plugins; ptr_plugin;
                 ptr_plugin = ptr_plugin->next_plugin)
            {
                for (ptr_handler = ptr_plugin->handlers;
                     ptr_handler; ptr_handler = ptr_handler->next_handler)
                {
                    if (ptr_handler->type == PLUGIN_HANDLER_COMMAND)
                    {
                        gui_printf (NULL, "   %s%s",
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                    ptr_handler->command);
                        if (ptr_handler->description
                            && ptr_handler->description[0])
                            gui_printf (NULL, " %s- %s",
                                        GUI_COLOR(GUI_COLOR_WIN_CHAT),
                                        ptr_handler->description);
                        gui_printf (NULL, "\n");
                    }
                }
            }
#endif
            break;
        case 1:
#ifdef PLUGINS
            for (ptr_plugin = weechat_plugins; ptr_plugin;
                 ptr_plugin = ptr_plugin->next_plugin)
            {
                for (ptr_handler = ptr_plugin->handlers;
                     ptr_handler; ptr_handler = ptr_handler->next_handler)
                {
                    if ((ptr_handler->type == PLUGIN_HANDLER_COMMAND)
                        && (ascii_strcasecmp (ptr_handler->command, argv[0]) == 0))
                    {
                        gui_printf (NULL, "\n");
                        gui_printf (NULL, "[p]");
                        gui_printf (NULL, "  %s/%s",
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                    ptr_handler->command);
                        if (ptr_handler->arguments &&
                            ptr_handler->arguments[0])
                            gui_printf (NULL, "  %s%s\n",
                                        GUI_COLOR(GUI_COLOR_WIN_CHAT),
                                        ptr_handler->arguments);
                        else
                            gui_printf (NULL, "\n");
                        if (ptr_handler->description &&
                            ptr_handler->description[0])
                            gui_printf (NULL, "\n%s\n",
                                        ptr_handler->description);
                        if (ptr_handler->arguments_description &&
                            ptr_handler->arguments_description[0])
                            gui_printf (NULL, "\n%s\n",
                                        ptr_handler->arguments_description);
                        return 0;
                    }
                }
            }
#endif
            for (i = 0; weechat_commands[i].command_name; i++)
            {
                if (ascii_strcasecmp (weechat_commands[i].command_name, argv[0]) == 0)
                {
                    gui_printf (NULL, "\n");
                    gui_printf (NULL, "[w]");
                    gui_printf (NULL, "  %s/%s",
                                GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                weechat_commands[i].command_name);
                    if (weechat_commands[i].arguments &&
                        weechat_commands[i].arguments[0])
                        gui_printf (NULL, "  %s%s\n",
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT),
                                    _(weechat_commands[i].arguments));
                    else
                        gui_printf (NULL, "\n");
                    if (weechat_commands[i].command_description &&
                        weechat_commands[i].command_description[0])
                        gui_printf (NULL, "\n%s\n",
                                    _(weechat_commands[i].command_description));
                    if (weechat_commands[i].arguments_description &&
                        weechat_commands[i].arguments_description[0])
                        gui_printf (NULL, "\n%s\n",
                                    _(weechat_commands[i].arguments_description));
                    return 0;
                }
            }
            for (i = 0; irc_commands[i].command_name; i++)
            {
                if ((ascii_strcasecmp (irc_commands[i].command_name, argv[0]) == 0)
                    && (irc_commands[i].cmd_function_args || irc_commands[i].cmd_function_1arg))
                {
                    gui_printf (NULL, "\n");
                    gui_printf (NULL, "[i]");
                    gui_printf (NULL, "  %s/%s",
                                GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                irc_commands[i].command_name);
                    if (irc_commands[i].arguments &&
                        irc_commands[i].arguments[0])
                        gui_printf (NULL, "  %s%s\n",
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT),
                                    _(irc_commands[i].arguments));
                    else
                        gui_printf (NULL, "\n");
                    if (irc_commands[i].command_description &&
                        irc_commands[i].command_description[0])
                        gui_printf (NULL, "\n%s\n",
                                    _(irc_commands[i].command_description));
                    if (irc_commands[i].arguments_description &&
                        irc_commands[i].arguments_description[0])
                        gui_printf (NULL, "\n%s\n",
                                    _(irc_commands[i].arguments_description));
                    return 0;
                }
            }
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf (NULL,
                        _("No help available, \"%s\" is an unknown command\n"),
                        argv[0]);
            break;
    }
    return 0;
}

/*
 * weechat_cmd_history: display current buffer history
 */

int
weechat_cmd_history (t_irc_server *server, t_irc_channel *channel,
                     int argc, char **argv)
{
    t_gui_buffer *buffer;
    t_history *ptr_history;
    int n;
    int n_total;
    int n_user;
    
    gui_buffer_find_context (server, channel, NULL, &buffer);
    
    n_user = cfg_history_display_default;
    
    if (argc == 1)
    {
        if (ascii_strcasecmp (argv[0], "clear") == 0)
        {
            history_buffer_free (buffer);
            return 0;
        }
        else
            n_user = atoi (argv[0]);
    }
    
    if (buffer->history)
    {
        n_total = 1;
        for (ptr_history = buffer->history;
             ptr_history->next_history;
             ptr_history = ptr_history->next_history)
        {
            n_total++;
        }
        for (n = 0; ptr_history; ptr_history = ptr_history->prev_history, n++)
        {
            if ((n_user > 0) && ((n_total - n_user) > n))
                continue;
            irc_display_prefix (NULL, buffer, GUI_PREFIX_INFO);
            gui_printf_nolog (buffer, "%s\n", ptr_history->text);
        }
    }
    
    return 0;
}

/*
 * weechat_cmd_ignore_display: display an ignore entry
 */

void
weechat_cmd_ignore_display (char *text, t_irc_ignore *ptr_ignore)
{
    if (text)
        gui_printf (NULL, "%s%s ",
                    GUI_COLOR(GUI_COLOR_WIN_CHAT),
                    text);
    
    gui_printf (NULL, _("%son %s%s%s/%s%s%s:%s ignoring %s%s%s from %s%s\n"),
                GUI_COLOR(GUI_COLOR_WIN_CHAT),
                GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                ptr_ignore->server_name,
                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                ptr_ignore->channel_name,
                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                GUI_COLOR(GUI_COLOR_WIN_CHAT),
                GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                ptr_ignore->type,
                GUI_COLOR(GUI_COLOR_WIN_CHAT),
                GUI_COLOR(GUI_COLOR_WIN_CHAT_HOST),
                ptr_ignore->mask);
}

/*
 * weechat_cmd_ignore: ignore IRC commands and/or hosts
 */

int
weechat_cmd_ignore (t_irc_server *server, t_irc_channel *channel,
                    int argc, char **argv)
{
    t_gui_buffer *buffer;
    t_irc_ignore *ptr_ignore;
    int i;
    
    gui_buffer_find_context (server, channel, NULL, &buffer);
    
    ptr_ignore = NULL;
    switch (argc)
    {
        case 0:
            /* List all ignore */
            if (irc_ignore)
            {
                gui_printf (NULL, "\n");
                gui_printf (NULL, _("List of ignore:\n"));
                i = 0;
                for (ptr_ignore = irc_ignore; ptr_ignore;
                     ptr_ignore = ptr_ignore->next_ignore)
                {
                    i++;
                    gui_printf (NULL, "%s[%s%d%s] ",
                                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                                GUI_COLOR(GUI_COLOR_WIN_CHAT),
                                i,
                                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
                    weechat_cmd_ignore_display (NULL, ptr_ignore);
                }
            }
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                gui_printf (NULL, _("No ignore defined.\n"));
            }
            return 0;
            break;
        case 1:
            ptr_ignore = irc_ignore_add (argv[0], "*", "*",
                                         (GUI_SERVER(buffer)) ?
                                         GUI_SERVER(buffer)->name : "*");
            break;
        case 2:
            ptr_ignore = irc_ignore_add (argv[0], argv[1], "*",
                                         (GUI_SERVER(buffer)) ?
                                         GUI_SERVER(buffer)->name : "*");
            break;
        case 3:
            ptr_ignore = irc_ignore_add (argv[0], argv[1], argv[2],
                                         (GUI_SERVER(buffer)) ?
                                         GUI_SERVER(buffer)->name : "*");
            break;
        case 4:
            ptr_ignore = irc_ignore_add (argv[0], argv[1], argv[2], argv[3]);
            break;
    }
    if (ptr_ignore)
    {
        gui_printf (NULL, "\n");
        weechat_cmd_ignore_display (_("New ignore:"), ptr_ignore);
        return 0;
    }
    else
        return -1;
}

/*
 * weechat_cmd_key_display: display a key binding
 */

void
weechat_cmd_key_display (t_gui_key *key, int new_key)
{
    char *expanded_name;

    expanded_name = gui_keyboard_get_expanded_name (key->key);
    if (new_key)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
        gui_printf (NULL, _("New key binding:  %s"),
                    (expanded_name) ? expanded_name : key->key);
    }
    else
        gui_printf (NULL, "  %20s", (expanded_name) ? expanded_name : key->key);
    gui_printf (NULL, "%s => %s%s%s%s%s\n",
                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                GUI_COLOR(GUI_COLOR_WIN_CHAT),
                (key->function) ?
                gui_keyboard_function_search_by_ptr (key->function) : key->command,
                (key->args) ? " \"" : "",
                (key->args) ? key->args : "",
                (key->args) ? "\"" : "");
    if (expanded_name)
        free (expanded_name);
}

/*
 * weechat_cmd_key: bind/unbind keys
 */

int
weechat_cmd_key (t_irc_server *server, t_irc_channel *channel,
                 char *arguments)
{
    t_gui_window *window;
    t_gui_buffer *buffer;
    char *pos, *pos_args, *args_tmp, *internal_code;
    int i, length;
    t_gui_key *ptr_key;
    void (*ptr_function)(t_gui_window *, char *);
    
    gui_buffer_find_context (server, channel, &window, &buffer);
    
    if (arguments)
    {
        while (arguments[0] == ' ')
            arguments++;
    }

    if (!arguments || (arguments[0] == '\0'))
    {
        gui_printf (NULL, "\n");
        gui_printf (NULL, _("Key bindings:\n"));
        for (ptr_key = gui_keys; ptr_key; ptr_key = ptr_key->next_key)
        {
            weechat_cmd_key_display (ptr_key, 0);
        }
    }
    else if (ascii_strncasecmp (arguments, "unbind ", 7) == 0)
    {
        arguments += 7;
        while (arguments[0] == ' ')
            arguments++;
        if (gui_keyboard_unbind (arguments))
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
            gui_printf (NULL, _("Key \"%s\" unbound\n"), arguments);
        }
        else
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf (NULL,
                        _("%s unable to unbind key \"%s\"\n"),
                        WEECHAT_ERROR, arguments);
            return -1;
        }
    }
    else if (ascii_strcasecmp (arguments, "functions") == 0)
    {
        gui_printf (NULL, "\n");
        gui_printf (NULL, _("Internal key functions:\n"));
        i = 0;
        while (gui_key_functions[i].function_name)
        {
            gui_printf (NULL, "%25s  %s\n",
                        gui_key_functions[i].function_name,
                        _(gui_key_functions[i].description));
            i++;
        }
    }
    else if (ascii_strncasecmp (arguments, "call ", 5) == 0)
    {
        arguments += 5;
        while (arguments[0] == ' ')
            arguments++;
        pos = strchr (arguments, ' ');
        if (pos)
            pos[0] = '\0';
        ptr_function = gui_keyboard_function_search_by_name (arguments);
        if (pos)
            pos[0] = ' ';
        if (ptr_function)
        {
            pos_args = pos;
            args_tmp = NULL;
            if (pos_args)
            {
                pos_args++;
                while (pos_args[0] == ' ')
                    pos_args++;
                if (pos_args[0] == '"')
                {
                    length = strlen (pos_args);
                    if ((length > 1) && (pos_args[length - 1] == '"'))
                        args_tmp = strndup (pos_args + 1, length - 2);
                    else
                        args_tmp = strdup (pos_args);
                }
                else
                    args_tmp = strdup (pos_args);
            }
            (void)(*ptr_function)(window, args_tmp);
            if (args_tmp)
                free (args_tmp);
        }
        else
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf (NULL,
                        _("%s unknown key function \"%s\"\n"),
                        WEECHAT_ERROR, arguments);
            return -1;
        }
    }
    else if (ascii_strncasecmp (arguments, "reset", 5) == 0)
    {
        arguments += 5;
        while (arguments[0] == ' ')
            arguments++;
        if (ascii_strcasecmp (arguments, "-yes") == 0)
        {
            gui_keyboard_free_all ();
            gui_keyboard_init ();
            irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
            gui_printf (NULL, _("Default key bindings restored\n"));
        }
        else
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf (NULL,
                        _("%s \"-yes\" argument is required for keys reset (security reason)\n"),
                        WEECHAT_ERROR);
            return -1;
        }
    }
    else
    {
        while (arguments[0] == ' ')
            arguments++;
        pos = strchr (arguments, ' ');
        if (!pos)
        {
            ptr_key = NULL;
            internal_code = gui_keyboard_get_internal_code (arguments);
            if (internal_code)
                ptr_key = gui_keyboard_search (internal_code);
            if (ptr_key)
            {
                gui_printf (NULL, "\n");
                gui_printf (NULL, _("Key:\n"));
                weechat_cmd_key_display (ptr_key, 0);
            }
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                gui_printf (NULL, _("No key found.\n"));
            }
            if (internal_code)
                free (internal_code);
            return 0;
        }
        pos[0] = '\0';
        pos++;
        while (pos[0] == ' ')
            pos++;
        ptr_key = gui_keyboard_bind (arguments, pos);
        if (ptr_key)
            weechat_cmd_key_display (ptr_key, 1);
        else
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf (NULL,
                        _("%s unable to bind key \"%s\"\n"),
                        WEECHAT_ERROR, arguments);
            return -1;
        }
    }
    
    return 0;
}

/*
 * weechat_cmd_panel_display_info: display infos about a panel
 */

void
weechat_cmd_panel_display_info (t_gui_panel *panel)
{
    gui_printf (NULL, "  %s%2d%s. ",
                GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                panel->number,
                GUI_COLOR(GUI_COLOR_WIN_CHAT));
    gui_printf (NULL, "%s%s%s ",
                GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                panel->name,
                GUI_COLOR(GUI_COLOR_WIN_CHAT));
    gui_printf (NULL, "(%s%s/%s",
                (panel->panel_window) ? _("global") : _("local"),
                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                GUI_COLOR(GUI_COLOR_WIN_CHAT));
    switch (panel->position)
    {
        case GUI_PANEL_TOP:
            gui_printf (NULL, "%s", _("top"));
            break;
        case GUI_PANEL_BOTTOM:
            gui_printf (NULL, "%s", _("bottom"));
            break;
        case GUI_PANEL_LEFT:
            gui_printf (NULL, "%s", _("left"));
            break;
        case GUI_PANEL_RIGHT:
            gui_printf (NULL, "%s", _("right"));
            break;
    }
    gui_printf (NULL, "%s/%s%d)\n",
                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                GUI_COLOR(GUI_COLOR_WIN_CHAT),
                panel->size);
}

/*
 * weechat_cmd_panel: manage panels
 */

int
weechat_cmd_panel (t_irc_server *server, t_irc_channel *channel,
                   int argc, char **argv)
{
    t_gui_panel *ptr_panel;
    
    /* make C compiler happy */
    (void) server;
    (void) channel;

    gui_printf (NULL, "\n/panel command is under development!\n");
    
    if ((argc == 0) || ((argc == 1) && (ascii_strcasecmp (argv[0], "list") == 0)))
    {
        /* list open panels */
        
        gui_printf (NULL, "\n");
        gui_printf (NULL, _("Open panels:\n"));
        
        for (ptr_panel = gui_panels; ptr_panel; ptr_panel = ptr_panel->next_panel)
        {
            weechat_cmd_panel_display_info (ptr_panel);
        }
    }
    else
    {
    }
    return 0;
}

/*
 * weechat_cmd_plugin_list: list loaded plugins
 */

void
weechat_cmd_plugin_list (char *name, int full)
{
#ifdef PLUGINS
    t_weechat_plugin *ptr_plugin;
    int plugins_found;
    t_plugin_handler *ptr_handler;
    int handler_found;
    t_plugin_modifier *ptr_modifier;
    int modifier_found;
    
    gui_printf (NULL, "\n");
    if (!name)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
        gui_printf (NULL, _("Plugins loaded:\n"));
    }
    
    plugins_found = 0;
    
    for (ptr_plugin = weechat_plugins; ptr_plugin;
         ptr_plugin = ptr_plugin->next_plugin)
    {
        if (!name || (ascii_strcasestr (ptr_plugin->name, name)))
        {
            plugins_found++;
            
            /* plugin info */
            irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
            gui_printf (NULL, "  %s%s%s v%s - %s (%s)\n",
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                        ptr_plugin->name,
                        GUI_COLOR(GUI_COLOR_WIN_CHAT),
                        ptr_plugin->version,
                        ptr_plugin->description,
                        ptr_plugin->filename);
            
            if (full)
            {
                /* message handlers */
                irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                gui_printf (NULL, _("     message handlers:\n"));
                handler_found = 0;
                for (ptr_handler = ptr_plugin->handlers;
                     ptr_handler; ptr_handler = ptr_handler->next_handler)
                {
                    if (ptr_handler->type == PLUGIN_HANDLER_MESSAGE)
                    {
                        handler_found = 1;
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                        gui_printf (NULL, _("       IRC(%s)\n"),
                                    ptr_handler->irc_command);
                    }
                }
                if (!handler_found)
                {
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                    gui_printf (NULL, _("       (no message handler)\n"));
                }
                
                /* command handlers */
                irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                gui_printf (NULL, _("     command handlers:\n"));
                handler_found = 0;
                for (ptr_handler = ptr_plugin->handlers;
                     ptr_handler; ptr_handler = ptr_handler->next_handler)
                {
                    if (ptr_handler->type == PLUGIN_HANDLER_COMMAND)
                    {
                        handler_found = 1;
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                        gui_printf (NULL, "       /%s",
                                    ptr_handler->command);
                        if (ptr_handler->description
                            && ptr_handler->description[0])
                            gui_printf (NULL, " (%s)",
                                        ptr_handler->description);
                        gui_printf (NULL, "\n");
                    }
                }
                if (!handler_found)
                {
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                    gui_printf (NULL, _("       (no command handler)\n"));
                }
                
                /* timer handlers */
                irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                gui_printf (NULL, _("     timer handlers:\n"));
                handler_found = 0;
                for (ptr_handler = ptr_plugin->handlers;
                     ptr_handler; ptr_handler = ptr_handler->next_handler)
                {
                    if (ptr_handler->type == PLUGIN_HANDLER_TIMER)
                    {
                        handler_found = 1;
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                        gui_printf (NULL, _("       %d seconds\n"),
                                    ptr_handler->interval);
                    }
                }
                if (!handler_found)
                {
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                    gui_printf (NULL, _("       (no timer handler)\n"));
                }
                
                /* keyboard handlers */
                irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                gui_printf (NULL, _("     keyboard handlers:\n"));
                handler_found = 0;
                for (ptr_handler = ptr_plugin->handlers;
                     ptr_handler; ptr_handler = ptr_handler->next_handler)
                {
                    if (ptr_handler->type == PLUGIN_HANDLER_KEYBOARD)
                        handler_found++;
                }
                irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                if (!handler_found)
                    gui_printf (NULL, _("       (no keyboard handler)\n"));
                else
                    gui_printf (NULL, _("       %d defined\n"),
                                handler_found);

                /* event handlers */
                irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                gui_printf (NULL, _("     event handlers:\n"));
                handler_found = 0;
                for (ptr_handler = ptr_plugin->handlers;
                     ptr_handler; ptr_handler = ptr_handler->next_handler)
                {
                    if (ptr_handler->type == PLUGIN_HANDLER_EVENT)
                        handler_found++;
                }
                irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                if (!handler_found)
                    gui_printf (NULL, _("       (no event handler)\n"));
                else
                    gui_printf (NULL, _("       %d defined\n"),
                                handler_found);
                
                /* modifiers */
                irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                gui_printf (NULL, _("     modifiers:\n"));
                modifier_found = 0;
                for (ptr_modifier = ptr_plugin->modifiers;
                     ptr_modifier; ptr_modifier = ptr_modifier->next_modifier)
                {
                    modifier_found++;
                }
                irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
                if (!modifier_found)
                    gui_printf (NULL, _("       (no modifier)\n"));
                else
                    gui_printf (NULL, _("       %d defined\n"),
                                modifier_found);
            }
        }
    }
    if (plugins_found == 0)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_PLUGIN);
        if (name)
            gui_printf (NULL, _("No plugin found.\n"));
        else
            gui_printf (NULL, _("  (no plugin)\n"));
    }
#else
    /* make C compiler happy */
    (void) name;
    (void) full;
#endif
}
    
/*
 * weechat_cmd_plugin: list/load/unload WeeChat plugins
 */

int
weechat_cmd_plugin (t_irc_server *server, t_irc_channel *channel,
                    int argc, char **argv)
{
#ifdef PLUGINS
    /* make C compiler happy */
    (void) server;
    (void) channel;
    
    switch (argc)
    {
        case 0:
            weechat_cmd_plugin_list (NULL, 0);
            break;
        case 1:
            if (ascii_strcasecmp (argv[0], "list") == 0)
                weechat_cmd_plugin_list (NULL, 0);
            else if (ascii_strcasecmp (argv[0], "listfull") == 0)
                weechat_cmd_plugin_list (NULL, 1);
            else if (ascii_strcasecmp (argv[0], "autoload") == 0)
                plugin_auto_load ();
            else if (ascii_strcasecmp (argv[0], "reload") == 0)
            {
                plugin_unload_all ();
                plugin_auto_load ();
            }
            else if (ascii_strcasecmp (argv[0], "unload") == 0)
                plugin_unload_all ();
            break;
        case 2:
            if (ascii_strcasecmp (argv[0], "list") == 0)
                weechat_cmd_plugin_list (argv[1], 0);
            else if (ascii_strcasecmp (argv[0], "listfull") == 0)
                weechat_cmd_plugin_list (argv[1], 1);
            else if (ascii_strcasecmp (argv[0], "load") == 0)
                plugin_load (argv[1]);
            else if (ascii_strcasecmp (argv[0], "reload") == 0)
                plugin_reload_name (argv[1]);
            else if (ascii_strcasecmp (argv[0], "unload") == 0)
                plugin_unload_name (argv[1]);
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s unknown option for \"%s\" command\n"),
                            WEECHAT_ERROR, "plugin");
            }
            break;
        default:
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf (NULL,
                        _("%s wrong argument count for \"%s\" command\n"),
                        WEECHAT_ERROR, "plugin");
    }
#else
    irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
    gui_printf (NULL,
                _("Command \"%s\" is not available, WeeChat was built "
                  "without plugins support.\n"),
                "plugin");
    /* make C compiler happy */
    (void) server;
    (void) channel;
    (void) argc;
    (void) argv;
#endif /* PLUGINS */
    
    return 0;
}

/*
 * weechat_cmd_reconnect_one_server: reconnect to a server
 *                                   return 0 if error, 1 if ok
 */

int
weechat_cmd_reconnect_one_server (t_irc_server *server, int no_join)
{
    if ((!server->is_connected) && (server->child_pid == 0))
    {
        irc_display_prefix (NULL, server->buffer, GUI_PREFIX_ERROR);
        gui_printf (server->buffer,
                    _("%s not connected to server \"%s\"!\n"),
                    WEECHAT_ERROR, server->name);
        return 0;
    }
    irc_send_quit_server (server, NULL);
    irc_server_disconnect (server, 0);
    if (irc_server_connect (server, no_join))
    {
        server->reconnect_start = 0;
        server->reconnect_join = (server->channels) ? 1 : 0;    
    }
    gui_status_draw (server->buffer, 1);
    
    /* reconnect ok */
    return 1;
}

/*
 * weechat_cmd_reconnect: reconnect to server(s)
 */

int
weechat_cmd_reconnect (t_irc_server *server, t_irc_channel *channel,
                       int argc, char **argv)
{
    t_gui_buffer *buffer;
    t_irc_server *ptr_server;
    int i, nb_reconnect, reconnect_ok, all_servers, no_join;
    
    gui_buffer_find_context (server, channel, NULL, &buffer);   
    
    nb_reconnect = 0;
    reconnect_ok = 1;
    
    all_servers = 0;
    no_join = 0;
    for (i = 0; i < argc; i++)
    {
        if (ascii_strcasecmp (argv[i], "-all") == 0)
            all_servers = 1;
        if (ascii_strcasecmp (argv[i], "-nojoin") == 0)
            no_join = 1;
    }
    
    if (all_servers)
    {
        for (ptr_server = irc_servers; ptr_server;
             ptr_server = ptr_server->next_server)
        {
            nb_reconnect++;
            if ((ptr_server->is_connected) || (ptr_server->child_pid != 0))
            {
                if (!weechat_cmd_reconnect_one_server (ptr_server, no_join))
                    reconnect_ok = 0;
            }
        }
    }
    else
    {
        for (i = 0; i < argc; i++)
        {
            if (argv[i][0] != '-')
            {
                nb_reconnect++;
                ptr_server = irc_server_search (argv[i]);
                if (ptr_server)
                {
                    if (!weechat_cmd_reconnect_one_server (ptr_server, no_join))
                        reconnect_ok = 0;
                }
                else
                {
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                    gui_printf (NULL, _("%s server \"%s\" not found\n"),
                                WEECHAT_ERROR, argv[i]);
                    reconnect_ok = 0;
                }
            }
        }
    }
    
    if (nb_reconnect == 0)
        reconnect_ok = weechat_cmd_reconnect_one_server (server, no_join);
    
    if (!reconnect_ok)
        return -1;
    
    return 0;
}

/*
 * weechat_cmd_save: save WeeChat and plugins options to disk
 */

int
weechat_cmd_save (t_irc_server *server, t_irc_channel *channel,
                  int argc, char **argv)
{
    /* make C compiler happy */
    (void) server;
    (void) channel;
    
    if (config_write ((argc == 1) ? argv[0] : NULL) == 0)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
        gui_printf_nolog (NULL, _("Configuration file saved\n"));
    }
    else
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
        gui_printf_nolog (NULL, _("%s failed to save configuration file\n"),
                          WEECHAT_ERROR);
    }

#ifdef PLUGINS
    if (plugin_config_write () == 0)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
        gui_printf_nolog (NULL, _("Plugins options saved\n"));
    }
    else
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
        gui_printf_nolog (NULL, _("%s failed to save plugins options\n"),
                          WEECHAT_ERROR);
    }
#endif
    
    return 0;
}

/*
 * weechat_cmd_server: list, add or remove server(s)
 */

int
weechat_cmd_server (t_irc_server *server, t_irc_channel *channel,
                    int argc, char **argv)
{
    t_gui_window *window;
    t_gui_buffer *buffer;
    int i, detailed_list, one_server_found;
    t_irc_server server_tmp, *ptr_server, *server_found, *new_server;
    t_gui_buffer *ptr_buffer;
    char *server_name, *error;
    long number;
    
    gui_buffer_find_context (server, channel, &window, &buffer);
    
    if ((argc == 0) || (argc == 1)
        || (ascii_strcasecmp (argv[0], "list") == 0)
        || (ascii_strcasecmp (argv[0], "listfull") == 0))
    {
        /* list servers */
        server_name = NULL;
        detailed_list = 0;
        for (i = 0; i < argc; i++)
        {
            if (ascii_strcasecmp (argv[i], "list") == 0)
                continue;
            if (ascii_strcasecmp (argv[i], "listfull") == 0)
            {
                detailed_list = 1;
                continue;
            }
            if (!server_name)
                server_name = argv[i];
        }
        if (!server_name)
        {
            if (irc_servers)
            {
                gui_printf (NULL, "\n");
                gui_printf (NULL, _("All servers:\n"));
                for (ptr_server = irc_servers; ptr_server;
                     ptr_server = ptr_server->next_server)
                {
                    irc_display_server (ptr_server, detailed_list);
                }
            }
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                gui_printf (NULL, _("No server.\n"));
            }
        }
        else
        {
            one_server_found = 0;
            for (ptr_server = irc_servers; ptr_server;
                 ptr_server = ptr_server->next_server)
            {
                if (ascii_strcasestr (ptr_server->name, server_name))
                {
                    if (!one_server_found)
                    {
                        gui_printf (NULL, "\n");
                        gui_printf (NULL, _("Servers with '%s':\n"),
                                    server_name);
                    }
                    one_server_found = 1;
                    irc_display_server (ptr_server, detailed_list);
                }
            }
            if (!one_server_found)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                gui_printf (NULL, _("No server with '%s' found.\n"),
                            server_name);
            }
        }
    }
    else
    {
        if (ascii_strcasecmp (argv[0], "add") == 0)
        {
            if (argc < 3)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s missing parameters for \"%s\" command\n"),
                            WEECHAT_ERROR, "server");
                return -1;
            }
            
            if (irc_server_name_already_exists (argv[1]))
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s server \"%s\" already exists, can't create it!\n"),
                            WEECHAT_ERROR, argv[1]);
                return -1;
            }

            /* init server struct */
            irc_server_init (&server_tmp);
            
            server_tmp.name = strdup (argv[1]);
            server_tmp.address = strdup (argv[2]);
            server_tmp.port = IRC_DEFAULT_PORT;
            
            /* parse arguments */
            for (i = 3; i < argc; i++)
            {
                if (argv[i][0] == '-')
                {
                    if (ascii_strcasecmp (argv[i], "-temp") == 0)
                        server_tmp.temp_server = 1;
                    if (ascii_strcasecmp (argv[i], "-auto") == 0)
                        server_tmp.autoconnect = 1;
                    if (ascii_strcasecmp (argv[i], "-noauto") == 0)
                        server_tmp.autoconnect = 0;
                    if (ascii_strcasecmp (argv[i], "-ipv6") == 0)
                        server_tmp.ipv6 = 1;
                    if (ascii_strcasecmp (argv[i], "-ssl") == 0)
                        server_tmp.ssl = 1;
                    if (ascii_strcasecmp (argv[i], "-port") == 0)
                    {
                        if (i == (argc - 1))
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s missing argument for \"%s\" option\n"),
                                        WEECHAT_ERROR, "-port");
                            irc_server_destroy (&server_tmp);
                            return -1;
                        }
                        error = NULL;
                        number = strtol (argv[++i], &error, 10);
                        if ((error) && (error[0] == '\0'))
                            server_tmp.port = number;
                    }
                    if (ascii_strcasecmp (argv[i], "-pwd") == 0)
                    {
                        if (i == (argc - 1))
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s missing argument for \"%s\" option\n"),
                                        WEECHAT_ERROR, "-pwd");
                            irc_server_destroy (&server_tmp);
                            return -1;
                        }
                        server_tmp.password = strdup (argv[++i]);
                    }
                    if (ascii_strcasecmp (argv[i], "-nicks") == 0)
                    {
                        if (i >= (argc - 3))
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s missing argument for \"%s\" option\n"),
                                        WEECHAT_ERROR, "-nicks");
                            irc_server_destroy (&server_tmp);
                            return -1;
                        }
                        server_tmp.nick1 = strdup (argv[++i]);
                        server_tmp.nick2 = strdup (argv[++i]);
                        server_tmp.nick3 = strdup (argv[++i]);
                    }
                    if (ascii_strcasecmp (argv[i], "-username") == 0)
                    {
                        if (i == (argc - 1))
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s missing argument for \"%s\" option\n"),
                                        WEECHAT_ERROR, "-username");
                            irc_server_destroy (&server_tmp);
                            return -1;
                        }
                        server_tmp.username = strdup (argv[++i]);
                    }
                    if (ascii_strcasecmp (argv[i], "-realname") == 0)
                    {
                        if (i == (argc - 1))
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s missing argument for \"%s\" option\n"),
                                        WEECHAT_ERROR, "-realname");
                            irc_server_destroy (&server_tmp);
                            return -1;
                        }
                        server_tmp.realname = strdup (argv[++i]);
                    }
                    if (ascii_strcasecmp (argv[i], "-command") == 0)
                    {
                        if (i == (argc - 1))
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s missing argument for \"%s\" option\n"),
                                        WEECHAT_ERROR, "-command");
                            irc_server_destroy (&server_tmp);
                            return -1;
                        }
                        server_tmp.command = strdup (argv[++i]);
                    }
                    if (ascii_strcasecmp (argv[i], "-autojoin") == 0)
                    {
                        if (i == (argc - 1))
                        {
                            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                            gui_printf (NULL,
                                        _("%s missing argument for \"%s\" option\n"),
                                        WEECHAT_ERROR, "-autojoin");
                            irc_server_destroy (&server_tmp);
                            return -1;
                        }
                        server_tmp.autojoin = strdup (argv[++i]);
                    }
                }
            }
            
            /* create new server */
            new_server = irc_server_new (server_tmp.name,
                                         server_tmp.autoconnect,
                                         server_tmp.autoreconnect,
                                         server_tmp.autoreconnect_delay,
                                         server_tmp.temp_server,
                                         server_tmp.address,
                                         server_tmp.port,
                                         server_tmp.ipv6,
                                         server_tmp.ssl,
                                         server_tmp.password,
                                         server_tmp.nick1,
                                         server_tmp.nick2,
                                         server_tmp.nick3,
                                         server_tmp.username,
                                         server_tmp.realname,
                                         server_tmp.hostname,
                                         server_tmp.command,
                                         1, /* command_delay */
                                         server_tmp.autojoin,
                                         1, /* autorejoin */
                                         NULL);
            if (new_server)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                gui_printf (NULL, _("Server %s%s%s created\n"),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                            server_tmp.name,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT));
            }
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s unable to create server\n"),
                            WEECHAT_ERROR);
                irc_server_destroy (&server_tmp);
                return -1;
            }
            
            if (new_server->autoconnect)
            {
                (void) gui_buffer_new (window, new_server, NULL,
                                       GUI_BUFFER_TYPE_STANDARD, 1);
                irc_server_connect (new_server, 0);
            }
            
            irc_server_destroy (&server_tmp);
        }
        else if (ascii_strcasecmp (argv[0], "copy") == 0)
        {
            if (argc < 3)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s missing server name for \"%s\" command\n"),
                            WEECHAT_ERROR, "server copy");
                return -1;
            }
            
            /* look for server by name */
            server_found = irc_server_search (argv[1]);
            if (!server_found)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s server \"%s\" not found for \"%s\" command\n"),
                            WEECHAT_ERROR, argv[1], "server copy");
                return -1;
            }
            
            /* check if target name already exists */
            if (irc_server_search (argv[2]))
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s server \"%s\" already exists for \"%s\" command\n"),
                            WEECHAT_ERROR, argv[2], "server copy");
                return -1;
            }
            
            /* duplicate server */
            new_server = irc_server_duplicate (server_found, argv[2]);
            if (new_server)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                gui_printf (NULL, _("Server %s%s%s has been copied to %s%s\n"),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                            argv[1],
                            GUI_COLOR(GUI_COLOR_WIN_CHAT),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                            argv[2]);
                gui_window_redraw_all_buffers ();
                return 0;
            }
            
            return -1;
        }
        else if (ascii_strcasecmp (argv[0], "rename") == 0)
        {
            if (argc < 3)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s missing server name for \"%s\" command\n"),
                            WEECHAT_ERROR, "server rename");
                return -1;
            }
            
            /* look for server by name */
            server_found = irc_server_search (argv[1]);
            if (!server_found)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s server \"%s\" not found for \"%s\" command\n"),
                            WEECHAT_ERROR, argv[1], "server rename");
                return -1;
            }
            
            /* check if target name already exists */
            if (irc_server_search (argv[2]))
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s server \"%s\" already exists for \"%s\" command\n"),
                            WEECHAT_ERROR, argv[2], "server rename");
                return -1;
            }

            /* rename server */
            if (irc_server_rename (server_found, argv[2]))
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
                gui_printf (NULL, _("Server %s%s%s has been renamed to %s%s\n"),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                            argv[1],
                            GUI_COLOR(GUI_COLOR_WIN_CHAT),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                            argv[2]);
                gui_window_redraw_all_buffers ();
                return 0;
            }
            
            return -1;
        }
        else if (ascii_strcasecmp (argv[0], "keep") == 0)
        {
            if (argc < 2)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s missing server name for \"%s\" command\n"),
                            WEECHAT_ERROR, "server keep");
                return -1;
            }
            
            /* look for server by name */
            server_found = irc_server_search (argv[1]);
            if (!server_found)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s server \"%s\" not found for \"%s\" command\n"),
                            WEECHAT_ERROR, argv[1], "server keep");
                return -1;
            }

            /* check that it is temporary server */
            if (!server_found->temp_server)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s server \"%s\" is not a temporary server\n"),
                            WEECHAT_ERROR, argv[1]);
                return -1;
            }
            
            /* remove temporary flag on server */
            server_found->temp_server = 0;

            irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
            gui_printf (NULL, _("Server %s%s%s is not temporary any more\n"),
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                        argv[1],
                        GUI_COLOR(GUI_COLOR_WIN_CHAT));
            
            return 0;
        }
        else if (ascii_strcasecmp (argv[0], "del") == 0)
        {
            if (argc < 2)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s missing server name for \"%s\" command\n"),
                            WEECHAT_ERROR, "server del");
                return -1;
            }
            
            /* look for server by name */
            server_found = irc_server_search (argv[1]);
            if (!server_found)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s server \"%s\" not found for \"%s\" command\n"),
                            WEECHAT_ERROR, argv[1], "server del");
                return -1;
            }
            if (server_found->is_connected)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s you can not delete server \"%s\" because you are connected to. "
                            "Try /disconnect %s before.\n"),
                            WEECHAT_ERROR, argv[1], argv[1]);
                return -1;
            }
            
            for (ptr_buffer = gui_buffers; ptr_buffer; ptr_buffer = ptr_buffer->next_buffer)
            {
                if (GUI_SERVER(ptr_buffer) == server_found)
                {
                    ptr_buffer->server = NULL;
                    ptr_buffer->channel = NULL;
                }
            }
            
            server_name = strdup (server_found->name);
            
            irc_server_free (server_found);
            
            irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
            gui_printf (NULL, _("Server %s%s%s has been deleted\n"),
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                        server_name,
                        GUI_COLOR(GUI_COLOR_WIN_CHAT));
            if (server_name)
                free (server_name);
            
            gui_window_redraw_buffer (buffer);
            
            return 0;
        }
        else
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf (NULL,
                        _("%s unknown option for \"%s\" command\n"),
                        WEECHAT_ERROR, "server");
            return -1;
        }
    }
    return 0;
}

/*
 * weechat_cmd_set_display_option: display config option
 */

void
weechat_cmd_set_display_option (t_config_option *option, char *prefix, void *value)
{
    char *color_name, *value2;
    
    gui_printf (NULL, "  %s%s%s%s = ",
                (prefix) ? prefix : "",
                (prefix) ? "." : "",
                option->option_name,
                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
    if (!value)
    {
        if (option->option_type == OPTION_TYPE_STRING)
            value = option->ptr_string;
        else
            value = option->ptr_int;
    }
    switch (option->option_type)
    {
        case OPTION_TYPE_BOOLEAN:
            gui_printf (NULL, "%s%s\n",
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_HOST),
                        (*((int *)value)) ? "ON" : "OFF");
            break;
        case OPTION_TYPE_INT:
            gui_printf (NULL, "%s%d\n",
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_HOST),
                        *((int *)value));
            break;
        case OPTION_TYPE_INT_WITH_STRING:
            gui_printf (NULL, "%s%s\n",
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_HOST),
                        option->array_values[*((int *)value)]);
            break;
        case OPTION_TYPE_COLOR:
            color_name = gui_color_get_name (*((int *)value));
            gui_printf (NULL, "%s%s\n",
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_HOST),
                        (color_name) ? color_name : _("(unknown)"));
            break;
        case OPTION_TYPE_STRING:
            if (*((char **)value))
            {
                value2 = strdup (*((char **)value));
                if (value2)
                {
                    if (cfg_log_hide_nickserv_pwd)
                    {
                        irc_display_hide_password (value2, 1);
                        if (strcmp (*((char **)value), value2) != 0)
                            gui_printf (NULL, _("%s(password hidden) "),
                                        GUI_COLOR(GUI_COLOR_WIN_CHAT));
                    }
                    gui_printf (NULL, "%s\"%s%s%s\"",
                                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                                GUI_COLOR(GUI_COLOR_WIN_CHAT_HOST),
                                value2,
                                GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
                    free (value2);
                }
            }
            else
                gui_printf (NULL, "%s\"\"",
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
            gui_printf (NULL, "\n");
            break;
    }
}

/*
 * weechat_cmd_set: set config options
 */

int
weechat_cmd_set (t_irc_server *server, t_irc_channel *channel,
                 char *arguments)
{
    char *option, *value, *pos;
    int i, j, section_displayed;
    t_config_option *ptr_option;
    t_irc_server *ptr_server;
    char option_name[256];
    void *ptr_option_value;
    int last_section, last_option, number_found;
    
    /* make C compiler happy */
    (void) server;
    (void) channel;
    
    option = NULL;
    value = NULL;
    if (arguments && arguments[0])
    {
        option = arguments;
        value = strchr (option, '=');
        if (value)
        {
            value[0] = '\0';
            
            /* remove spaces before '=' */
            pos = value - 1;
            while ((pos > option) && (pos[0] == ' '))
            {
                pos[0] = '\0';
                pos--;
            }
            
            /* skip spaces after '=' */
            value++;
            while (value[0] && (value[0] == ' '))
            {
                value++;
            }
            
            /* remove simple or double quotes 
               and spaces at the end */
            if (strlen(value) > 1)
            {
                pos = value + strlen (value) - 1;
                while ((pos > value) && (pos[0] == ' '))
                {
                    pos[0] = '\0';
                    pos--;
                }
                pos = value + strlen (value) - 1;
                if (((value[0] == '\'') &&
                     (pos[0] == '\'')) ||
                    ((value[0] == '"') &&
                     (pos[0] == '"')))
                {
                    pos[0] = '\0';
                    value++;
                }
            }
        }
    }
    
    if (value)
    {
        pos = strrchr (option, '.');
        if (pos)
        {
            /* server config option modification */
            pos[0] = '\0';
            ptr_server = irc_server_search (option);
            if (!ptr_server)
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL,
                            _("%s server \"%s\" not found\n"),
                            WEECHAT_ERROR, option);
            }
            else
            {
                switch (config_set_server_value (ptr_server, pos + 1, value))
                {
                    case 0:
                        gui_printf (NULL, "\n");
                        gui_printf (NULL, "%s[%s%s %s%s%s]\n",
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                    config_sections[CONFIG_SECTION_SERVER].section_name,
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                                    ptr_server->name,
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
                        for (i = 0; weechat_options[CONFIG_SECTION_SERVER][i].option_name; i++)
                        {
                            if (strcmp (weechat_options[CONFIG_SECTION_SERVER][i].option_name, pos + 1) == 0)
                                break;
                        }
                        if (weechat_options[CONFIG_SECTION_SERVER][i].option_name)
                        {
                            ptr_option_value = config_get_server_option_ptr (ptr_server,
                                weechat_options[CONFIG_SECTION_SERVER][i].option_name);
                            weechat_cmd_set_display_option (&weechat_options[CONFIG_SECTION_SERVER][i],
                                                            ptr_server->name,
                                                            ptr_option_value);
                        }
                        config_change_buffer_content ();
                        break;
                    case -1:
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                        gui_printf (NULL, _("%s config option \"%s\" not found\n"),
                                    WEECHAT_ERROR, pos + 1);
                        break;
                    case -2:
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                        gui_printf (NULL, _("%s incorrect value for option \"%s\"\n"),
                                    WEECHAT_ERROR, pos + 1);
                        break;
                }
            }
            pos[0] = '.';
        }
        else
        {
            ptr_option = config_option_search (option);
            if (ptr_option)
            {
                if (ptr_option->handler_change == NULL)
                {
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                    gui_printf (NULL,
                                _("%s option \"%s\" can not be changed while WeeChat is running\n"),
                                WEECHAT_ERROR, option);
                }
                else
                {
                    if (config_option_set_value (ptr_option, value) == 0)
                    {
                        gui_printf (NULL, "\n");
                        gui_printf (NULL, "%s[%s%s%s]\n",
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                    config_get_section (ptr_option),
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
                        weechat_cmd_set_display_option (ptr_option, NULL, NULL);
                        (void) (ptr_option->handler_change());
                    }
                    else
                    {
                        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                        gui_printf (NULL, _("%s incorrect value for option \"%s\"\n"),
                                    WEECHAT_ERROR, option);
                    }
                }
            }
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL, _("%s config option \"%s\" not found\n"),
                            WEECHAT_ERROR, option);
            }
        }
    }
    else
    {
        last_section = -1;
        last_option = -1;
        number_found = 0;
        for (i = 0; i < CONFIG_NUMBER_SECTIONS; i++)
        {
            section_displayed = 0;
            if ((i != CONFIG_SECTION_KEYS) && (i != CONFIG_SECTION_ALIAS)
                && (i != CONFIG_SECTION_IGNORE) && (i != CONFIG_SECTION_SERVER))
            {
                for (j = 0; weechat_options[i][j].option_name; j++)
                {
                    if ((!option) ||
                        ((option) && (option[0])
                         && (strstr (weechat_options[i][j].option_name, option)
                             != NULL)))
                    {
                        if (!section_displayed)
                        {
                            gui_printf (NULL, "\n");
                            gui_printf (NULL, "%s[%s%s%s]\n",
                                        GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                                        GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                        config_sections[i].section_name,
                                        GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
                            section_displayed = 1;
                        }
                        weechat_cmd_set_display_option (&weechat_options[i][j], NULL, NULL);
                        last_section = i;
                        last_option = j;
                        number_found++;
                    }
                }
            }
        }
        for (ptr_server = irc_servers; ptr_server;
             ptr_server = ptr_server->next_server)
        {
            section_displayed = 0;
            for (i = 0; weechat_options[CONFIG_SECTION_SERVER][i].option_name; i++)
            {
                snprintf (option_name, sizeof (option_name), "%s.%s",
                          ptr_server->name, 
                          weechat_options[CONFIG_SECTION_SERVER][i].option_name);
                if ((!option) ||
                        ((option) && (option[0])
                         && (strstr (option_name, option) != NULL)))
                {
                    if (!section_displayed)
                    {
                        gui_printf (NULL, "\n");
                        gui_printf (NULL, "%s[%s%s %s%s%s]\n",
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                                    config_sections[CONFIG_SECTION_SERVER].section_name,
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_SERVER),
                                    ptr_server->name,
                                    GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
                        section_displayed = 1;
                    }
                    ptr_option_value = config_get_server_option_ptr (ptr_server,
                        weechat_options[CONFIG_SECTION_SERVER][i].option_name);
                    if (ptr_option_value)
                    {
                        weechat_cmd_set_display_option (&weechat_options[CONFIG_SECTION_SERVER][i],
                                                        ptr_server->name,
                                                        ptr_option_value);
                        last_section = CONFIG_SECTION_SERVER;
                        last_option = i;
                        number_found++;
                    }
                }
            }
        }
        if (number_found == 0)
        {
            if (option)
                gui_printf (NULL, _("No config option found with \"%s\"\n"),
                            option);
            else
                gui_printf (NULL, _("No config option found\n"));
        }
        else
        {
            if ((number_found == 1) && (last_section >= 0) && (last_option >= 0))
            {
                gui_printf (NULL, "\n");
                gui_printf (NULL, _("%sDetail:\n"),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL));
                switch (weechat_options[last_section][last_option].option_type)
                {
                    case OPTION_TYPE_BOOLEAN:
                        gui_printf (NULL, _("  . type boolean (values: 'on' or 'off')\n"));
                        gui_printf (NULL, _("  . default value: '%s'\n"),
                                    (weechat_options[last_section][last_option].default_int == BOOL_TRUE) ?
                                    "on" : "off");
                        break;
                    case OPTION_TYPE_INT:
                        gui_printf (NULL, _("  . type integer (values: between %d and %d)\n"),
                                    weechat_options[last_section][last_option].min,
                                    weechat_options[last_section][last_option].max);
                        gui_printf (NULL, _("  . default value: %d\n"),
                                    weechat_options[last_section][last_option].default_int);
                        break;
                    case OPTION_TYPE_INT_WITH_STRING:
                        gui_printf (NULL, _("  . type string (values: "));
                        i = 0;
                        while (weechat_options[last_section][last_option].array_values[i])
                        {
                            gui_printf (NULL, "'%s'",
                                        weechat_options[last_section][last_option].array_values[i]);
                            if (weechat_options[last_section][last_option].array_values[i + 1])
                                gui_printf (NULL, ", ");
                            i++;
                        }
                        gui_printf (NULL, ")\n");
                        gui_printf (NULL, _("  . default value: '%s'\n"),
                            (weechat_options[last_section][last_option].default_string) ?
                            weechat_options[last_section][last_option].default_string : _("empty"));
                        break;
                    case OPTION_TYPE_COLOR:
                        gui_printf (NULL, _("  . type color (Curses or Gtk color, look at WeeChat doc)\n"));
                        gui_printf (NULL, _("  . default value: '%s'\n"),
                            (weechat_options[last_section][last_option].default_string) ?
                            weechat_options[last_section][last_option].default_string : _("empty"));
                        break;
                    case OPTION_TYPE_STRING:
                        gui_printf (NULL, _("  . type string (any string)\n"));
                        gui_printf (NULL, _("  . default value: '%s'\n"),
                                    (weechat_options[last_section][last_option].default_string) ?
                                    weechat_options[last_section][last_option].default_string : _("empty"));
                        break;
                }
                gui_printf (NULL, _("  . description: %s\n"),
                            _(weechat_options[last_section][last_option].long_description));
            }
            else
            {
                gui_printf (NULL, "\n");
                gui_printf (NULL, "%s%d %s",
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                            number_found,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT));
                if (option)
                    gui_printf (NULL, _("config option(s) found with \"%s\"\n"),
                                option);
                else
                    gui_printf (NULL, _("config option(s) found\n"));
            }
        }
    }
    return 0;
}

/*
 * weechat_cmd_setp: set plugin options
 */

int
weechat_cmd_setp (t_irc_server *server, t_irc_channel *channel,
                  char *arguments)
{
#ifdef PLUGINS
    char *option, *value, *pos, *ptr_name;
    t_plugin_option *ptr_option;
    int number_found;
    
    /* make C compiler happy */
    (void) server;
    (void) channel;
    
    option = NULL;
    value = NULL;
    if (arguments && arguments[0])
    {
        option = arguments;
        value = strchr (option, '=');
        if (value)
        {
            value[0] = '\0';
            
            /* remove spaces before '=' */
            pos = value - 1;
            while ((pos > option) && (pos[0] == ' '))
            {
                pos[0] = '\0';
                pos--;
            }
            
            /* skip spaces after '=' */
            value++;
            while (value[0] && (value[0] == ' '))
            {
                value++;
            }
            
            /* remove simple or double quotes 
               and spaces at the end */
            if (strlen(value) > 1)
            {
                pos = value + strlen (value) - 1;
                while ((pos > value) && (pos[0] == ' '))
                {
                    pos[0] = '\0';
                    pos--;
                }
                pos = value + strlen (value) - 1;
                if (((value[0] == '\'') &&
                     (pos[0] == '\'')) ||
                    ((value[0] == '"') &&
                     (pos[0] == '"')))
                {
                    pos[0] = '\0';
                    value++;
                }
            }
        }
    }
    
    if (value)
    {
        ptr_name = NULL;
        ptr_option = plugin_config_search_internal (option);
        if (ptr_option)
            ptr_name = ptr_option->name;
        else
        {
            pos = strchr (option, '.');
            if (pos)
                pos[0] = '\0';
            if (!pos || !pos[1] || (!plugin_search (option)))
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL, _("%s plugin \"%s\" not found\n"),
                            WEECHAT_ERROR, option);
            }
            else
                ptr_name = option;
            if (pos)
                pos[0] = '.';
        }
        if (ptr_name)
        {
            if (plugin_config_set_internal (ptr_name, value))
            {
                gui_printf (NULL, "\n  %s%s = \"%s%s%s\"\n",
                            ptr_name,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_HOST),
                            value,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
            }
            else
            {
                irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                gui_printf (NULL, _("%s incorrect value for plugin option \"%s\"\n"),
                            WEECHAT_ERROR, ptr_name);
            }
        }
    }
    else
    {
        number_found = 0;
        for (ptr_option = plugin_options; ptr_option;
             ptr_option = ptr_option->next_option)
        {
            if ((!option) ||
                ((option) && (option[0])
                 && (strstr (ptr_option->name, option) != NULL)))
            {
                if (number_found == 0)
                    gui_printf (NULL, "\n");
                gui_printf (NULL, "  %s%s = \"%s%s%s\"\n",
                            ptr_option->name,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_HOST),
                            ptr_option->value,
                            GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
                number_found++;
            }
        }
        if (number_found == 0)
        {
            if (option)
                gui_printf (NULL, _("No plugin option found with \"%s\"\n"),
                            option);
            else
                gui_printf (NULL, _("No plugin option found\n"));
        }
        else
        {
            gui_printf (NULL, "\n");
            gui_printf (NULL, "%s%d %s",
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                        number_found,
                        GUI_COLOR(GUI_COLOR_WIN_CHAT));
            if (option)
                gui_printf (NULL, _("plugin option(s) found with \"%s\"\n"),
                            option);
            else
                gui_printf (NULL, _("plugin option(s) found\n"));
        }
    }
#else
    /* make C compiler happy */
    (void) server;
    (void) channel;
    (void) arguments;
    
    irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
    gui_printf (NULL,
                _("Command \"%s\" is not available, WeeChat was built "
                  "without plugins support.\n"),
                "setp");
#endif
    
    return 0;
}

/*
 * cmd_unalias: remove an alias
 */

int
weechat_cmd_unalias (t_irc_server *server, t_irc_channel *channel,
                     char *arguments)
{
    t_weelist *ptr_weelist;
    t_weechat_alias *ptr_alias;
    
    /* make C compiler happy */
    (void) server;
    (void) channel;
    
    if (arguments[0] == '/')
        arguments++;
    
    ptr_weelist = weelist_search (index_commands, arguments);
    if (!ptr_weelist)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
        gui_printf (NULL, _("%s alias or command \"%s\" not found\n"),
                    WEECHAT_ERROR, arguments);
        return -1;
    }
    
    weelist_remove (&index_commands, &last_index_command, ptr_weelist);
    ptr_alias = alias_search (arguments);
    if (ptr_alias)
        alias_free (ptr_alias);
    irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
    gui_printf (NULL, _("Alias \"%s\" removed\n"),
                arguments);
    return 0;
}

/*
 * weechat_cmd_unignore: unignore IRC commands and/or hosts
 */

int
weechat_cmd_unignore (t_irc_server *server, t_irc_channel *channel,
                      int argc, char **argv)
{
    t_gui_buffer *buffer;
    char *error;
    int number, ret;
    
    gui_buffer_find_context (server, channel, NULL, &buffer);
    
    ret = 0;
    switch (argc)
    {
        case 0:
            /* List all ignore */
            weechat_cmd_ignore (server, channel, argc, argv);
            return 0;
            break;
        case 1:
            error = NULL;
            number = strtol (argv[0], &error, 10);
            if ((error) && (error[0] == '\0'))
                ret = irc_ignore_search_free_by_number (number);
            else
                ret = irc_ignore_search_free (argv[0], "*", "*",
                                              (GUI_SERVER(buffer)) ?
                                              GUI_SERVER(buffer)->name : "*");
            break;
        case 2:
            ret = irc_ignore_search_free (argv[0], argv[1], "*",
                                          (GUI_SERVER(buffer)) ?
                                          GUI_SERVER(buffer)->name : "*");
            break;
        case 3:
            ret = irc_ignore_search_free (argv[0], argv[1], argv[2],
                                          (GUI_SERVER(buffer)) ?
                                          GUI_SERVER(buffer)->name : "*");
            break;
        case 4:
            ret = irc_ignore_search_free (argv[0], argv[1], argv[2], argv[3]);
            break;
    }
    
    if (ret)
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
        gui_printf (NULL,
                    NG_("%s%d%s ignore was removed.\n",
                        "%s%d%s ignore were removed.\n",
                        ret),
                    GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                    ret,
                    GUI_COLOR(GUI_COLOR_WIN_CHAT));
    }
    else
    {
        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
        gui_printf (NULL, _("%s no ignore found\n"),
                    WEECHAT_ERROR);
        return -1;
    }
    
    return 0;
}

/*
 * weechat_cmd_upgrade: upgrade WeeChat
 */

int
weechat_cmd_upgrade (t_irc_server *server, t_irc_channel *channel,
                     int argc, char **argv)
{
    t_irc_server *ptr_server;
    int filename_length;
    char *filename, *ptr_binary;
    char *exec_args[7] = { NULL, "-a", "--dir", NULL, "--session", NULL, NULL };
    
    /* make C compiler happy */
    (void) server;
    (void) channel;
    
    ptr_binary = (argc > 0) ? argv[0] : weechat_argv0;
    
    for (ptr_server = irc_servers; ptr_server;
         ptr_server = ptr_server->next_server)
    {
        if (ptr_server->child_pid != 0)
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf_nolog (NULL,
                              _("%s can't upgrade: connection to at least "
                                "one server is pending\n"),
                              WEECHAT_ERROR);
            return -1;
        }
        /* TODO: remove this test, and fix gnutls save/load in session */
        if (ptr_server->is_connected && ptr_server->ssl_connected)
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf_nolog (NULL,
                              _("%s can't upgrade: connection to at least "
                                "one SSL server is active "
                                "(should be fixed in a future version)\n"),
                              WEECHAT_ERROR);
            return -1;
        }
        if (ptr_server->outqueue)
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf_nolog (NULL,
                              _("%s can't upgrade: anti-flood is active on "
                                "at least one server (sending many lines)\n"),
                              WEECHAT_ERROR);
            return -1;
        }
    }
    
    filename_length = strlen (weechat_home) + strlen (WEECHAT_SESSION_NAME) + 2;
    filename = (char *) malloc (filename_length * sizeof (char));
    if (!filename)
        return -2;
    snprintf (filename, filename_length, "%s%s" WEECHAT_SESSION_NAME,
              weechat_home, DIR_SEPARATOR);
    
    irc_display_prefix (NULL, NULL, GUI_PREFIX_INFO);
    gui_printf_nolog (NULL, _("Upgrading WeeChat...\n"));
    
    if (!session_save (filename))
    {
        free (filename);
        irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
        gui_printf_nolog (NULL,
                          _("%s unable to save session in file\n"),
                          WEECHAT_ERROR);
        return -1;
    }
    
    exec_args[0] = strdup (ptr_binary);
    exec_args[3] = strdup (weechat_home);
    exec_args[5] = strdup (filename);
    
    /* unload plugins, save config, then upgrade */
#ifdef PLUGINS    
    plugin_end ();
#endif
    if (cfg_look_save_on_exit)
        (void) config_write (NULL);
    gui_main_end ();
    fifo_remove ();
    weechat_log_close ();
    
    execvp (exec_args[0], exec_args);
    
    /* this code should not be reached if execvp is ok */
#ifdef PLUGINS
    plugin_init (1);
#endif
    
    weechat_iconv_fprintf (stderr,
                           _("%s exec failed (program: \"%s\"), exiting WeeChat\n"),
                           WEECHAT_ERROR,
                           exec_args[0]);
    
    free (exec_args[0]);
    free (exec_args[3]);
    free (filename);
    
    exit (EXIT_FAILURE);
    
    /* never executed */
    return -1;
}

/*
 * weechat_cmd_uptime: display WeeChat uptime
 */

int
weechat_cmd_uptime (t_irc_server *server, t_irc_channel *channel,
                    int argc, char **argv)
{
    t_gui_buffer *buffer;
    time_t running_time;
    int day, hour, min, sec;
    char string[256];
    
    gui_buffer_find_context (server, channel, NULL, &buffer);
    
    running_time = time (NULL) - weechat_start_time;
    day = running_time / (60 * 60 * 24);
    hour = (running_time % (60 * 60 * 24)) / (60 * 60);
    min = ((running_time % (60 * 60 * 24)) % (60 * 60)) / 60;
    sec = ((running_time % (60 * 60 * 24)) % (60 * 60)) % 60;
    
    if ((argc == 1) && (strcmp (argv[0], "-o") == 0)
        && ((GUI_BUFFER_IS_CHANNEL(buffer))
            || (GUI_BUFFER_IS_PRIVATE(buffer))))
    {
        snprintf (string, sizeof (string),
                  _("WeeChat uptime: %d %s %02d:%02d:%02d, started on %s"),
                  day,
                  NG_("day", "days", day),
                  hour,
                  min,
                  sec,
                  ctime (&weechat_start_time));
        string[strlen (string) - 1] = '\0';
        user_command (server, channel, string, 0);
    }
    else
    {
        irc_display_prefix (NULL, buffer, GUI_PREFIX_INFO);
        gui_printf_nolog (buffer,
                          _("WeeChat uptime: %s%d %s%s "
                            "%s%02d%s:%s%02d%s:%s%02d%s, "
                            "started on %s%s"),
                          GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                          day,
                          GUI_COLOR(GUI_COLOR_WIN_CHAT),
                          NG_("day", "days", day),
                          GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                          hour,
                          GUI_COLOR(GUI_COLOR_WIN_CHAT),
                          GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                          min,
                          GUI_COLOR(GUI_COLOR_WIN_CHAT),
                          GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                          sec,
                          GUI_COLOR(GUI_COLOR_WIN_CHAT),
                          GUI_COLOR(GUI_COLOR_WIN_CHAT_CHANNEL),
                          ctime (&weechat_start_time));
    }
    
    return 0;
}

/*
 * weechat_cmd_window: manage windows
 */

int
weechat_cmd_window (t_irc_server *server, t_irc_channel *channel,
                    int argc, char **argv)
{
    t_gui_window *window, *ptr_win;
    t_gui_buffer *buffer;
    int i;
    char *error;
    long number;
    
    gui_buffer_find_context (server, channel, &window, &buffer);
    
    if ((argc == 0) || ((argc == 1) && (ascii_strcasecmp (argv[0], "list") == 0)))
    {
        /* list open windows */
        
        gui_printf (NULL, "\n");
        gui_printf (NULL, _("Open windows:\n"));
        
        i = 1;
        for (ptr_win = gui_windows; ptr_win; ptr_win = ptr_win->next_window)
        {
            gui_printf (NULL, "%s[%s%d%s] (%s%d:%d%s;%s%dx%d%s) ",
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                        GUI_COLOR(GUI_COLOR_WIN_CHAT),
                        i,
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                        GUI_COLOR(GUI_COLOR_WIN_CHAT),
                        ptr_win->win_x,
                        ptr_win->win_y,
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK),
                        GUI_COLOR(GUI_COLOR_WIN_CHAT),
                        ptr_win->win_width,
                        ptr_win->win_height,
                        GUI_COLOR(GUI_COLOR_WIN_CHAT_DARK));
            
            weechat_cmd_buffer_display_info (ptr_win->buffer);
            
            i++;
        }
    }
    else
    {
        if (ascii_strcasecmp (argv[0], "splith") == 0)
        {
            /* split window horizontally */
            if (argc > 1)
            {
                error = NULL;
                number = strtol (argv[1], &error, 10);
                if ((error) && (error[0] == '\0')
                    && (number > 0) && (number < 100))
                    gui_window_split_horiz (window, number);
            }
            else
                gui_window_split_horiz (window, 50);
        }
        else if (ascii_strcasecmp (argv[0], "splitv") == 0)
        {
            /* split window vertically */
            if (argc > 1)
            {
                error = NULL;
                number = strtol (argv[1], &error, 10);
                if ((error) && (error[0] == '\0')
                    && (number > 0) && (number < 100))
                    gui_window_split_vertic (window, number);
            }
            else
                gui_window_split_vertic (window, 50);
        }
        else if (ascii_strcasecmp (argv[0], "resize") == 0)
        {
            /* resize window */
            if (argc > 1)
            {
                error = NULL;
                number = strtol (argv[1], &error, 10);
                if ((error) && (error[0] == '\0')
                    && (number > 0) && (number < 100))
                    gui_window_resize (window, number);
            }
        }
        else if (ascii_strcasecmp (argv[0], "merge") == 0)
        {
            if (argc >= 2)
            {
                if (ascii_strcasecmp (argv[1], "all") == 0)
                    gui_window_merge_all (window);
                else
                {
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                    gui_printf (NULL,
                                _("%s unknown option for \"%s\" command\n"),
                                WEECHAT_ERROR, "window merge");
                    return -1;
                }
            }
            else
            {
                if (!gui_window_merge (window))
                {
                    irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
                    gui_printf (NULL,
                                _("%s can not merge windows, "
                                  "there's no other window with same size "
                                  "near current one.\n"),
                                WEECHAT_ERROR);
                    return -1;
                }
            }
        }
        else if (ascii_strncasecmp (argv[0], "b", 1) == 0)
        {
            /* jump to window by buffer number */
            error = NULL;
            number = strtol (argv[0] + 1, &error, 10);
            if ((error) && (error[0] == '\0'))
                gui_window_switch_by_buffer (window, number);
        }
        else if (ascii_strcasecmp (argv[0], "-1") == 0)
            gui_window_switch_previous (window);
        else if (ascii_strcasecmp (argv[0], "+1") == 0)
            gui_window_switch_next (window);
        else if (ascii_strcasecmp (argv[0], "up") == 0)
            gui_window_switch_up (window);
        else if (ascii_strcasecmp (argv[0], "down") == 0)
            gui_window_switch_down (window);
        else if (ascii_strcasecmp (argv[0], "left") == 0)
            gui_window_switch_left (window);
        else if (ascii_strcasecmp (argv[0], "right") == 0)
            gui_window_switch_right (window);
        else
        {
            irc_display_prefix (NULL, NULL, GUI_PREFIX_ERROR);
            gui_printf (NULL,
                        _("%s unknown option for \"%s\" command\n"),
                        WEECHAT_ERROR, "window");
            return -1;
        }
    }
    return 0;
}