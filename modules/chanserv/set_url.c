/*
 * Copyright (c) 2015 GnuTheme Development Group (GnuTheme.org)
 * Copyright (c) 2003-2004 E. Will et al.
 * Copyright (c) 2006-2010 Atheme Development Group
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the CService SET URL command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/set_url", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"GnuTheme Development Group <http://www.GnuTheme.org>"
);

static void cs_cmd_set_url(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_set_url = { "URL", N_("Sets the channel URL."), AC_NONE, 2, cs_cmd_set_url, { .path = "cservice/set_url" } };

mowgli_patricia_t **cs_set_cmdtree;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, cs_set_cmdtree, "chanserv/set_core", "cs_set_cmdtree");

	command_add(&cs_set_url, *cs_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_set_url, *cs_set_cmdtree);
}

static void cs_cmd_set_url(sourceinfo_t *si, int parc, char *parv[])
{
	mychan_t *mc;
	char *url = parv[1];

	if (!(mc = mychan_find(parv[0])))
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), parv[0]);
		return;
	}
	
	if (metadata_find(mc, "private:frozen:freezer"))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is frozen."), parv[0]);
		return;
	}

	if (!chanacs_source_has_flag(mc, si, CA_SET))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to execute this command."));
		return;
	}

	if (!url || !strcasecmp("OFF", url) || !strcasecmp("NONE", url))
	{
		/* not in a namespace to allow more natural use of SET PROPERTY.
		 * they may be able to introduce spaces, though. c'est la vie.
		 */
		if (metadata_find(mc, "url"))
		{
			metadata_delete(mc, "url");
			logcommand(si, CMDLOG_SET, "SET:URL:NONE: \2%s\2", mc->name);
			command_success_nodata(si, _("The URL for \2%s\2 has been cleared."), parv[0]);
			return;
		}

		command_fail(si, fault_nochange, _("The URL for \2%s\2 was not set."), parv[0]);
		return;
	}

	/* we'll overwrite any existing metadata */
	metadata_add(mc, "url", url);

	logcommand(si, CMDLOG_SET, "SET:URL: \2%s\2 \2%s\2", mc->name, url);
	command_success_nodata(si, _("The URL of \2%s\2 has been set to \2%s\2."), parv[0], url);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
