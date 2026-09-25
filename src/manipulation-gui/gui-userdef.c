/*
 * "Other GIMP procedure...": choose a GIMP procedure and set its arguments.
 *
 * GIMP 3 describes the arguments of every procedure with GParamSpecs and
 * can build a dialog for them (GimpProcedureDialog), so the arguments are
 * edited in GIMP's own dialog of the procedure, and stored serialized. The
 * image, its drawables and the run mode are filled in by the batch.
 */

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <stdlib.h>
#include <string.h>
#include "gui-userdef.h"
#include "../bimp.h"
#include "../bimp-gui.h"
#include "../bimp-manipulations.h"
#include "../bimp-operate.h"
#include "../bimp-utils.h"
#include "../plugin-intl.h"

static void init_procedure_list(void);
static int fill_procedure_list(const char*);
static gboolean procedure_visible(GtkTreeModel*, GtkTreeIter*, gpointer);
static void search_procedure (GtkEditable*, gpointer);
static void selection_changed (GtkTreeSelection*, gpointer);
static void update_procedure_box(void);
static void edit_settings (GtkButton*, gpointer);

static GtkWidget *treeview_procedures;
static GtkWidget *label_procedure, *label_blurb, *label_settings, *button_settings;
static GtkWidget *parent_dialog;
static GtkWidget *entry_search;
static GtkTreeModel *filter_model;

/* the procedure and settings being edited, until the user clicks OK */
static gchar *temp_procedure;
static gchar *temp_config;

enum
{
  LIST_ITEM = 0,
  N_COLUMNS
};

GtkWidget* bimp_userdef_gui_new(userdef_settings settings, GtkWidget *parent)
{
    GtkWidget *gui, *grid_chooser, *vbox_proc;
    GtkWidget *scroll_procedures;
    GtkWidget *label_help, *label_search;
    GtkTreeSelection *treesel_proc;

    parent_dialog = parent;

    g_free(temp_procedure);
    g_free(temp_config);
    temp_procedure = g_strdup(settings->procedure);
    temp_config = g_strdup(settings->config);

    gui = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    label_help = gtk_label_new(
        _("Choose a supported GIMP procedure from the list on the left\nand define its parameters on the right."));
    gtk_label_set_justify(GTK_LABEL(label_help), GTK_JUSTIFY_CENTER);

    grid_chooser = bimp_grid_new(5, 10);

    scroll_procedures = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_procedures), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request (scroll_procedures, PROCLIST_W, PROCLIST_H);

    label_search = gtk_label_new(g_strconcat(_("Search"), ":", NULL));
    entry_search = gtk_entry_new();

    treeview_procedures = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_procedures), FALSE);
    init_procedure_list();
    gtk_container_add(GTK_CONTAINER(scroll_procedures), treeview_procedures);

    /* the right side: what the procedure does, and its settings */
    vbox_proc = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(vbox_proc, PROCLIST_W * 1.5, -1);
    label_procedure = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label_procedure), 0);
    gtk_label_set_selectable(GTK_LABEL(label_procedure), TRUE);
    label_blurb = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label_blurb), 0);
    gtk_label_set_line_wrap(GTK_LABEL(label_blurb), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label_blurb), 40);
    label_settings = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label_settings), 0);
    button_settings = gtk_button_new_with_mnemonic(_("_Settings..."));
    gtk_widget_set_halign(button_settings, GTK_ALIGN_START);
    g_signal_connect(G_OBJECT(button_settings), "clicked", G_CALLBACK(edit_settings), NULL);

    gtk_box_pack_start(GTK_BOX(vbox_proc), label_procedure, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_proc), label_blurb, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_proc), label_settings, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_proc), button_settings, FALSE, FALSE, 0);

    bimp_grid_attach(grid_chooser, label_search, 0, 1, 0, 1, FALSE, FALSE);
    bimp_grid_attach(grid_chooser, entry_search, 1, 2, 0, 1, TRUE, FALSE);
    bimp_grid_attach(grid_chooser, scroll_procedures, 0, 2, 1, 2, TRUE, TRUE);
    bimp_grid_attach(grid_chooser, vbox_proc, 2, 3, 0, 2, TRUE, TRUE);

    gtk_box_pack_start(GTK_BOX(gui), label_help, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gui), grid_chooser, TRUE, TRUE, 0);

    treesel_proc = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_procedures));
    int sel_index = fill_procedure_list(temp_procedure);
    if (sel_index >= 0) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(sel_index, -1);
        gtk_tree_selection_select_path(treesel_proc, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview_procedures), path, NULL, TRUE, 0.5, 0.0);
        gtk_tree_path_free(path);
    }
    g_signal_connect(G_OBJECT(treesel_proc), "changed", G_CALLBACK(selection_changed), NULL);
    g_signal_connect(G_OBJECT(entry_search), "changed", G_CALLBACK(search_procedure), NULL);

    update_procedure_box();

    return gui;
}

/* initializes the GtkTreeView */
static void init_procedure_list()
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkListStore *store;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("List Items", renderer, "text", LIST_ITEM, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_procedures), column);

    /* all supported procedures once; the search only filters them */
    store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING);
    filter_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter_model), procedure_visible, NULL, NULL);
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_procedures), filter_model);
    g_object_unref(filter_model);
    g_object_unref(store);
}

static gboolean procedure_visible(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    const gchar *search = entry_search ? gtk_entry_get_text(GTK_ENTRY(entry_search)) : NULL;
    gchar *name = NULL;
    gboolean visible;

    if (search == NULL || *search == '\0') return TRUE;

    gtk_tree_model_get(model, iter, LIST_ITEM, &name, -1);
    visible = name != NULL && str_contains_cins(name, (char*)search);
    g_free(name);

    return visible;
}

