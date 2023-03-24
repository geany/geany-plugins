/* Included directly into dhp-object.c just to keep this whole mess out
 * of that file until it's not done so stupidly. */

void devhelp_plugin_load_settings(DevhelpPlugin *self, const gchar *filename)
{
	GError *error;
	GKeyFile *kf;
	gboolean value;

	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	kf = g_key_file_new();
	self->priv->kf = kf;

	if (!g_key_file_load_from_file(kf, filename, G_KEY_FILE_KEEP_COMMENTS, NULL))
		return;

	if (g_key_file_has_group(kf, "webview"))
	{
		gchar *last_uri = NULL;
		if (g_key_file_has_key(kf, "webview", "location", NULL))
		{
			gint location = g_key_file_get_integer(kf, "webview", "location", NULL);
			switch(location)
			{
				case 0:
					devhelp_plugin_set_webview_location(self,
						DEVHELP_PLUGIN_WEBVIEW_LOCATION_NONE);
					break;
				case 1:
					devhelp_plugin_set_webview_location(self,
						DEVHELP_PLUGIN_WEBVIEW_LOCATION_SIDEBAR);
					break;
				case 2:
					devhelp_plugin_set_webview_location(self,
						DEVHELP_PLUGIN_WEBVIEW_LOCATION_MESSAGE_WINDOW);
					break;
				case 3:
					devhelp_plugin_set_webview_location(self,
						DEVHELP_PLUGIN_WEBVIEW_LOCATION_MAIN_NOTEBOOK);
					break;
				default:
					g_warning("Unknown webview location %d", location);
					break;
			}
		}

		if (g_key_file_has_key(kf, "webview", "focus_webview_on_search", NULL))
		{
			self->priv->focus_webview_on_search = \
				g_key_file_get_boolean(kf, "webview", "focus_webview_on_search", NULL);
		}

		if (g_key_file_has_key(kf, "webview", "last_uri", NULL))
		{
			last_uri = g_key_file_get_string(kf, "webview", "last_uri", NULL);
			if (last_uri != NULL && strlen(last_uri) == 0)
			{
				g_free(last_uri);
				last_uri = NULL;
			}
		}

		if (g_key_file_has_key(kf, "webview", "custom_homepage", NULL))
		{
			gchar *custom_homepage;

			custom_homepage = g_key_file_get_string(kf, "webview", "custom_homepage", NULL);
			if (custom_homepage != NULL && strlen(custom_homepage) == 0)
			{
				g_free(custom_homepage);
				custom_homepage = NULL;
			}
			else if (custom_homepage != NULL)
			{
				g_free(last_uri); /* always NULL or valid string */
				last_uri = g_strdup(custom_homepage);
			}
			self->priv->custom_homepage = custom_homepage;
		}

		if (last_uri != NULL)
		{
			devhelp_plugin_set_webview_uri(self, last_uri);
			g_free(last_uri);
		}

	} /* webview group */

	if (g_key_file_has_group(kf, "doc_providers"))
	{

		if (g_key_file_has_key(kf, "doc_providers", "devhelp", NULL))
		{
			error = NULL;
			value = g_key_file_get_boolean(kf, "doc_providers", "devhelp", &error);
			if (error != NULL)
				g_error_free(error);
			else
				devhelp_plugin_set_use_devhelp(self, value);
		}

		if (g_key_file_has_key(kf, "doc_providers", "man_pages", NULL))
		{
			error = NULL;
			value = g_key_file_get_boolean(kf, "doc_providers", "man_pages", &error);
			if (error != NULL)
				g_error_free(error);
			else
				devhelp_plugin_set_use_man(self, value);
		}

		if (g_key_file_has_key(kf, "doc_providers", "codesearch", NULL))
		{
			error = NULL;
			value = g_key_file_get_boolean(kf, "doc_providers", "codesearch", &error);
			if (error != NULL)
				g_error_free(error);
			else
				devhelp_plugin_set_use_codesearch(self, value);
		}

	} /* doc_providers group */

	if (g_key_file_has_group(kf, "devhelp"))
	{
		if (g_key_file_has_key(kf, "devhelp", "show_devhelp_sidebar", NULL))
		{
			error = NULL;
			value = g_key_file_get_boolean(kf, "devhelp", "show_devhelp_sidebar", &error);
			if (error != NULL)
				g_error_free(error);
			else
				devhelp_plugin_set_devhelp_sidebar_visible(self, value);
		}

		if (g_key_file_has_key(kf, "devhelp", "set_sidebar_tabs_bottom", NULL))
		{
			error = NULL;
			value = g_key_file_get_boolean(kf, "devhelp", "set_sidebar_tabs_bottom", &error);
			if (error != NULL)
				g_error_free(error);
			else
				devhelp_plugin_set_sidebar_tabs_bottom(self, value);
		}

		if (g_key_file_has_key(kf, "devhelp", "focus_sidebar_on_search", NULL))
		{
			error = NULL;
			value = g_key_file_get_boolean(kf, "devhelp", "focus_sidebar_on_search", &error);
			if (error != NULL)
				g_error_free(error);
			else
				self->priv->focus_sidebar_on_search = value;
		}

	} /* devhelp group */

	if (g_key_file_has_group(kf, "man_pages"))
	{

		if (g_key_file_has_key(kf, "man_pages", "prog_path", NULL))
		{
			gchar *man_path;
			error = NULL;
			man_path = g_key_file_get_string(kf, "man_pages", "prog_path", &error);
			if (error != NULL)
				g_error_free(error);
			else if (strlen(man_path) == 0)
				g_free(man_path);
			else
			{
				g_free(self->priv->man_prog_path);
				self->priv->man_prog_path = man_path;
			}
		}

		if (g_key_file_has_key(kf, "man_pages", "pager_prog", NULL))
		{
			gchar *pager;
			error = NULL;
			pager = g_key_file_get_string(kf, "man_pages", "pager_prog", &error);
			if (error != NULL)
				g_error_free(error);
			else if (strlen(pager) == 0)
				g_free(pager);
			else
			{
				g_free(self->priv->man_pager_prog);
				self->priv->man_pager_prog = pager;
			}
		}

		if (g_key_file_has_key(kf, "man_pages", "section_order", NULL))
		{
			gchar *sect;
			error = NULL;
			sect = g_key_file_get_string(kf, "man_pages", "section_order", &error);
			if (error != NULL)
				g_error_free(error);
			else if (strlen(sect) == 0)
				g_free(sect);
			else
			{
				g_free(self->priv->man_section_order);
				self->priv->man_section_order = sect;
			}
		}

	} /* man_pages group */

	if (g_key_file_has_group(kf, "codesearch"))
	{
		gchar *uri;

		if (g_key_file_has_key(kf, "codesearch", "base_uri", NULL))
		{
			error = NULL;
			uri = g_key_file_get_string(kf, "codesearch", "base_uri", &error);
			if (error != NULL)
				g_error_free(error);
			else if (strlen(uri) == 0)
				g_free(uri);
			else
			{
				g_free(self->priv->codesearch_base_uri);
				self->priv->codesearch_base_uri = uri;
			}
		}

		if (g_key_file_has_key(kf, "codesearch", "uri_params", NULL))
		{
			error = NULL;
			uri = g_key_file_get_string(kf, "codesearch", "uri_params", &error);
			if (error != NULL)
				g_error_free(error);
			else if (strlen(uri) == 0)
				g_free(uri);
			else
			{
				g_free(self->priv->codesearch_params);
				self->priv->codesearch_params = uri;
			}
		}

		if (g_key_file_has_key(kf, "codesearch", "use_lang_for_search", NULL))
		{
			error = NULL;
			value = g_key_file_get_boolean(kf, "codesearch", "use_lang_for_search", &error);
			if (error != NULL)
				g_error_free(error);
			else
				self->priv->codesearch_use_lang = value;
		}

	} /* codesearch group */

	if (g_key_file_has_group(kf, "misc"))
	{
		gint pos;
		error = NULL;
		pos = g_key_file_get_integer(kf, "misc", "main_notebook_tab_pos", &error);
		if (error != NULL)
			g_error_free(error);
		else
		{
			switch (pos)
			{
				case 0:
					self->priv->main_nb_tab_pos = GTK_POS_LEFT;
					break;
				case 1:
					self->priv->main_nb_tab_pos = GTK_POS_RIGHT;
					break;
				case 2:
					self->priv->main_nb_tab_pos = GTK_POS_TOP;
					break;
				case 3:
					self->priv->main_nb_tab_pos = GTK_POS_BOTTOM;
					break;
			}
			if (self->priv->location == DEVHELP_PLUGIN_WEBVIEW_LOCATION_MAIN_NOTEBOOK)
			{
				gtk_notebook_set_tab_pos(GTK_NOTEBOOK(self->priv->main_notebook),
					self->priv->main_nb_tab_pos);
			}
		}
	} /* misc group */
}


