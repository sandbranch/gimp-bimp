/*
 * BIMP - Batch Image Manipulation Plugin for GIMP
 *
 * (C) 2018 - Alessandro Francesconi
 * http://www.alessandrofrancesconi.it/projects/bimp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */


#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include "bimp.h"
#include "bimp-manipulations.h"
#include "bimp-gui.h"
#include "bimp-operate.h"
#include "bimp-serialize.h"
#include "bimp-utils.h"
#include "plugin-intl.h"

GSList* bimp_input_filenames;
char* bimp_output_folder;

gint bimp_opt_alertoverwrite;
gboolean bimp_opt_keepfolderhierarchy;
gboolean bimp_opt_deleteondone;
gboolean bimp_opt_keepdates;

gboolean bimp_is_busy;
gboolean bimp_interactive;

GSList* bimp_supported_procedures;


typedef struct _Bimp      Bimp;
typedef struct _BimpClass BimpClass;

struct _Bimp
{
    GimpPlugIn parent_instance;
};

struct _BimpClass
{
    GimpPlugInClass parent_class;
};

#define BIMP_TYPE (bimp_get_type ())
GType bimp_get_type (void) G_GNUC_CONST;

static GList* bimp_query_procedures (GimpPlugIn*);
static GimpProcedure* bimp_create_procedure (GimpPlugIn*, const gchar*);
static GimpValueArray* bimp_run (GimpProcedure*, GimpProcedureConfig*, gpointer);
static GimpValueArray* bimp_run_batch (GimpProcedure*, GimpProcedureConfig*, gpointer);

G_DEFINE_TYPE (Bimp, bimp, GIMP_TYPE_PLUG_IN)

GIMP_MAIN (BIMP_TYPE)

static void bimp_class_init (BimpClass *klass)
{
    GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

    plug_in_class->query_procedures = bimp_query_procedures;
    plug_in_class->create_procedure = bimp_create_procedure;
    /* the default set_i18n uses the "bimp" domain in <plug-in folder>/locale */
}

static void bimp_init (Bimp *bimp)
{
}

static GList* bimp_query_procedures (GimpPlugIn *plug_in)
{
    GList *list = NULL;

    list = g_list_append (list, g_strdup (PLUG_IN_PROC));
    list = g_list_append (list, g_strdup (PLUG_IN_PROC_BATCH));

    return list;
}

static GimpProcedure* bimp_create_procedure (GimpPlugIn *plug_in, const gchar *name)
{
    GimpProcedure *procedure = NULL;

    if (! strcmp (name, PLUG_IN_PROC)) {
        procedure = gimp_procedure_new (plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN,
                                        bimp_run, NULL, NULL);

        gimp_procedure_set_menu_label (procedure, _("Batch Image Manipulation..."));
        gimp_procedure_add_menu_path (procedure, "<Image>/File/[Open]");
        gimp_procedure_set_documentation (procedure,
            _("Applies GIMP manipulations on groups of images"),
            _("Opens a window to build a set of manipulations (resize, crop, "
              "color correction, watermark, format change, any GIMP "
              "procedure...) and to apply it to a list of image files."),
            name);
        gimp_procedure_set_attribution (procedure,
            "Alessandro Francesconi <alessandrofrancesconi@live.it>",
            "Copyright (C) Alessandro Francesconi",
            "2018");

        gimp_procedure_add_enum_argument (procedure, "run-mode",
            "Run mode", "The run mode",
            GIMP_TYPE_RUN_MODE, GIMP_RUN_INTERACTIVE, G_PARAM_READWRITE);
    }
    else if (! strcmp (name, PLUG_IN_PROC_BATCH)) {
        procedure = gimp_procedure_new (plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN,
                                        bimp_run_batch, NULL, NULL);

        gimp_procedure_set_documentation (procedure,
            _("Applies a saved BIMP manipulation set to image files"),
            _("Loads a manipulation set saved from the BIMP window (.bimp) "
              "and applies it to each of the given files, writing the results "
              "into the output folder. Existing files are skipped unless "
              "overwrite is set."),
            name);
        gimp_procedure_set_attribution (procedure,
            "Alessandro Francesconi <alessandrofrancesconi@live.it>",
            "Copyright (C) Alessandro Francesconi",
            "2026");

        gimp_procedure_add_enum_argument (procedure, "run-mode",
            "Run mode", "The run mode",
            GIMP_TYPE_RUN_MODE, GIMP_RUN_NONINTERACTIVE, G_PARAM_READWRITE);
        gimp_procedure_add_file_argument (procedure, "set-file",
            "Manipulation set", "A manipulation set saved from BIMP (.bimp)",
            GIMP_FILE_CHOOSER_ACTION_OPEN, FALSE, NULL, G_PARAM_READWRITE);
        gimp_procedure_add_string_array_argument (procedure, "files",
            "Files", "Paths of the image files to process",
            G_PARAM_READWRITE);
        gimp_procedure_add_file_argument (procedure, "output-folder",
            "Output folder", "Folder to write the results into",
            GIMP_FILE_CHOOSER_ACTION_SELECT_FOLDER, FALSE, NULL, G_PARAM_READWRITE);
        gimp_procedure_add_boolean_argument (procedure, "overwrite",
            "Overwrite", "Overwrite existing files in the output folder",
            FALSE, G_PARAM_READWRITE);
        gimp_procedure_add_boolean_argument (procedure, "keep-folder-hierarchy",
            "Keep folder hierarchy", "Recreate the folders of the input files below the output folder",
            FALSE, G_PARAM_READWRITE);
        gimp_procedure_add_boolean_argument (procedure, "keep-dates",
            "Keep dates", "Keep the modification dates of overwritten files",
            FALSE, G_PARAM_READWRITE);

        gimp_procedure_add_int_return_value (procedure, "processed",
            "Processed", "Number of files processed",
            0, G_MAXINT, 0, G_PARAM_READWRITE);
        gimp_procedure_add_int_return_value (procedure, "errors",
            "Errors", "Number of files that failed",
            0, G_MAXINT, 0, G_PARAM_READWRITE);
    }

    return procedure;
}

