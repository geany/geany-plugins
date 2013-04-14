#ifndef STORE_H

#include "store/scptreestore.h"

#define scp_tree_store_get_iter_from_string(store, iter, path_str) \
	gtk_tree_model_get_iter_from_string((GtkTreeModel *) (store), (iter), (path_str))
#define scp_tree_selection_get_selected(selection, p_store, iter) \
	gtk_tree_selection_get_selected((selection), (GtkTreeModel **) (p_store), (iter))

#define STORE_H 1
#endif