void devhelp_plugin_store_settings(DevhelpPlugin *self, const gchar *filename)
{
	gchar *text;
	GKeyFile *kf;

	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	if (self->priv->kf == NULL)
		self->priv->kf = g_key_file_new();
	kf = self->priv->kf;

    g_key_file_set_integer(kf, "webview", "location", (gint)self->priv->location);
    g_key_file_set_boolean(kf, "webview", "focus_webview_on_search", self->priv->focus_webview_on_search);
    g_key_file_set_string(kf, "webview", "custom_homepage", self->priv->custom_homepage != NULL ? self->priv->custom_homepage : "");
    g_key_file_set_string(kf, "webview", "last_uri", devhelp_plugin_get_webview_uri(self));

    g_key_file_set_boolean(kf, "doc_providers", "devhelp", self->priv->use_devhelp);
    g_key_file_set_boolean(kf, "doc_providers", "man_pages", self->priv->use_man);
    g_key_file_set_boolean(kf, "doc_providers", "codesearch", self->priv->use_codesearch);

    g_key_file_set_boolean(kf, "devhelp", "show_devhelp_sidebar",
#if GTK_CHECK_VERSION(2,18,0)
      gtk_widget_get_visible(self->priv->sb_notebook)
#else
      GTK_WIDGET_VISIBLE(self->priv->sb_notebook)
#endif
    );
    g_key_file_set_boolean(kf, "devhelp", "set_sidebar_tabs_bottom", devhelp_plugin_get_sidebar_tabs_bottom(self));
    g_key_file_set_boolean(kf, "devhelp", "focus_sidebar_on_search", self->priv->focus_sidebar_on_search);

    g_key_file_set_string(kf, "man_pages", "prog_path", self->priv->man_prog_path);
    g_key_file_set_string(kf, "man_pages", "page_prog", self->priv->man_pager_prog);
    g_key_file_set_string(kf, "man_pages", "section_order", self->priv->man_section_order);

    g_key_file_set_string(kf, "codesearch", "base_uri", self->priv->codesearch_base_uri);
    g_key_file_set_string(kf, "codesearch", "uri_params", self->priv->codesearch_params != NULL ? self->priv->codesearch_params : "");
    g_key_file_set_boolean(kf, "codesearch", "use_lang_for_search", self->priv->codesearch_use_lang);

    switch(self->priv->main_nb_tab_pos)
    {
		case GTK_POS_LEFT:
			g_key_file_set_integer(kf, "misc", "main_notebook_tab_pos", 0);
			break;
		case GTK_POS_RIGHT:
			g_key_file_set_integer(kf, "misc", "main_notebook_tab_pos", 1);
			break;
		case GTK_POS_TOP:
			g_key_file_set_integer(kf, "misc", "main_notebook_tab_pos", 2);
			break;
		case GTK_POS_BOTTOM:
			g_key_file_set_integer(kf, "misc", "main_notebook_tab_pos", 3);
			break;
	}

	text = g_key_file_to_data(kf, NULL, NULL);
	g_file_set_contents(filename, text, -1, NULL);
	g_free(text);
}
