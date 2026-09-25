#ifndef __BIMP_H__
#define __BIMP_H__

#include <gtk/gtk.h>
#include <glib.h>

#define PLUG_IN_FULLNAME "Batch Image Manipulation Plugin"
#define PLUG_IN_DESCRIPTION "Applies GIMP manipulations on groups of images"
#define PLUG_IN_COPYRIGHT "(C) 2021 - Alessandro Francesconi"
#define PLUG_IN_WEBSITE "http://www.alessandrofrancesconi.it/projects/bimp"
#define PLUG_IN_BINARY "bimp"
#define PLUG_IN_PROC "plug-in-bimp"
#define PLUG_IN_PROC_BATCH "plug-in-bimp-batch"
#ifndef PLUG_IN_VERSION
#define PLUG_IN_VERSION "3.0"
#endif

#define BIMP_RESULT_OK 1
#define BIMP_RESULT_WARNING 0
#define BIMP_RESULT_ERROR -1

#define BIMP_ASK_OVERWRITE 0
#define BIMP_OVERWRITE_SKIP_ASK 1
#define BIMP_DONT_OVERWRITE_SKIP_ASK 2

extern GSList* bimp_input_filenames;
extern char* bimp_output_folder;

extern gint bimp_opt_alertoverwrite;
extern gboolean bimp_opt_keepfolderhierarchy;
extern gboolean bimp_opt_deleteondone;
extern gboolean bimp_opt_keepdates;

extern gboolean bimp_is_busy;

/* TRUE when running from the dialog, FALSE for plug-in-bimp-batch */
extern gboolean bimp_interactive;

extern GSList* bimp_supported_procedures;
void init_supported_procedures(void);
gboolean bimp_procedure_is_supported(const gchar*);

#endif
