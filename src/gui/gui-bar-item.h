/*
 * Copyright (c) 2003-2009 by FlashCode <flashcode@flashtux.org>
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


#ifndef __WEECHAT_GUI_BAR_ITEM_H
#define __WEECHAT_GUI_BAR_ITEM_H 1

enum t_gui_bar_item_weechat
{
    GUI_BAR_ITEM_INPUT_PASTE = 0,
    GUI_BAR_ITEM_INPUT_PROMPT,
    GUI_BAR_ITEM_INPUT_SEARCH,
    GUI_BAR_ITEM_INPUT_TEXT,
    GUI_BAR_ITEM_TIME,
    GUI_BAR_ITEM_BUFFER_COUNT,
    GUI_BAR_ITEM_BUFFER_PLUGIN,
    GUI_BAR_ITEM_BUFFER_NUMBER,
    GUI_BAR_ITEM_BUFFER_NAME,
    GUI_BAR_ITEM_BUFFER_FILTER,
    GUI_BAR_ITEM_BUFFER_NICKLIST_COUNT,
    GUI_BAR_ITEM_SCROLL,
    GUI_BAR_ITEM_HOTLIST,
    GUI_BAR_ITEM_COMPLETION,
    GUI_BAR_ITEM_BUFFER_TITLE,
    GUI_BAR_ITEM_BUFFER_NICKLIST,
    /* number of bar items */
    GUI_BAR_NUM_ITEMS,
};

struct t_gui_window;

struct t_gui_bar_item
{
    struct t_weechat_plugin *plugin; /* plugin                              */
    char *name;                      /* bar item name                       */
    char *(*build_callback)(void *data,
                            struct t_gui_bar_item *item,
                            struct t_gui_window *window);
                                     /* callback called for building item   */
    void *build_callback_data;       /* data for callback                   */
    struct t_gui_bar_item *prev_item; /* link to previous bar item          */
    struct t_gui_bar_item *next_item; /* link to next bar item              */
};

struct t_gui_bar_item_hook
{
    struct t_hook *hook;                   /* pointer to hook               */
    struct t_gui_bar_item_hook *next_hook; /* next hook                     */
};

/* variables */

extern struct t_gui_bar_item *gui_bar_items;
extern struct t_gui_bar_item *last_gui_bar_item;
extern char *gui_bar_item_names[];

/* functions */

extern int gui_bar_item_valid (struct t_gui_bar_item *bar_item);
extern struct t_gui_bar_item *gui_bar_item_search (const char *name);
extern int gui_bar_item_string_is_item (const char *string,
                                        const char *item_name);
extern int gui_bar_item_used_in_a_bar (const char *item_name,
                                       int partial_name);
extern char *gui_bar_item_get_value (const char *name,
                                     struct t_gui_bar *bar,
                                     struct t_gui_window *window);
extern struct t_gui_bar_item *gui_bar_item_new (struct t_weechat_plugin *plugin,
                                                const char *name,
                                                char *(*build_callback)(void *data,
                                                                        struct t_gui_bar_item *item,
                                                                        struct t_gui_window *window),
                                                void *build_callback_data);
extern void gui_bar_item_update (const char *name);
extern void gui_bar_item_free (struct t_gui_bar_item *item);
extern void gui_bar_item_free_all ();
extern void gui_bar_item_free_all_plugin (struct t_weechat_plugin *plugin);
extern void gui_bar_item_init ();
extern void gui_bar_item_end ();
extern int gui_bar_item_add_to_infolist (struct t_infolist *infolist,
                                         struct t_gui_bar_item *bar_item);
extern void gui_bar_item_print_log ();

#endif /* gui-bar-item.h */