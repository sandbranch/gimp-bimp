#ifndef __BIMP_UTILS_H__
#define __BIMP_UTILS_H__

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include "bimp-manipulations.h"

char* str_replace(char*, char*, char*);
char* comp_get_filename(char*);
char* comp_get_filefolder(char*);
gboolean str_contains_cins(char*, char*);
gboolean file_has_extension(char*, char*);
char* get_user_dir(void); 
int glib_strcmpi(gconstpointer, gconstpointer);
gchar** get_path_folders (char*);
char* get_datetime(void);
time_t get_modification_time(char*);
int set_modification_time(char*, time_t);
GdkPixbuf* pixbuf_new_from_resource(const char*);
GtkWidget* image_new_from_resource(const char*);
GtkWidget* image_new_from_resource_scaled(const char*, GdkWindow*);
GeglColor* rgba_to_gegl(const GdkRGBA*);
gchar* rgba_to_hex16(const GdkRGBA*);
GtkWidget* bimp_align_new(gfloat, gfloat, gfloat, gfloat);
void bimp_align_set_padding(GtkWidget*, guint, guint, guint, guint);
void bimp_grid_attach(GtkWidget*, GtkWidget*, gint, gint, gint, gint, gboolean, gboolean);
GtkWidget* bimp_grid_new(guint, guint);

#if defined _WIN32
#define FILE_SEPARATOR '\\'
#define FILE_SEPARATOR_STR "\\"
#else
#define FILE_SEPARATOR '/'
#define FILE_SEPARATOR_STR "/"
#endif

#define min(a,b) (a < b ? a : b)
#define max(a,b) (a > b ? a : b)

#endif
