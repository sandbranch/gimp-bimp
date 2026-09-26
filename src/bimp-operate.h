#ifndef __BIMP_OPERATE_H__
#define __BIMP_OPERATE_H__

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include "bimp-manipulations.h"

typedef struct imageout_str {
    GimpImage* image;
    GimpLayer** drawables; /* NULL-terminated */
    gint drawable_count;
    char* filepath;
    char* filename;
} *image_output;

void bimp_start_batch(gpointer);
void bimp_run_batch_sync(gint*, gint*);
void bimp_init_batch(void);
gboolean bimp_apply_drawable_manipulations(image_output, gchar*, gchar*);
GimpProcedureConfig* bimp_userdef_create_config(userdef_settings, GimpProcedure**, gboolean*);
void bimp_userdef_set_image(GimpProcedure*, GimpProcedureConfig*, GimpImage*, GimpDrawable*);

#endif