/* fills the list with the procedures that can be used in a batch;
 * returns the row of 'selection', or -1 */
static int fill_procedure_list(const char* selection)
{
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model)));
    GtkTreeIter iter;
    GSList *cur;
    int row = 0, found = -1;
    
    init_supported_procedures();
    
    for (cur = bimp_supported_procedures; cur != NULL; cur = cur->next) {
        char *name = cur->data;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, LIST_ITEM, name, -1);
        if (selection != NULL && strcmp(name, selection) == 0) found = row;
        row++;
    }
    
    return found;
}

static void search_procedure (GtkEditable *editable, gpointer data)
{
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter_model));
}

static void selection_changed (GtkTreeSelection *selection, gpointer data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *name = NULL;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return;
    gtk_tree_model_get(model, &iter, LIST_ITEM, &name, -1);

    if (g_strcmp0(name, temp_procedure) != 0) {
        /* another procedure: its own default settings */
        g_free(temp_procedure);
        g_free(temp_config);
        temp_procedure = name;
        temp_config = NULL;
        update_procedure_box();
    }
    else {
        g_free(name);
    }
}

/* the arguments the user sets; the others come from the batch */
static GList* editable_arguments(GimpProcedure *proc)
{
    GParamSpec **specs;
    gint n_specs, i;
    GList *names = NULL;

    specs = gimp_procedure_get_arguments(proc, &n_specs);
    for (i = 0; i < n_specs; i++) {
        GType type = G_PARAM_SPEC_VALUE_TYPE(specs[i]);

        if (type == GIMP_TYPE_RUN_MODE || type == GIMP_TYPE_IMAGE ||
            g_type_is_a(type, GIMP_TYPE_ITEM) || type == GIMP_TYPE_CORE_OBJECT_ARRAY)
            continue;
        names = g_list_append(names, (gpointer)g_param_spec_get_name(specs[i]));
    }

    return names;
}

static void update_procedure_box()
{
    GimpProcedure *proc = temp_procedure ? gimp_pdb_lookup_procedure(gimp_get_pdb(), temp_procedure) : NULL;
    GList *args;

    gtk_dialog_set_response_sensitive(GTK_DIALOG(parent_dialog), GTK_RESPONSE_ACCEPT, proc != NULL);

    if (proc == NULL) {
        gtk_label_set_text(GTK_LABEL(label_procedure), temp_procedure ? temp_procedure : "");
        gtk_label_set_text(GTK_LABEL(label_blurb), temp_procedure ? _("This GIMP has no such procedure.") : "");
        gtk_label_set_text(GTK_LABEL(label_settings), "");
        gtk_widget_set_sensitive(button_settings, FALSE);
        return;
    }

    gchar *markup = g_markup_printf_escaped("<b>%s</b>",
        gimp_procedure_get_menu_label(proc) ? gimp_procedure_get_menu_label(proc) : temp_procedure);
    gtk_label_set_markup(GTK_LABEL(label_procedure), markup);
    g_free(markup);
    gtk_widget_set_tooltip_text(label_procedure, temp_procedure);
    gtk_label_set_text(GTK_LABEL(label_blurb), gimp_procedure_get_blurb(proc) ? gimp_procedure_get_blurb(proc) : "");

    args = editable_arguments(proc);
    if (args == NULL) {
        gtk_label_set_text(GTK_LABEL(label_settings), _("This procedure has no settings."));
        gtk_widget_set_sensitive(button_settings, FALSE);
    }
    else {
        gtk_label_set_text(GTK_LABEL(label_settings), temp_config ? _("Custom settings") : _("Default settings"));
        gtk_widget_set_sensitive(button_settings, TRUE);
    }
    g_list_free(args);
}

/* serializes the settings without the image and drawables, which only
 * exist while GIMP runs */
static gchar* serialize_settings(GimpProcedure *proc, GimpProcedureConfig *config)
{
    bimp_userdef_set_image(proc, config, NULL, NULL);

    return gimp_config_serialize_to_string(GIMP_CONFIG(config), NULL);
}

static void edit_settings (GtkButton *button, gpointer data)
{
    struct manip_userdef_set temp = { temp_procedure, temp_config };
    GimpProcedure *proc = NULL;
    GimpProcedureConfig *config = bimp_userdef_create_config(&temp, &proc);
    GtkWidget *dialog;
    GList *args;

    if (config == NULL) return;

    dialog = gimp_procedure_dialog_new(proc, config,
        gimp_procedure_get_menu_label(proc) ? gimp_procedure_get_menu_label(proc) : temp_procedure);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent_dialog));
    /* the dialog it opens from is modal: this one must be too, to get input */
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gimp_procedure_dialog_set_ok_label(GIMP_PROCEDURE_DIALOG(dialog), _("_OK"));

    args = editable_arguments(proc);
    gimp_procedure_dialog_fill_list(GIMP_PROCEDURE_DIALOG(dialog), args);
    g_list_free(args);

    if (gimp_procedure_dialog_run(GIMP_PROCEDURE_DIALOG(dialog))) {
        g_free(temp_config);
        temp_config = serialize_settings(proc, config);
    }
    gtk_widget_destroy(dialog);
    g_object_unref(config);

    update_procedure_box();
}

void bimp_userdef_save(userdef_settings orig_settings)
{
    if (temp_procedure == NULL) return;

    g_free(orig_settings->procedure);
    g_free(orig_settings->config);
    orig_settings->procedure = g_strdup(temp_procedure);
    orig_settings->config = g_strdup(temp_config);
}
