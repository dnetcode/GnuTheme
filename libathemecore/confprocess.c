/*
 * gnutheme-services: A collection of minimalist IRC services
 * confprocess.c: Generic configuration processing.
 *
 * Copyright (c) 2014-2015 GnuTheme Development Group (http://www.GnuTheme.org)
 * Copyright (c) 2005-2008 Atheme Project (http://www.atheme.org)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "atheme.h"
#include <limits.h>

enum conftype
{
	CONF_HANDLER,
	CONF_UINT,
	CONF_DURATION,
	CONF_DUPSTR,
	CONF_BOOL,
	CONF_SUBBLOCK
};

struct ConfTable
{
	char *name;
	enum conftype type;
	unsigned int flags;
	union
	{
		int (*handler) (mowgli_config_file_entry_t *);

		struct
		{
			unsigned int *var;
			unsigned int min;
			unsigned int max;
			unsigned int def;
		} uint_val;
		struct
		{
			unsigned int *var;
			const char *defunit;
			unsigned int def;
		} duration_val;
		struct
		{
			char **var;
			char *def;
		} dupstr_val;
		struct
		{
			bool *var;
			bool def;
		} bool_val;

		mowgli_list_t *subblock;
	} un;
	mowgli_node_t node;
};

mowgli_heap_t *conftable_heap;

mowgli_list_t confblocks;
bool conf_need_rehash;

/* *INDENT-ON* */

void conf_report_warning(mowgli_config_file_entry_t *ce, const char *fmt, ...)
{
	va_list va;
	char buf[BUFSIZE];
	char name[80];

	return_if_fail(ce != NULL);
	return_if_fail(fmt != NULL);

	va_start(va, fmt);
	vsnprintf(buf, BUFSIZE, fmt, va);
	va_end(va);

	if (ce->prevlevel == NULL)
		mowgli_strlcpy(name, ce->varname, sizeof name);
	else if (ce->prevlevel->prevlevel == NULL)
		snprintf(name, sizeof name, "%s::%s",
				ce->prevlevel->varname, ce->varname);
	else if (ce->prevlevel->prevlevel->prevlevel == NULL)
		snprintf(name, sizeof name, "%s::%s::%s",
				ce->prevlevel->prevlevel->varname,
				ce->prevlevel->varname, ce->varname);
	else
		snprintf(name, sizeof name, "...::%s::%s::%s",
				ce->prevlevel->prevlevel->varname,
				ce->prevlevel->varname, ce->varname);
	slog(LG_ERROR, "%s:%d: [%s] warning: %s", ce->fileptr->filename, ce->varlinenum, name, buf);
}

bool process_uint_configentry(mowgli_config_file_entry_t *ce, unsigned int *var,
		unsigned int min, unsigned int max)
{
	unsigned long v;
	char *end;

	if (ce->vardata == NULL)
	{
		conf_report_warning(ce, "no parameter for configuration option");
		return false;
	}
	errno = 0;
	v = strtoul(ce->vardata, &end, 10);
	if (errno != 0 || *end != '\0' || end == ce->vardata)
	{
		conf_report_warning(ce, "invalid number \"%s\"",
				ce->vardata);
		return false;
	}
	if (v > max || v < min)
	{
		conf_report_warning(ce, "value %lu is out of range [%u,%u]",
				v,
				min,
				max);
		return false;
	}
	*var = v;
	return true;
}

static struct
{
	const char *name;
	unsigned int value;
} duration_units[] =
{
	{ "s", 1 }, /* must be first */
	{ "m", 60 },
	{ "h", 60 * 60 },
	{ "d", 24 * 60 * 60 },
	{ "w", 7 * 24 * 60 * 60 },
	{ NULL, 0 }
};

bool process_duration_configentry(mowgli_config_file_entry_t *ce, unsigned int *var,
		const char *defunit)
{
	unsigned long v;
	unsigned int i;
	unsigned int max;
	const char *unit;
	char *end;

	return_val_if_fail(ce != NULL, false);
	return_val_if_fail(var != NULL, false);
	return_val_if_fail(defunit != NULL, false);

	if (ce->vardata == NULL)
	{
		conf_report_warning(ce, "no parameter for configuration option");
		return false;
	}
	errno = 0;
	v = strtoul(ce->vardata, &end, 10);
	if (errno != 0 || end == ce->vardata)
	{
		conf_report_warning(ce, "invalid number \"%s\"",
				ce->vardata);
		return false;
	}
	while (*end == ' ' || *end == '\t')
		end++;
	unit = *end != '\0' ? end : defunit;
	if (unit == NULL || *unit == '\0')
		i = 0;
	else
	{
		for (i = 0; duration_units[i].name != NULL; i++)
			if (!strcasecmp(duration_units[i].name, unit))
				break;
		if (duration_units[i].name == NULL)
		{
			conf_report_warning(ce, "invalid unit \"%s\"", unit);
			return false;
		}
	}
	max = UINT_MAX / duration_units[i].value;
	if (v > max)
	{
		conf_report_warning(ce, "value %lu%s is out of range [%u%s,%u%s]",
				v,
				duration_units[i].name,
				0,
				duration_units[i].name,
				max,
				duration_units[i].name);
		return false;
	}
	*var = v * duration_units[i].value;
	return true;
}

