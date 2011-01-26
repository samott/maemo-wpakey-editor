/******************************************************************************
 * Copyright (C) 2010, Shaun Amott <shaun@inerd.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: wpakey-editor.c,v 1.1.1.1 2010/06/15 18:44:06 samott Exp $
 *****************************************************************************/


/******************************************************************************
 * Includes
 *****************************************************************************/

#include <hildon-cp-plugin/hildon-cp-plugin-interface.h>
#include <hildon/hildon.h>
#include <gtk/gtk.h>
#include <gconf/gconf.h>

#include <string.h>
#include <stdlib.h>


/******************************************************************************
 * Definitions
 *****************************************************************************/

#define IAP_PATH "/system/osso/connectivity/IAP"
#define HEX_CHARS "0123456789ABCDEFabcdef"


/******************************************************************************
 * Globals
 *****************************************************************************/

GConfEngine *gconf;


/******************************************************************************
 * Prototypes
 *****************************************************************************/

gboolean net_selector(osso_context_t *, gpointer, char **, char **);
gboolean key_editor(osso_context_t *, gpointer, const char *, const char *);
void get_wpa_key(const char *, GString **);
gboolean set_wpa_key(const char *, const char *);
void showmsg(gpointer, const char *, GtkMessageType);
void showtooltip(gpointer, const char *);


/******************************************************************************
 * Func: execute()
 *
 * Desc: Plug-in entry point.
 *****************************************************************************/

osso_return_t execute(osso_context_t *osso, gpointer data, gboolean user_activated)
{
	char *guid, *name;

	gconf = gconf_engine_get_default();

	while (net_selector(osso, data, &guid, &name) == TRUE) {
		key_editor(osso, data, guid, name);
	}

	gconf_engine_unref(gconf);

	return OSSO_OK;
}


/******************************************************************************
 * Func: save_state()
 *
 * Desc: Save state.
 *****************************************************************************/

osso_return_t save_state(osso_context_t *osso, gpointer data)
{
	return OSSO_OK;
}


/******************************************************************************
 * Func: net_selector()
 *
 * Desc: Show the "net selection" dialog -- a list of the networks the system
 *       currently knows about.
 *
 * Args: osso - osso context, from execute()
 *       data - Top level GTK widget, from execute()
 *       guid - Returned network GUID (if a network was selected)
 *       name - Returned network name (if a network was selected)
 *
 * Retn: res  - Did user select a network?
 *****************************************************************************/

gboolean net_selector(osso_context_t *osso, gpointer data, char **guid, char **name)
{
	GtkWidget *dialog, *content;
	GtkWidget *netlist;
	GSList *nets;
	GSList *item;
	GSList *guids = NULL;
	GError *err = NULL;
	gint response = GTK_RESPONSE_NONE;

	/* Create dialog with OK and cancel buttons. */

	dialog = gtk_dialog_new_with_buttons(
		"WPA keys",
		GTK_WINDOW(data),
		GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_OK,
		GTK_RESPONSE_OK,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_CANCEL,
		NULL
	);

	content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	/* Create a combo box to contain a list of networks */

	netlist = gtk_combo_box_new_text();

	gtk_container_add(GTK_CONTAINER(content), netlist);

	/* Get a list of network GUIDs */

	nets = gconf_engine_all_dirs(gconf, IAP_PATH, &err);

	if (err) {
		/* Something went wrong */
		showmsg(data, "Error enumerating networks", GTK_MESSAGE_ERROR);
		goto skipdialog;
	}

	if (g_slist_length(nets) == 0) {
		showmsg(data, "No networks configured!", GTK_MESSAGE_INFO);
		goto skipdialog;
	}

	/* Populate the network list */

	for (item = nets; item != NULL; item = g_slist_next(item)) {
		gchar *guid, *name, *type;
		GString *path;

		guid = strrchr(item->data, '/') + 1;

		/* Get name of connection */

		path = g_string_new(IAP_PATH "/");
		g_string_append(path, guid);
		g_string_append(path, "/name");

		name = gconf_engine_get_string(gconf, path->str, &err);

		if (name == NULL || err != NULL) {
			g_string_free(path, TRUE);
			continue;
		}

		/* Connection type - we only want WPA PSK conns */

		g_string_assign(path, IAP_PATH "/");
		g_string_append(path, guid);
		g_string_append(path, "/wlan_security");

		type = gconf_engine_get_string(gconf, path->str, &err);

		g_string_free(path, TRUE);

		if (type == NULL || err != NULL
				|| strcmp(type, "WPA_PSK") != 0) {
			continue;
		}

		gtk_combo_box_append_text(GTK_COMBO_BOX(netlist), name);

		/* For GUID lookups (simpler than GtkComboBox data models) */
		guids = g_slist_append(guids, guid);
	}

	/* Check we found a configurable network */
	if (guids == NULL) {
		showmsg(data, "No WPA-PSK networks found!", GTK_MESSAGE_INFO);
		goto skipdialog;
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(netlist), 0);

	/* Show controls */
	gtk_widget_show_all(dialog);

	/* Present dialog to user */
	response = gtk_dialog_run(GTK_DIALOG(dialog));

	if (response == GTK_RESPONSE_OK) {
		/* Find the GUID using our lookup table. */
		gint selected = gtk_combo_box_get_active(GTK_COMBO_BOX(netlist));

		if (selected == -1)
			return FALSE;

		*guid = g_slist_nth_data(guids, selected);
		*name = gtk_combo_box_get_active_text(GTK_COMBO_BOX(netlist));
	}

skipdialog:
	g_slist_free(nets);
	g_slist_free(guids);

	gtk_widget_destroy(GTK_WIDGET(dialog));

	return (response == GTK_RESPONSE_OK);
}