static GimpValueArray* bimp_run (GimpProcedure *procedure, GimpProcedureConfig *config, gpointer run_data)
{
    GimpRunMode run_mode;

    g_object_get (config, "run-mode", &run_mode, NULL);

    if (run_mode == GIMP_RUN_NONINTERACTIVE) {
        GError *error = g_error_new_literal (GIMP_PLUG_IN_ERROR, 0,
            "plug-in-bimp opens a window; use plug-in-bimp-batch to run "
            "a saved manipulation set non-interactively");
        return gimp_procedure_new_return_values (procedure, GIMP_PDB_CALLING_ERROR, error);
    }

    bimp_interactive = TRUE;
    bimp_show_gui();

    return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}

static GimpValueArray* bimp_run_batch (GimpProcedure *procedure, GimpProcedureConfig *config, gpointer run_data)
{
    GFile *set_file = NULL, *output_folder = NULL;
    gchar **files = NULL;
    gboolean overwrite, keep_hierarchy, keep_dates;
    GimpValueArray *return_vals;
    gint processed = 0, errors = 0;
    int i;

    g_object_get (config,
        "set-file", &set_file,
        "files", &files,
        "output-folder", &output_folder,
        "overwrite", &overwrite,
        "keep-folder-hierarchy", &keep_hierarchy,
        "keep-dates", &keep_dates,
        NULL);

    if (set_file == NULL || output_folder == NULL || files == NULL || files[0] == NULL) {
        GError *error = g_error_new_literal (GIMP_PLUG_IN_ERROR, 0,
            "plug-in-bimp-batch needs a manipulation set, files and an output folder");
        return_vals = gimp_procedure_new_return_values (procedure, GIMP_PDB_CALLING_ERROR, error);
        goto out;
    }

    bimp_interactive = FALSE;

    if (!bimp_deserialize_from_file (g_file_peek_path (set_file))) {
        GError *error = g_error_new (GIMP_PLUG_IN_ERROR, 0,
            "Could not read the manipulation set %s", g_file_peek_path (set_file));
        return_vals = gimp_procedure_new_return_values (procedure, GIMP_PDB_EXECUTION_ERROR, error);
        goto out;
    }

    bimp_input_filenames = NULL;
    for (i = 0; files[i] != NULL; i++)
        bimp_input_filenames = g_slist_append (bimp_input_filenames, g_strdup (files[i]));

    bimp_output_folder = g_file_get_path (output_folder);
    bimp_opt_alertoverwrite = overwrite ? BIMP_OVERWRITE_SKIP_ASK : BIMP_DONT_OVERWRITE_SKIP_ASK;
    bimp_opt_keepfolderhierarchy = keep_hierarchy;
    bimp_opt_keepdates = keep_dates;
    bimp_opt_deleteondone = FALSE;

    bimp_run_batch_sync (&processed, &errors);

    return_vals = gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
    GIMP_VALUES_SET_INT (return_vals, 1, processed);
    GIMP_VALUES_SET_INT (return_vals, 2, errors);

out:
    g_clear_object (&set_file);
    g_clear_object (&output_folder);
    g_strfreev (files);

    return return_vals;
}