static void set_default(struct ConfTable *ct)
{
	return_if_fail(ct != NULL);

	if (ct->flags & CONF_NO_REHASH && runflags & RF_REHASHING)
		return;

	switch (ct->type)
	{
		case CONF_UINT:
			*ct->un.uint_val.var = ct->un.uint_val.def;
			break;
		case CONF_DURATION:
			*ct->un.duration_val.var = ct->un.duration_val.def;
			break;
		case CONF_DUPSTR:
			if (*ct->un.dupstr_val.var)
				free(*ct->un.dupstr_val.var);
			*ct->un.dupstr_val.var = ct->un.dupstr_val.def ? sstrdup(ct->un.dupstr_val.def) : NULL;
			break;
		case CONF_BOOL:
			*ct->un.bool_val.var = ct->un.bool_val.def;
			break;
		case CONF_HANDLER:
		case CONF_SUBBLOCK:
			break;
	}
}

static void process_configentry(struct ConfTable *ct, mowgli_config_file_entry_t *ce)
{
	return_if_fail(ct != NULL);
	return_if_fail(ce != NULL);

	if (ct->flags & CONF_NO_REHASH && runflags & RF_REHASHING)
		return;

	switch (ct->type)
	{
		case CONF_HANDLER:
			ct->un.handler(ce);
			break;
		case CONF_UINT:
			if (!process_uint_configentry(ce, ct->un.uint_val.var,
					ct->un.uint_val.min,
					ct->un.uint_val.max))
				return;
			break;
		case CONF_DURATION:
			if (!process_duration_configentry(ce,
						ct->un.duration_val.var,
						ct->un.duration_val.defunit))
				return;
			break;
		case CONF_DUPSTR:
			if (ce->vardata == NULL)
				conf_report_warning(ce, "no parameter for configuration option");
			else
			{
				if (*ct->un.dupstr_val.var)
					free(*ct->un.dupstr_val.var);
				*ct->un.dupstr_val.var = sstrdup(ce->vardata);
			}
			break;
		case CONF_BOOL:
			if (ce->vardata == NULL ||
					!strcasecmp(ce->vardata, "yes") ||
					!strcasecmp(ce->vardata, "on") ||
					!strcasecmp(ce->vardata, "true") ||
					!strcmp(ce->vardata, "1"))
				*ct->un.bool_val.var = true;
			else if (!strcasecmp(ce->vardata, "no") ||
					!strcasecmp(ce->vardata, "off") ||
					!strcasecmp(ce->vardata, "false") ||
					!strcmp(ce->vardata, "0"))
				*ct->un.bool_val.var = false;
			else
				conf_report_warning(ce, "invalid boolean \"%s\"", ce->vardata);
			break;
		case CONF_SUBBLOCK:
			subblock_handler(ce, ct->un.subblock);
			break;
	}
}

void conf_process(mowgli_config_file_t *cfp)
{
	mowgli_config_file_t *cfptr;
	mowgli_config_file_entry_t *ce;
	mowgli_node_t *tn;
	struct ConfTable *ct = NULL;
	struct ConfTable *loadmodule = NULL;

	return_if_fail(cfp != NULL);

	MOWGLI_ITER_FOREACH(tn, confblocks.head)
	{
		ct = tn->data;

		set_default(ct);

		if (!strcasecmp(ct->name, "LOADMODULE"))
			loadmodule = ct;
	}


	/* LOADMODULEs may change confblocks, so we must
	 * load modules before doing anything else */
	MOWGLI_ITER_FOREACH(cfptr, cfp)
	{
		MOWGLI_ITER_FOREACH(ce, cfptr->entries)
		{
			if (!strcasecmp(ce->varname, "LOADMODULE"))
			{
				process_configentry(loadmodule, ce);
			}
		}
	}

	MOWGLI_ITER_FOREACH(cfptr, cfp)
	{
		MOWGLI_ITER_FOREACH(ce, cfptr->entries)
		{
			MOWGLI_ITER_FOREACH(tn, confblocks.head)
			{
				ct = tn->data;

				if ((!strcasecmp(ct->name, ce->varname)) && (strcasecmp(ce->varname, "LOADMODULE")))
				{
					process_configentry(ct, ce);
					break;
				}
			}

			if (ct == NULL)
				conf_report_warning(ce, "invalid configuration option");
		}
	}

	conf_need_rehash = false;
}