/******************************************************************************
 * Func: key_editor()
 *
 * Desc: Show the "key editor" dialog -- a box for entering the WPA key for the
 *       currently selected network.
 *
 * Args: osso - osso context, from execute()
 *       data - Top level GTK widget, from execute()
 *
 *       guid - GUID of network to edit.
 *       name - Name of network - saves another lookup.
 *
 * Retn: res  - Was a new key saved successfully?
 *****************************************************************************/

gboolean key_editor(osso_context_t *osso, gpointer data, const char *guid, const char *name)
{
	GtkWidget *dialog, *content;
	GtkWidget *keybox;
	GString *key;
	gint response;

	GString *title = g_string_new("WPA Key for ");
	g_string_append(title, name);

	/* Create dialog with OK and cancel buttons. */

	dialog = gtk_dialog_new_with_buttons(
		title->str,
		GTK_WINDOW(data),
		GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_OK,
		GTK_RESPONSE_OK,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_CANCEL,
		NULL
	);

	g_string_free(title, TRUE);

	content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	/* Key entry field */

	keybox = gtk_entry_new();

	gtk_container_add(GTK_CONTAINER(content), keybox);

	/* Get current key value */

	key = g_string_new("");

	get_wpa_key(guid, &key);
	gtk_entry_set_text(GTK_ENTRY(keybox), key->str);

	g_string_free(key, TRUE);

	/* Show controls */

	gtk_widget_show_all(dialog);

showdialog:
	/* Present dialog to user */

	response = gtk_dialog_run(GTK_DIALOG(dialog));

	if (response == GTK_RESPONSE_OK)
	{
		if (set_wpa_key(guid, gtk_entry_get_text(GTK_ENTRY(keybox)))) {
			showtooltip(data, "WPA key saved!");
		} else {
			showmsg(data, "Invalid key! Not saved.", GTK_MESSAGE_ERROR);
			goto showdialog;
		}
	}

	gtk_widget_destroy(GTK_WIDGET(keybox));
	gtk_widget_destroy(GTK_WIDGET(dialog));

	return (response == GTK_RESPONSE_OK);
}


/******************************************************************************
 * Func: get_wpa_key()
 *
 * Desc: Get the current WPA key (passphrase or key text) for the given GUID.
 *
 * Args: guid - Network GUID to get key for.
 *       key  - Where to put key.
 *
 * Retn: n/a
 *****************************************************************************/