/* The procedure types that "Other GIMP procedure..." can fill in by itself,
 * with the image being processed, or that the user sets once in its dialog */
static gboolean param_is_batchable (GParamSpec *spec, gboolean *has_image)
{
    GType type = G_PARAM_SPEC_VALUE_TYPE (spec);

    if (type == GIMP_TYPE_RUN_MODE)
        return TRUE;

    if (type == GIMP_TYPE_IMAGE) {
        if (*has_image) return FALSE; /* a second image cannot come from the batch */
        *has_image = TRUE;
        return TRUE;
    }

    /* the drawable(s) of the image: filled in with its merged layer */
    if (g_type_is_a (type, GIMP_TYPE_ITEM))
        return g_type_is_a (GIMP_TYPE_LAYER, type);
    if (type == GIMP_TYPE_CORE_OBJECT_ARRAY)
        return g_strcmp0 (g_param_spec_get_name (spec), "drawables") == 0;

    if (type == GIMP_TYPE_DISPLAY ||
        type == GIMP_TYPE_PARASITE ||
        type == GIMP_TYPE_INT32_ARRAY ||
        type == GIMP_TYPE_DOUBLE_ARRAY ||
        type == GIMP_TYPE_COLOR_ARRAY ||
        type == G_TYPE_BYTES ||
        type == G_TYPE_STRV)
        return FALSE;

    return TRUE;
}

gboolean bimp_procedure_is_supported (const gchar *proc_name)
{
    GimpProcedure *proc = gimp_pdb_lookup_procedure (gimp_get_pdb (), proc_name);
    GParamSpec **specs;
    gint n_specs, i;
    gboolean has_image = FALSE, has_drawable = FALSE, ok = TRUE;

    if (proc == NULL) return FALSE;

    specs = gimp_procedure_get_arguments (proc, &n_specs);
    for (i = 0; i < n_specs && ok; i++) {
        GType type = G_PARAM_SPEC_VALUE_TYPE (specs[i]);

        ok = param_is_batchable (specs[i], &has_image);
        if (g_type_is_a (type, GIMP_TYPE_ITEM) || type == GIMP_TYPE_CORE_OBJECT_ARRAY)
            has_drawable = TRUE;
    }

    return ok && (has_image || has_drawable);
}

/*
 * Used by userdef gui, filters the full list of GIMP procedures:
 *  - by ignoring system's procedures
 *  - by ignoring procedures whose arguments cannot be given by a batch
 *    (other images or layers, arrays, displays, ...) */
void init_supported_procedures()
{
    gchar **results;
    int i;

    if (bimp_supported_procedures != NULL) return;

    results = gimp_pdb_query_procedures (
        gimp_get_pdb (),
        "^(?!.*(?:"
            "plug-in-bimp|"
            "extension-|"
            "-get-|"
            "-is-|"
            "-has-|"
            "-print-|"
            "file-glob|"
            "twain-acquire|"
            "-load|"
            "-save|"
            "-export|"
            "-thumbnail|"
            "-select|"
            "-free|"
            "-help|"
            "-temp|"
            "-undo|"
            "-copy|"
            "-paste|"
            "-cut|"
            "-buffer|"
            "-register|"
            "-metadata|"
            "-layer|"
            "-selection|"
            "-brush|"
            "-guide|"
            "-parasite|"
            "gimp-display|"
            "gimp-fonts|"
            "gimp-gimprc|"
            "gimp-gradient|"
            "gimp-online|"
            "gimp-palette|"
            "gimp-path|"
            "gimp-pattern|"
            "gimp-plugins|"
            "gimp-pdb|"
            "gimp-procedural|"
            "gimp-progress|"
            "gimp-quit|"
            "gimp-vectors|"
            "temp-procedure"
        ")).*",
        ".*", ".*", ".*", ".*", ".*", ".*", ".*"
    );

    for (i = 0; results != NULL && results[i] != NULL; i++) {
        /* check each parameter for compatibility and sort it alphabetically */
        if (bimp_procedure_is_supported (results[i])) {
            bimp_supported_procedures = g_slist_insert_sorted (bimp_supported_procedures, g_strdup (results[i]), glib_strcmpi);
        }
    }

    g_strfreev (results);
}
