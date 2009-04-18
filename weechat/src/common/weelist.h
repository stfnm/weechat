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


#ifndef __WEECHAT_LIST_H
#define __WEECHAT_LIST_H 1

#define WEELIST_POS_SORT      0
#define WEELIST_POS_BEGINNING 1
#define WEELIST_POS_END       2

typedef struct t_weelist t_weelist;

struct t_weelist
{
    char *data;
    t_weelist *prev_weelist;
    t_weelist *next_weelist;
};

extern int weelist_get_size (t_weelist *);
extern t_weelist *weelist_search (t_weelist *, char *);
extern t_weelist *weelist_add (t_weelist **, t_weelist **, char *, int);
extern void weelist_remove (t_weelist **, t_weelist **, t_weelist *);
extern void weelist_remove_all (t_weelist **, t_weelist **);
extern void weelist_print_log (t_weelist *, char *);

#endif /* weelist.h */