int subblock_handler(mowgli_config_file_entry_t *ce, mowgli_list_t *entries)
{
	mowgli_node_t *tn;
	struct ConfTable *ct = NULL;

	return_val_if_fail(ce != NULL, 0);
	return_val_if_fail(entries != NULL, 0);

	MOWGLI_ITER_FOREACH(tn, entries->head)
	{
		ct = tn->data;

		set_default(ct);
	}

	MOWGLI_ITER_FOREACH(ce, ce->entries)
	{
		MOWGLI_ITER_FOREACH(tn, entries->head)
		{
			ct = tn->data;

			if (!strcasecmp(ct->name, ce->varname))
			{
				process_configentry(ct, ce);
				break;
			}
		}

		if (ct == NULL)
			conf_report_warning(ce, "invalid configuration option");
	}
	return 0;
}

struct ConfTable *find_top_conf(const char *name)
{
	mowgli_node_t *n;
	struct ConfTable *ct;

	return_val_if_fail(name != NULL, NULL);

	MOWGLI_ITER_FOREACH(n, confblocks.head)
	{
		ct = n->data;

		if (!strcasecmp(ct->name, name))
			return ct;
	}

	return NULL;
}

struct ConfTable *find_conf_item(const char *name, mowgli_list_t *conflist)
{
	mowgli_node_t *n;
	struct ConfTable *ct;

	return_val_if_fail(name != NULL, NULL);
	return_val_if_fail(conflist != NULL, NULL);

	MOWGLI_ITER_FOREACH(n, conflist->head)
	{
		ct = n->data;

		if (!strcasecmp(ct->name, name))
			return ct;
	}

	return NULL;
}

void add_top_conf(const char *name, int (*handler) (mowgli_config_file_entry_t *ce))
{
	struct ConfTable *ct;

	return_if_fail(name != NULL);
	return_if_fail(handler != NULL);

	if (find_top_conf(name))
	{
		slog(LG_DEBUG, "add_top_conf(): duplicate config block '%s'.", name);
		return;
	}

	ct = mowgli_heap_alloc(conftable_heap);

	ct->name = sstrdup(name);
	ct->type = CONF_HANDLER;
	ct->flags = 0;
	ct->un.handler = handler;

	mowgli_node_add(ct, &ct->node, &confblocks);
	conf_need_rehash = true;
}

void add_subblock_top_conf(const char *name, mowgli_list_t *list)
{
	struct ConfTable *ct;

	return_if_fail(name != NULL);
	return_if_fail(list != NULL);

	if (find_top_conf(name))
	{
		slog(LG_DEBUG, "add_top_conf(): duplicate config block '%s'.", name);
		return;
	}

	ct = mowgli_heap_alloc(conftable_heap);

	ct->name = sstrdup(name);
	ct->type = CONF_SUBBLOCK;
	ct->flags = 0;
	ct->un.subblock = list;

	mowgli_node_add(ct, &ct->node, &confblocks);
	conf_need_rehash = true;
}

void add_conf_item(const char *name, mowgli_list_t *conflist, int (*handler) (mowgli_config_file_entry_t *ce))
{
	struct ConfTable *ct;

	return_if_fail(name != NULL);
	return_if_fail(conflist != NULL);
	return_if_fail(handler != NULL);

	if (find_conf_item(name, conflist))
	{
		slog(LG_DEBUG, "add_conf_item(): duplicate item %s", name);
		return;
	}

	ct = mowgli_heap_alloc(conftable_heap);

	ct->name = sstrdup(name);
	ct->type = CONF_HANDLER;
	ct->flags = 0;
	ct->un.handler = handler;

	mowgli_node_add(ct, &ct->node, conflist);
	conf_need_rehash = true;
}

