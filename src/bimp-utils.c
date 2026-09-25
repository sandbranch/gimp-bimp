/* shared utility functions */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libgimp/gimp.h>
#include "bimp-utils.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include <sys/stat.h>
#include <time.h>
#include <utime.h>

/* replace all the occurrences of 'rep' into 'orig' with text 'with' */
char* str_replace(char *orig, char *rep, char *with) 
{
    char *result;
    char *ins;
    char *tmp;
    int len_rep;
    int len_with;
    int len_front;
    int count;

    if (!orig) {
        return NULL;
    }
    if (!rep || !(len_rep = strlen(rep))) {
        return NULL;
    }
    if (!(ins = strstr(orig, rep))) {
        return NULL;
    }
    
    if (!with) {
        with = "";
    }

    len_with = strlen(with);

    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result) {
        return NULL;
    }
    
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep;
    }
    strcpy(tmp, orig);
    return result;
}

/* gets the filename from the given path 
 * (compatible with unix and win) */
char* comp_get_filename(char* path) 
{
    char *pfile;
    
    pfile = path + strlen(path);
    for (; pfile > path; pfile--)
    {
        if (*pfile == FILE_SEPARATOR)
        {
            pfile++;
            break;
        }
    }

    return pfile;
}

/* gets only the file folder from the given path 
 * (compatible with unix and win) */
char* comp_get_filefolder(char* path) 
{
    int i;
    char *folder = strdup(path);

    for (i = strlen(folder); i > 0 ; i--)
    {
        if (folder[i-1] == FILE_SEPARATOR)
        {
            folder[i] = '\0';
            break;
        }
    }
    return folder;
}

/* return TRUE if the first string 'fullstr' contains one or more occurences of substring 'search'.
 * (case-insensitive version) */
gboolean str_contains_cins(char* fullstr, char* search) {
    return (
        strstr(
            g_ascii_strdown(fullstr, strlen(fullstr)), 
            g_ascii_strdown(search, strlen(search))
        )!= NULL
    );
}

gboolean file_has_extension(char* file, char* ext) {
    return g_str_has_suffix(
        g_ascii_strdown(file, strlen(file)), 
        g_ascii_strdown(ext, strlen(ext))
    ); 
}

char* get_user_dir() 
{
    char* path = NULL;
    
#ifdef _WIN32
    path = g_strconcat(getenv("HOMEDRIVE"), getenv("HOMEPATH"), NULL);
    if (strlen(path) == 0) path = "C:\\";
#else
    path = getenv("HOME");
    if (strlen(path) == 0) path = "/";
#endif
    
    return path;
}

/* C-string case-insensitive comparison function (with gconstpointer args) */ 
int glib_strcmpi(gconstpointer str1, gconstpointer str2)
{
    return strcasecmp(str1, str2);
}

gchar** get_path_folders (char *path)
{
    return g_strsplit(path, FILE_SEPARATOR_STR, 0);
}

/* gets the current date and time in "%Y-%m-%d_%H-%M" format */
char* get_datetime() 
{
    time_t rawtime;
    struct tm * timeinfo;
    char* format;

    format = (char*)malloc(sizeof(char)*18);
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    strftime (format, 18, "%Y-%m-%d_%H-%M", timeinfo);

    return format;
}

time_t get_modification_time(char* filename) 
{
    struct stat filestats;
    if (stat(filename, &filestats) < 0) {
        return -1;
    }

    return filestats.st_mtime;
}

int set_modification_time(char* filename, time_t mtime) 
{
    struct stat filestats;
    
    if (stat(filename, &filestats) < 0) {
        return -1;
    }

    struct utimbuf new_time;
    new_time.actime = filestats.st_atime;
    new_time.modtime = mtime;
    if (utime(filename, &new_time) < 0) {
        return -1;
    }
    else return 0;
}

GdkPixbuf* pixbuf_new_from_resource(const char* path) 
{
	GdkPixbuf* pixbuf;
	pixbuf = gdk_pixbuf_new_from_resource(path, NULL);

    return pixbuf;
}

GtkWidget* image_new_from_resource(const char* path)
{
    return image_new_from_resource_scaled(path, NULL);
}

GtkWidget* image_new_from_resource_scaled(const char* path, GdkWindow *window) 
{
    return gtk_image_new_from_pixbuf(pixbuf_new_from_resource(path));
}

/* colors: GTK widgets use GdkRGBA, GIMP uses GeglColor, and .bimp files
 * store "#rrrrggggbbbb" (16 bits per channel) as GdkColor did in BIMP 2 */

GeglColor* rgba_to_gegl(const GdkRGBA* rgba)
{
    GeglColor *color = gegl_color_new(NULL);
    gegl_color_set_rgba_with_space(color, rgba->red, rgba->green, rgba->blue, rgba->alpha, babl_space("sRGB"));
    return color;
}

gchar* rgba_to_hex16(const GdkRGBA* rgba)
{
    return g_strdup_printf("#%04x%04x%04x",
        (guint)(CLAMP(rgba->red, 0, 1) * 65535 + 0.5),
        (guint)(CLAMP(rgba->green, 0, 1) * 65535 + 0.5),
        (guint)(CLAMP(rgba->blue, 0, 1) * 65535 + 0.5));
}

/* GTK 3 replacements for GTK 2's GtkAlignment and GtkTable, which BIMP's
 * windows were built with */

GtkWidget* bimp_align_new(gfloat xalign, gfloat yalign, gfloat xscale, gfloat yscale)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_set_halign(box, xscale >= 1.0 ? GTK_ALIGN_FILL :
        xalign < 0.25 ? GTK_ALIGN_START : xalign > 0.75 ? GTK_ALIGN_END : GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, yscale >= 1.0 ? GTK_ALIGN_FILL :
        yalign < 0.25 ? GTK_ALIGN_START : yalign > 0.75 ? GTK_ALIGN_END : GTK_ALIGN_CENTER);

    return box;
}

void bimp_align_set_padding(GtkWidget* align, guint top, guint bottom, guint left, guint right)
{
    gtk_widget_set_margin_top(align, top);
    gtk_widget_set_margin_bottom(align, bottom);
    gtk_widget_set_margin_start(align, left);
    gtk_widget_set_margin_end(align, right);
}

/* attaches like gtk_table_attach (left, right, top, bottom) */
void bimp_grid_attach(GtkWidget* grid, GtkWidget* child, gint left, gint right, gint top, gint bottom, gboolean hexpand, gboolean vexpand)
{
    gtk_grid_attach(GTK_GRID(grid), child, left, top, right - left, bottom - top);
    if (hexpand) gtk_widget_set_hexpand(child, TRUE);
    if (vexpand) gtk_widget_set_vexpand(child, TRUE);
}

GtkWidget* bimp_grid_new(guint row_spacing, guint col_spacing)
{
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), row_spacing);
    gtk_grid_set_column_spacing(GTK_GRID(grid), col_spacing);
    return grid;
}