void get_wpa_key(const char *guid, GString **key)
{
	GString *path;
	GSList *bytes = NULL;
	GError *err = NULL;
	GSList *item;
	gchar *val;

	/* Look for passphrase first. */

	path = g_string_new(IAP_PATH "/");
	g_string_append(path, guid);
	g_string_append(path, "/EAP_wpa_preshared_passphrase");

	val = gconf_engine_get_string(gconf, path->str, &err);

	if (val != NULL && err == NULL) {
		/* Passphrase found: return it */
		g_string_free(path, TRUE);
		g_string_assign(*key, val);
		return;
	}

	/* Now check for a raw key */

	g_string_assign(path, IAP_PATH "/");
	g_string_append(path, guid);
	g_string_append(path, "/EAP_wpa_preshared_key");

	bytes = gconf_engine_get_list(gconf, path->str, GCONF_VALUE_INT, &err);

	g_string_free(path, TRUE);

	if (bytes == NULL || err != NULL) {
		/* Error or no key set */
		g_string_assign(*key, "");
		return;
	}

	/* Build a string from the bytes */

	g_string_assign(*key, "");

	for (item = bytes; item != NULL; item = g_slist_next(item)) {
		char hexbyte[2];
		sprintf(hexbyte, "%02x", GPOINTER_TO_UINT(item->data));
		g_string_append(*key, hexbyte);
	}

	return;
}


/******************************************************************************
 * Func: set_wpa_key()
 *
 * Desc: Set the WPA key (passphrase or key text) for the given GUID.
 *
 * Args: guid - Network GUID to get key for.
 *       key  - Key string.
 *
 * Retn: succ - Key was valid and set successfully.
 *****************************************************************************/

gboolean set_wpa_key(const char *guid, const char *key)
{
	GString *path, *keystr;

	keystr = g_string_new(key);

	/* Verify key length */

	if (keystr->len < 8 || keystr->len > 64) {
		g_string_free(keystr, TRUE);
		return FALSE;
	}

	/* Possible hex key... */

	if (keystr->len == 64) {
		GSList *bytes = NULL;
		int i = 0;

		if (strspn(keystr->str, HEX_CHARS) != 64) {
			/* Invalid chars: reject */
			g_string_free(keystr, TRUE);
			return FALSE;
		}

		/* Unset the passphrase */

		path = g_string_new(IAP_PATH "/");
		g_string_append(path, guid);
		g_string_append(path, "/EAP_wpa_preshared_passphrase");

		gconf_engine_unset(gconf, path->str, NULL);

		/* Split key into bytes */

		for (i = 0; i < keystr->len; i += 2) {
			char bstr[3];
			bstr[0] = keystr->str[i];
			bstr[1] = keystr->str[i+1];
			bstr[2] = '\0';
			bytes = g_slist_prepend(bytes, GINT_TO_POINTER(strtol(bstr, NULL, 16)));
		}

		/* More efficient to prepend & reverse */
		bytes = g_slist_reverse(bytes);

		g_string_assign(path, IAP_PATH "/");
		g_string_append(path, guid);
		g_string_append(path, "/EAP_wpa_preshared_key");

		gconf_engine_set_list(gconf, path->str, GCONF_VALUE_INT, bytes, NULL);

		g_slist_free(bytes);
		g_string_free(path, TRUE);
		g_string_free(keystr, TRUE);

		return TRUE;
	}

	/* Not a hex key, so set as passphrase */

	/* Clear key */

	path = g_string_new(IAP_PATH "/");
	g_string_append(path, guid);
	g_string_append(path, "/EAP_wpa_preshared_key");

	gconf_engine_unset(gconf, path->str, NULL);

	/* Now set passphrase */

	g_string_assign(path, IAP_PATH "/");
	g_string_append(path, guid);
	g_string_append(path, "/EAP_wpa_preshared_passphrase");

	gconf_engine_set_string(gconf, path->str, keystr->str, NULL);

	/* Done */

	g_string_free(path, TRUE);
	g_string_free(keystr, TRUE);

	return TRUE;
}


/******************************************************************************
 * Func: showmsg()
 *
 * Desc: Display a dialog box containing an error message.
 *
 * Args: data - Top level GTK widget, from execute()
 *       msg  - Message to display.
 *       type - Type of message.
 *
 * Retn: n/a
 *****************************************************************************/

void showmsg(gpointer data, const char *msg, GtkMessageType type)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
		data,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		type,
		GTK_BUTTONS_CLOSE,
		msg
	);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


/******************************************************************************
 * Func: showtooltip()
 *
 * Desc: Display a yellow tooltip notification.
 *
 * Args: data - Top level GTK widget, from execute()
 *       msg  - Message to display.
 *
 * Retn: n/a
 *****************************************************************************/

void showtooltip(gpointer data, const char *msg)
{
	hildon_banner_show_information_override_dnd(data, msg);
}