void add_uint_conf_item(const char *name, mowgli_list_t *conflist, unsigned int flags, unsigned int *var, unsigned int min, unsigned int max, unsigned int def)
{
	struct ConfTable *ct;

	if (find_conf_item(name, conflist))
	{
		slog(LG_DEBUG, "add_uint_conf_item(): duplicate item %s", name);
		return;
	}

	ct = mowgli_heap_alloc(conftable_heap);

	ct->name = sstrdup(name);
	ct->type = CONF_UINT;
	ct->flags = flags;
	ct->un.uint_val.var = var;
	ct->un.uint_val.min = min;
	ct->un.uint_val.max = max;
	ct->un.uint_val.def = def;

	mowgli_node_add(ct, &ct->node, conflist);
	conf_need_rehash = true;
}

void add_duration_conf_item(const char *name, mowgli_list_t *conflist, unsigned int flags, unsigned int *var, const char *defunit, unsigned int def)
{
	struct ConfTable *ct;

	if (find_conf_item(name, conflist))
	{
		slog(LG_DEBUG, "add_duration_conf_item(): duplicate item %s", name);
		return;
	}

	ct = mowgli_heap_alloc(conftable_heap);

	ct->name = sstrdup(name);
	ct->type = CONF_DURATION;
	ct->flags = flags;
	ct->un.duration_val.var = var;
	ct->un.duration_val.defunit = defunit;
	ct->un.duration_val.def = def;

	mowgli_node_add(ct, &ct->node, conflist);
	conf_need_rehash = true;
}

void add_dupstr_conf_item(const char *name, mowgli_list_t *conflist, unsigned int flags, char **var, const char *def)
{
	struct ConfTable *ct;

	if (find_conf_item(name, conflist))
	{
		slog(LG_DEBUG, "add_dupstr_conf_item(): duplicate item %s", name);
		return;
	}

	ct = mowgli_heap_alloc(conftable_heap);

	ct->name = sstrdup(name);
	ct->type = CONF_DUPSTR;
	ct->flags = flags;
	ct->un.dupstr_val.var = var;
	ct->un.dupstr_val.def = sstrdup(def);

	mowgli_node_add(ct, &ct->node, conflist);
	conf_need_rehash = true;
}

void add_bool_conf_item(const char *name, mowgli_list_t *conflist, unsigned int flags, bool *var, bool def)
{
	struct ConfTable *ct;

	if (find_conf_item(name, conflist))
	{
		slog(LG_DEBUG, "add_bool_conf_item(): duplicate item %s", name);
		return;
	}

	ct = mowgli_heap_alloc(conftable_heap);

	ct->name = sstrdup(name);
	ct->type = CONF_BOOL;
	ct->flags = flags;
	ct->un.bool_val.var = var;
	ct->un.bool_val.def = def;

	mowgli_node_add(ct, &ct->node, conflist);
	conf_need_rehash = true;
}

void del_top_conf(const char *name)
{
	struct ConfTable *ct;

	return_if_fail(name != NULL);

	if (!(ct = find_top_conf(name)))
	{
		slog(LG_DEBUG, "del_top_conf(): cannot delete nonexistant block %s", name);
		return;
	}

	mowgli_node_delete(&ct->node, &confblocks);

	free(ct->name);

	mowgli_heap_free(conftable_heap, ct);
}

void del_conf_item(const char *name, mowgli_list_t *conflist)
{
	struct ConfTable *ct;

	return_if_fail(name != NULL);
	return_if_fail(conflist != NULL);

	if (!(ct = find_conf_item(name, conflist)))
	{
		slog(LG_DEBUG, "del_conf_item(): cannot delete nonexistant item %s", name);
		return;
	}

	mowgli_node_delete(&ct->node, conflist);

	if (ct->type == CONF_DUPSTR && ct->un.dupstr_val.def)
	{
		free(ct->un.dupstr_val.def);
	}

	free(ct->name);

	mowgli_heap_free(conftable_heap, ct);
}

conf_handler_t conftable_get_conf_handler(struct ConfTable *ct)
{
	return_val_if_fail(ct != NULL, NULL);

	return ct->type == CONF_HANDLER ? ct->un.handler : NULL;
}

/* stolen from Sentinel */
int token_to_value(struct Token token_table[], const char *token)
{
	int i;

	return_val_if_fail(token_table != NULL, TOKEN_ERROR);
	return_val_if_fail(token != NULL, TOKEN_ERROR);

	for (i = 0; token_table[i].text != NULL; i++)
	{
		if (strcasecmp(token_table[i].text, token) == 0)
		{
			return token_table[i].value;
		}
	}

	/* Otherwise... */
	return TOKEN_UNMATCHED;
}

void init_confprocess(void)
{
	conftable_heap = sharedheap_get(sizeof(struct ConfTable));

	if (!conftable_heap)
	{
		slog(LG_ERROR, "init_confprocess(): block allocator failure.");
		exit(EXIT_FAILURE);
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
