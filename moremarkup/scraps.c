    /*
    next = gtk_button_new_with_label("next");
    gtk_container_add(GTK_CONTAINER(toolbar), next);
    previous = gtk_button_new_with_label("previous"); 
    gtk_container_add(GTK_CONTAINER(toolbar), previous);
    replace_entry = gtk_entry_new_with_max_length(120);
    gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(replace_entry));
    replace = gtk_button_new_with_label("replace"); 
    gtk_container_add(GTK_CONTAINER(toolbar), replace);*/
    
	//g_signal_connect(item, "clicked", G_CALLBACK(on_refresh), NULL);
     /*
    GtkWidget *search_bar, *entry, *box, *menu_button;
    search_bar = gtk_search_bar_new ();
    gtk_search_bar_set_show_close_button (search_bar, TRUE);
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_add (GTK_CONTAINER (search_bar), box);
    gtk_widget_set_style (GTK_WIDGET(search_bar), GTK_TOOLBAR_ICONS);
    entry = gtk_search_entry_new ();
    gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);
    menu_button = gtk_menu_button_new ();
    gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(menu_button), GTK_STOCK_JUMP_TO);
    gtk_box_pack_start (GTK_BOX (box), menu_button, FALSE, FALSE, 0);
    ;*/

    /*
    GtkWidget *vbox = ui_lookup_widget(geany_data->main_widgets->window, "vbox1");
    gtk_box_pack_start(GTK_BOX(vbox), priv->search_dock_bar, FALSE, FALSE, 1);
    gtk_box_reorder_child(GTK_BOX(vbox), priv->search_dock_bar, 2);
    gtk_widget_show(priv->search_dock_bar);*/

    GtkWidget *selection_indicator_plain = gtk_menu_item_new_with_label ("Plain");
    GtkWidget *selection_indicator_squiggle = gtk_menu_item_new_with_label ("Squiggle");
    GtkWidget *selection_indicator_tt = gtk_menu_item_new_with_label ("TT");
    GtkWidget *selection_indicator_diagonal = gtk_menu_item_new_with_label ("Diagonal");
    GtkWidget *selection_indicator_strike = gtk_menu_item_new_with_label ("Strike");
    GtkWidget *selection_indicator_box = gtk_menu_item_new_with_label ("Box");
    
        /*
    
    g_signal_connect(indicator_item, "clicked", G_CALLBACK(on_refresh), NULL);
    g_signal_connect(color_item, "clicked", G_CALLBACK(on_refresh), NULL);
    g_signal_connect(indicator_menu, "show", G_CALLBACK(on_menu_show), NULL);
    g_signal_connect(color_menu, "show", G_CALLBACK(on_menu_show), NULL);

    */
        gtk_combo_box_text_append_text ( GTK_COMBO_BOX_TEXT( indicator_menu ), "Plain" );
    gtk_combo_box_text_append_text ( GTK_COMBO_BOX_TEXT( indicator_menu ), "Squiggle" );
    gtk_combo_box_text_append_text ( GTK_COMBO_BOX_TEXT( indicator_menu ), "TT" );
    gtk_combo_box_text_append_text ( GTK_COMBO_BOX_TEXT( indicator_menu ), "Diagonal" );
    gtk_combo_box_text_append_text ( GTK_COMBO_BOX_TEXT( indicator_menu ), "Strike" );
    gtk_combo_box_text_append_text ( GTK_COMBO_BOX_TEXT( indicator_menu ), "Box" );*/
    
        gtk_container_add(GTK_CONTAINER(indicator_menu), selection_indicator_plain);
    gtk_container_add(GTK_CONTAINER(indicator_menu), selection_indicator_squiggle);
    gtk_container_add(GTK_CONTAINER(indicator_menu), selection_indicator_tt);
    gtk_container_add(GTK_CONTAINER(indicator_menu), selection_indicator_diagonal);
    gtk_container_add(GTK_CONTAINER(indicator_menu), selection_indicator_strike);
    gtk_container_add(GTK_CONTAINER(indicator_menu), selection_indicator_box);*/
    
        g_signal_connect(selection_indicator_plain, "activate", G_CALLBACK(set_indicator_cb), GINT_TO_POINTER(INDIC_PLAIN));
    g_signal_connect(selection_indicator_squiggle, "activate", G_CALLBACK(set_indicator_cb), GINT_TO_POINTER(INDIC_SQUIGGLE));
    g_signal_connect(selection_indicator_tt, "activate", G_CALLBACK(set_indicator_cb), GINT_TO_POINTER(INDIC_TT));
    g_signal_connect(selection_indicator_diagonal, "activate", G_CALLBACK(set_indicator_cb), GINT_TO_POINTER(INDIC_DIAGONAL));
    g_signal_connect(selection_indicator_strike, "activate", G_CALLBACK(set_indicator_cb), GINT_TO_POINTER(INDIC_STRIKE));
    g_signal_connect(selection_indicator_box, "activate", G_CALLBACK(set_indicator_cb), GINT_TO_POINTER(INDIC_BOX));



    /* plugin menu item 
    const gchar *menu_text = _("Activate MoreMarkup");
    plugin_menu_item = gtk_menu_item_new_with_mnemonic(menu_text);
    gtk_widget_show(plugin_menu_item);
    gtk_container_add(GTK_CONTAINER(geany_data->main_widgets->tools_menu), plugin_menu_item);
    */ 
    /* signals */
    //g_signal_connect(plugin_menu_item, "activate", G_CALLBACK(toggle_status), mdock);
