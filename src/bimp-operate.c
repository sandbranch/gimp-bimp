// Functions called when the user clicks on 'APPLY', or by plug-in-bimp-batch


#include <string.h>
#include <math.h>
#include <float.h>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimpbase/gimpbase.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "bimp-operate.h"
#include "bimp.h"
#include "bimp-manipulations.h"
#include "bimp-gui.h"
#include "bimp-utils.h"
#include "bimp-serialize.h"
#include "plugin-intl.h"

static gboolean process_image(gpointer);

static gboolean apply_manipulation(manipulation, image_output);
static gboolean apply_resize(resize_settings, image_output);
static gboolean apply_crop(crop_settings, image_output);
static gboolean apply_fliprotate(fliprotate_settings, image_output);
static gboolean apply_color(color_settings, image_output);
static gboolean apply_sharpblur(sharpblur_settings, image_output);
static gboolean apply_watermark(watermark_settings, image_output);
static void calc_watermark_xy (int, int, int, int, watermark_position, int, gdouble*, gdouble*);
static gboolean apply_userdef(userdef_settings, image_output);
static gboolean apply_rename(rename_settings, image_output, char*);

static gboolean image_save(format_type, image_output, format_params);

static int overwrite_result(char*, GtkWidget*);
static void report_error(const gchar*);
static void refresh_drawables(image_output);

static char* current_datetime;
static int processed_count;
static int success_count;
static int total_images;

static char* common_folder_path;

static gboolean list_contains_changeformat;
static gboolean list_contains_rename;
static gboolean list_contains_watermark;
static gboolean list_contains_savingplugin;

// set when a manipulation of the current image failed: the image counts as an error
static gboolean manipulation_failed;

// set of variables to be used when doing Curve color correction
// they are global so the batch process will read the source curve file once
static gboolean colorcurve_init;
static gboolean colorcurve_ok;
static int colorcurve_num_points_v;
static gdouble* colorcurve_ctr_points_v;
static int colorcurve_num_points_r;
static gdouble* colorcurve_ctr_points_r;
static int colorcurve_num_points_g;
static gdouble* colorcurve_ctr_points_g;
static int colorcurve_num_points_b;
static gdouble* colorcurve_ctr_points_b;
static int colorcurve_num_points_a;
static gdouble* colorcurve_ctr_points_a;

/* in the window an error opens a dialog; in a batch it goes to the terminal */
static void report_error(const gchar* message)
{
    if (bimp_interactive) {
        bimp_show_error_dialog((char*)message, bimp_window_main);
    }
    else {
        g_printerr("BIMP: %s\n", message);
    }
}

static void progress_set(double fraction, char* text)
{
    if (bimp_interactive) bimp_progress_bar_set(fraction, text);
}

static void batch_prepare(void)
{
    // initialization
    g_print("\nBIMP - Batch Manipulation Plugin\nStart batch processing...\n");
    processed_count = 0;
    success_count = 0;
    total_images = g_slist_length(bimp_input_filenames);
    progress_set(0.0, "");

    bimp_init_batch();

    current_datetime = get_datetime();
    common_folder_path = NULL;

    if (bimp_opt_keepfolderhierarchy){
        int i, j;
        gboolean need_hierarchy = FALSE;
        char * path = NULL;
        char ** common_folder;
        char ** current_folder;
        size_t common_folder_size, current_folder_size;

        path = comp_get_filefolder(g_slist_nth(bimp_input_filenames,0)->data);

        common_folder = get_path_folders(path);
        common_folder_size = 0;
        for (common_folder_size = 0; common_folder[common_folder_size] != NULL; ++common_folder_size);

        for (i=1; i < total_images ; i++)
        {
            path = comp_get_filefolder(g_slist_nth(bimp_input_filenames,i)->data);
            current_folder = get_path_folders (path);
            for (current_folder_size = 0; current_folder[current_folder_size] != NULL; ++current_folder_size);

            // The common path is at most as long as the shortest path
            while (common_folder_size > current_folder_size)
            {
                need_hierarchy = TRUE;
                g_free(common_folder[common_folder_size-1]);
                common_folder[common_folder_size-1] = NULL;
                common_folder_size--;
            }

            for (j=0; j < common_folder_size; ++j)
            {
                if (strcmp(common_folder[j], current_folder[j]) != 0) {
                    need_hierarchy = TRUE;
                    while (common_folder_size > j)
                    {
                        g_free(common_folder[common_folder_size-1]);
                        common_folder[common_folder_size-1] = NULL;
                        common_folder_size--;
                    }
                    break;
                }
            }

            g_strfreev(current_folder);
        }

        if (need_hierarchy)
            common_folder_path = g_strjoinv(FILE_SEPARATOR_STR, common_folder);

        g_strfreev(common_folder);
    }
}

void bimp_start_batch(gpointer parent_dialog)
{
    bimp_set_busy(TRUE);
    batch_prepare();

    // one image per main loop iteration, so the window stays responsive
    g_idle_add((GSourceFunc)process_image, parent_dialog);
}

/* runs the whole batch at once, without a window (plug-in-bimp-batch) */
void bimp_run_batch_sync(gint* processed, gint* errors)
{
    bimp_is_busy = TRUE;
    batch_prepare();

    while (process_image(NULL));

    *processed = processed_count;
    *errors = processed_count - success_count;
}

void bimp_init_batch()
{
    list_contains_changeformat = bimp_list_contains_manip(MANIP_CHANGEFORMAT);
    list_contains_rename = bimp_list_contains_manip(MANIP_RENAME);
    list_contains_watermark = bimp_list_contains_manip(MANIP_WATERMARK);
    list_contains_savingplugin = bimp_list_contains_savingplugin();

    colorcurve_init = FALSE;
}

static gboolean process_image(gpointer parent)
{
    gboolean success = TRUE;

    image_output imageout = (image_output)g_malloc0(sizeof(struct imageout_str));
    char* orig_filename = NULL;
    char* orig_basename = NULL;
    char* orig_file_ext = NULL;
    char* output_file_comp = NULL;

    // store original file path and name
    orig_filename = g_slist_nth (bimp_input_filenames, processed_count)->data;
    orig_basename = g_strdup(comp_get_filename(orig_filename));

    // store original extension and check error cases
    orig_file_ext = g_strdup(strrchr(orig_basename, '.'));
    if (orig_file_ext == NULL) {
        /* under Linux, GtkFileChooser lets to pick an image file without extension, but GIMP cannot
         * save it back if its format remains unchanged. Operation can continue only if a MANIP_CHANGEFORMAT
         * is present */
        if (list_contains_changeformat) {
            orig_file_ext = g_malloc0(sizeof(char));
        }
        else {
            gchar *msg = g_strdup_printf(_("Can't save image \"%s\": input file has no extension.\nYou can solve this error by adding a \"Change format or compression\" step"), orig_basename);
            report_error(msg);
            g_free(msg);
            success = FALSE;
            goto process_end;
        }
    }
    else if (g_ascii_strcasecmp(orig_file_ext, ".svg") == 0 && !list_contains_changeformat) {
        gchar *msg = g_strdup_printf(_("GIMP can't save %s back to its original SVG format.\nYou can solve this error by adding a \"Change format or compression\" step"), orig_basename);
        report_error(msg);
        g_free(msg);
        success = FALSE;
        goto process_end;
    }

    g_print("\nWorking on file %d of %d (%s)\n", processed_count + 1, total_images, orig_filename);
    if (bimp_interactive) {
        gchar *text = g_strdup_printf(_("Working on file \"%s\"..."), orig_basename);
        progress_set(((double)processed_count)/total_images, text);
        g_free(text);
    }

    // rename and save process...
    orig_basename[strlen(orig_basename) - strlen(orig_file_ext)] = '\0'; // remove extension from basename

    // check if a rename pattern is defined
    if(list_contains_rename) {
        g_print("Applying RENAME...\n");
        apply_rename((rename_settings)(bimp_list_get_manip(MANIP_RENAME))->settings, imageout, orig_basename);
    }
    else {
        imageout->filename = g_strdup(orig_basename);
    }

    // To keep the folder hierarchy
    if (common_folder_path == NULL)    {
        // Not selected or required, everything goes into the same destination folder
        output_file_comp = g_malloc0(sizeof(char));
    }
    else {
        // keep folders to add to output path
        output_file_comp =
            g_strndup(&orig_filename[strlen(common_folder_path)+1],
            strlen(orig_filename)-(strlen(common_folder_path)+1)
            -strlen(orig_basename)-strlen(orig_file_ext));
    }

    if (strlen(output_file_comp) > 0) {
#ifdef _WIN32
        // Clean output_file_comp
        // Should only be concerned for ':' in Drive letter
        int i;
        for (i = 0; i < strlen(output_file_comp); ++i)
            if ( output_file_comp[i] == ':' )
                output_file_comp[i] = '_';
#endif
        // Create path if needed
        gchar *dir = g_strconcat(bimp_output_folder, FILE_SEPARATOR_STR, output_file_comp, NULL);
        g_mkdir_with_parents(dir, 0777);
        g_free(dir);
    }

    // save the final image in output dir with proper format and params
    format_type final_format = -1;
    format_params params = NULL;
    gchar *name;

    if(list_contains_changeformat) {
        changeformat_settings settings = (changeformat_settings)(bimp_list_get_manip(MANIP_CHANGEFORMAT))->settings;
        final_format = settings->format;
        params = settings->params;

        g_print("Changing FORMAT to %s\n", format_type_string[final_format][0]);
        name = g_strconcat(imageout->filename, ".", format_type_string[final_format][0], NULL); // append new file extension
    }
    else {
        // if not specified, save in original format
        name = g_strconcat(imageout->filename, orig_file_ext, NULL); // append old file extension
        final_format = -1;
    }
    g_free(imageout->filename);
    imageout->filename = name;
    imageout->filepath = g_strconcat(bimp_output_folder, FILE_SEPARATOR_STR, output_file_comp, imageout->filename, NULL); // build new path

    // check if writing possible
    gboolean will_overwrite = FALSE;
    if (bimp_opt_alertoverwrite != BIMP_OVERWRITE_SKIP_ASK) {
        // file already exists ?
        will_overwrite = g_file_test(imageout->filepath, G_FILE_TEST_IS_REGULAR);
        if (will_overwrite) {
            // "Don't overwrite" without confirmation
            if (bimp_opt_alertoverwrite == BIMP_DONT_OVERWRITE_SKIP_ASK) {
                g_print("Destination file already exists and won't be overwritten\n");
                goto process_end;
            }
            else {
                // Ask what to do
                int ow_res = overwrite_result(imageout->filepath, parent);
                if (ow_res == 0) {
                    g_print("Destination file already exists and user select to don't overwrite\n");
                    goto process_end;
                }
            }
        }
    }
    else {
        will_overwrite = g_file_test(imageout->filepath, G_FILE_TEST_IS_REGULAR);
    }

    // apply all the main manipulations
    if (!bimp_apply_drawable_manipulations(imageout, (gchar*)orig_filename, (gchar*)orig_basename)) {
        gchar *msg = g_strdup_printf(_("Could not open \"%s\""), orig_filename);
        report_error(msg);
        g_free(msg);
        success = FALSE;
        goto process_end;
    }
    if (manipulation_failed) {
        // still saved, with the manipulations that worked, like before
        g_printerr("BIMP: some manipulations failed on %s\n", orig_filename);
        success = FALSE;
    }

    time_t mod_time = -1;
    if (will_overwrite && bimp_opt_keepdates) {
        // I must keep the dates even if the file has been overwritten
        mod_time = get_modification_time(imageout->filepath);
        if (mod_time == -1) g_print("An error occurred when retrieving the modification date of file.\n");
    }

    // Save
    g_print("Saving file %s in %s\n", imageout->filename, imageout->filepath);
    if (!image_save(final_format, imageout, params)) {
        g_printerr("BIMP: could not save %s\n", imageout->filepath);
        success = FALSE;
    }

    if (will_overwrite && bimp_opt_keepdates && mod_time > -1) {
        // replace with the old dates
        int res = set_modification_time(imageout->filepath, mod_time);
        if (res == -1) g_print("An error occurred when replacing the modification date of file.\n");
    }

    gimp_image_delete(imageout->image);

process_end:

    g_free(orig_basename);
    g_free(orig_file_ext);
    g_free(output_file_comp);
    g_free(imageout->filename);
    g_free(imageout->filepath);
    g_free(imageout->drawables);
    g_free(imageout);

    processed_count++;
    if (success) success_count++;

    if (!bimp_is_busy) {
        progress_set(0.0, _("Operations stopped"));
        g_print("\nStopped, %d files processed.\n", processed_count);
        return FALSE;
    }
    else {
        if (processed_count == total_images) {
            int errors_count = processed_count - success_count;
            if (bimp_interactive) {
                gchar *text = g_strdup_printf(_("End, all files have been processed with %d errors"), errors_count);
                progress_set(1.0, text);
                g_free(text);
            }
            g_print("\nEnd, %d files have been processed with %d errors.\n", processed_count, errors_count);

            if (bimp_interactive) bimp_set_busy(FALSE);
            else bimp_is_busy = FALSE;

            return FALSE;
        }
        else {
            return TRUE;
        }
    }
}

static void refresh_drawables(image_output out)
{
    g_free(out->drawables);
    out->drawables = gimp_image_get_layers(out->image);
    for (out->drawable_count = 0; out->drawables[out->drawable_count] != NULL; out->drawable_count++);
}

/* merges the visible layers into one. GIMP 3 gives the merged layer an alpha
 * channel even when there is only one layer, which would turn every RGB or
 * gray image into RGBA or gray with alpha: a single visible layer that covers
 * the image is kept as it is. */
static GimpLayer* merge_layers(GimpImage *image)
{
    GimpLayer **layers = gimp_image_get_layers(image);
    GimpLayer *layer = NULL;
    gint x, y;

    if (layers[0] != NULL && layers[1] == NULL &&
        gimp_item_get_visible(GIMP_ITEM(layers[0])) &&
        gimp_layer_get_opacity(layers[0]) == 100.0 &&
        gimp_drawable_get_width(GIMP_DRAWABLE(layers[0])) == gimp_image_get_width(image) &&
        gimp_drawable_get_height(GIMP_DRAWABLE(layers[0])) == gimp_image_get_height(image) &&
        gimp_drawable_get_offsets(GIMP_DRAWABLE(layers[0]), &x, &y) && x == 0 && y == 0) {
        layer = layers[0];
    }
    g_free(layers);

    return layer != NULL ? layer : gimp_image_merge_visible_layers(image, GIMP_CLIP_TO_IMAGE);
}

/* loads the image and applies every manipulation except rename and format;
 * returns FALSE if the image cannot be loaded */
gboolean bimp_apply_drawable_manipulations(image_output imageout, gchar* orig_filename, gchar* orig_basename)
{
    GFile *file = g_file_new_for_path(orig_filename);
    imageout->image = gimp_file_load(GIMP_RUN_NONINTERACTIVE, file);
    g_object_unref(file);
    manipulation_failed = FALSE;

    if (imageout->image == NULL) return FALSE;

    // stop saving the undo steps for this session
    gimp_image_undo_freeze(imageout->image);

    refresh_drawables(imageout);
    g_print("Total drawables count: %d\n", imageout->drawable_count);

    // apply all the intermediate manipulations
    g_slist_foreach(bimp_selected_manipulations, (GFunc)apply_manipulation, imageout);

    //  watermark at last
    if(list_contains_watermark) {
        GSList* watermarks = bimp_list_get_manip_all(MANIP_WATERMARK);
        GSList *iterator = NULL;
        for (iterator = watermarks; iterator; iterator = iterator->next) {
            g_print("Applying WATERMARK...\n");
            if (!apply_watermark((watermark_settings)(((manipulation)(iterator->data))->settings), imageout)) {
                g_printerr("BIMP: the watermark failed\n");
                manipulation_failed = TRUE;
            }
        }
        g_slist_free(watermarks);
    }

    // re-enable undo
    gimp_image_undo_thaw(imageout->image);

    return TRUE;
}

static gboolean apply_manipulation(manipulation man, image_output out)
{
    gboolean success = TRUE;

    if (man->type == MANIP_RESIZE) {
        g_print("Applying RESIZE...\n");
        success = apply_resize((resize_settings)(man->settings), out);
    }
    else if (man->type == MANIP_CROP) {
        g_print("Applying CROP...\n");
        success = apply_crop((crop_settings)(man->settings), out);
    }
    else if (man->type == MANIP_FLIPROTATE) {
        g_print("Applying FLIP OR ROTATE...\n");
        success = apply_fliprotate((fliprotate_settings)(man->settings), out);
    }
    else if (man->type == MANIP_COLOR) {
        g_print("Applying COLOR CORRECTION...\n");
        success = apply_color((color_settings)(man->settings), out);
    }
    else if (man->type == MANIP_SHARPBLUR) {
        g_print("Applying SHARPBLUR...\n");
        success = apply_sharpblur((sharpblur_settings)(man->settings), out);
    }
    else if (man->type == MANIP_USERDEF && ((userdef_settings)(man->settings))->procedure != NULL) {
        g_print("Applying %s...\n", ((userdef_settings)(man->settings))->procedure);
        success = apply_userdef((userdef_settings)(man->settings), out);
    }

    if (!success) manipulation_failed = TRUE;

    return success;
}

static gboolean apply_resize(resize_settings settings, image_output out)
{
    gboolean success = TRUE;
    gint orig_w, orig_h, final_w, final_h, view_w, view_h;
    gdouble orig_res_x, orig_res_y;

    if (settings->change_res) {
        success = gimp_image_get_resolution(
            out->image,
            &orig_res_x,
            &orig_res_y
        );

        if ((settings->new_res_x != orig_res_x) || (settings->new_res_y != orig_res_y)) {
            // change resolution
            success = gimp_image_set_resolution(
                out->image,
                settings->new_res_x,
                settings->new_res_y
            );
        }
    }

    orig_w = gimp_image_get_width(out->image);
    orig_h = gimp_image_get_height(out->image);

    if (settings->resize_mode_width == RESIZE_DISABLE && settings->resize_mode_height == RESIZE_DISABLE) {
        return success;
    }

    gdouble newwpct, newwpctmax;
    if (settings->resize_mode_width == RESIZE_PERCENT) {
        newwpct = newwpctmax = settings->new_w_pc / 100.0;
    }
    else if(settings->resize_mode_width == RESIZE_PIXEL) {
        newwpct = newwpctmax = (double)settings->new_w_px / (double)orig_w;
    }
    else {
        newwpct = 1;
        newwpctmax = DBL_MAX;
    }

    gdouble newhpct, newhpctmax;
    if (settings->resize_mode_height == RESIZE_PERCENT) {
        newhpct = newhpctmax = settings->new_h_pc / 100.0;
    }
    else if(settings->resize_mode_height == RESIZE_PIXEL) {
        newhpct = newhpctmax = (double)settings->new_h_px / (double)orig_h;
    }
    else {
        newhpct = 1;
        newhpctmax = DBL_MAX;
    }

    if(settings->stretch_mode == STRETCH_ASPECT) {
        gdouble newpct = min(newwpctmax, newhpctmax);

        final_w = view_w = round(orig_w * newpct);
        final_h = view_h = round(orig_h * newpct);
    }
    else if (settings->stretch_mode == STRETCH_PADDED) {
        gdouble newpct = min(newwpctmax, newhpctmax);

        final_w = round(orig_w * newpct);
        final_h = round(orig_h * newpct);
        view_w = round(orig_w * newwpct);
        view_h = round(orig_h * newhpct);
    }
    else {
        final_w = view_w = round(orig_w * newwpct);
        final_h = view_h = round(orig_h * newhpct);
    }

    gimp_context_push();
    gimp_context_set_interpolation (settings->interpolation);
    success = gimp_image_scale (
        out->image,
        final_w,
        final_h
    );
    gimp_context_pop();

    // add a padding if requested
    if (settings->stretch_mode == STRETCH_PADDED) {

        // the padding will be drawn using a coloured layer at the bottom of the image
        GimpImageType layerType = gimp_image_get_base_type(out->image) * 2; /* RGB, GRAY or INDEXED */
        if (gimp_drawable_has_alpha(GIMP_DRAWABLE(out->drawables[0]))) layerType++;

        GimpLayer *layer = gimp_layer_new(
            out->image,
            "padding_layer",
            view_w, view_h,
            layerType,
            settings->padding_color.alpha * 100,
            GIMP_LAYER_MODE_NORMAL
        );

        gimp_image_insert_layer(out->image, layer, NULL, 0);
        gimp_image_lower_item_to_bottom(out->image, GIMP_ITEM(layer));

        // fill it with the selected color
        GeglColor *background = rgba_to_gegl(&(settings->padding_color));
        gimp_context_push();
        gimp_context_set_background(background);
        gimp_drawable_fill(GIMP_DRAWABLE(layer), GIMP_FILL_BACKGROUND);
        gimp_context_pop();
        g_object_unref(background);

        // move it to the center
        gimp_item_transform_translate(GIMP_ITEM(layer), -abs(view_w - final_w) / 2, -abs(view_h - final_h) / 2);

        // finish changing the canvas size accordingly
        success = gimp_image_resize_to_layers(out->image);
        refresh_drawables(out);
    }

    return success;
}

static gboolean apply_crop(crop_settings settings, image_output out)
{
    gboolean success = TRUE;
    gint newWidth, newHeight, oldWidth, oldHeight, posX = 0, posY = 0;
    gboolean keepX = FALSE, keepY = FALSE;

    oldWidth = gimp_image_get_width(out->image);
    oldHeight = gimp_image_get_height(out->image);

    if (settings->manual) {
        newWidth = min(oldWidth, settings->new_w);
        newHeight = min(oldHeight, settings->new_h);
    }
    else {
        float ratio1, ratio2;
        if (settings->ratio == CROP_PRESET_CUSTOM) {
            ratio1 = settings->custom_ratio1;
            ratio2 = settings->custom_ratio2;
        }
        else {
            ratio1 = (float)crop_preset_ratio[settings->ratio][0];
            ratio2 = (float)crop_preset_ratio[settings->ratio][1];
        }

        if (( (float)oldWidth / oldHeight ) > ( ratio1 / ratio2) ) {
            // crop along the width
            newHeight = oldHeight;
            newWidth = round(( ratio1 * (float)newHeight ) / ratio2);
            keepY = TRUE;
        } else {
            // crop along the height
            newWidth = oldWidth;
            newHeight = round(( ratio2 * (float)newWidth) / ratio1);
            keepX = TRUE;
        }
    }

    switch (settings->start_pos) {
        case CROP_START_TL:
            posX = 0;
            posY = 0;
            break;

        case CROP_START_TR:
            posX = (oldWidth - newWidth);
            posY = 0;
            break;

        case CROP_START_BL:
            posX = 0;
            posY = (oldHeight - newHeight);
            break;

        case CROP_START_BR:
            posX = (oldWidth - newWidth);
            posY = (oldHeight - newHeight);
            break;

        default:
            if (!keepX) posX = (oldWidth - newWidth) / 2;
            if (!keepY) posY = (oldHeight - newHeight) / 2;
            break;
    }

    success = gimp_image_crop (
        out->image,
        newWidth,
        newHeight,
        posX,
        posY
    );

    return success;
}

static gboolean apply_fliprotate(fliprotate_settings settings, image_output out)
{
    gboolean success = TRUE;

    if (settings->flip_h) {
        success = gimp_image_flip (out->image, GIMP_ORIENTATION_HORIZONTAL);
    }

    if (settings->flip_v) {
        success = gimp_image_flip (out->image, GIMP_ORIENTATION_VERTICAL);
    }

    if (settings->rotate) {
        success = gimp_image_rotate (out->image, settings->rotation_type);
    }

    return success;
}

static gboolean apply_color(color_settings settings, image_output out)
{
    gboolean success = TRUE;
    int i;

    GimpDrawable *default_drawable = GIMP_DRAWABLE(out->drawables[0]);
    if (settings->brightness != 0 || settings->contrast != 0) {
        // brightness or contrast have been modified, apply the manipulation

        if (!gimp_drawable_is_rgb(default_drawable)) {
            gimp_image_convert_rgb(out->image);
        }

        for (i = 0; i < out->drawable_count; i++) {
            GimpDrawableFilter *filter = gimp_drawable_filter_new(
                GIMP_DRAWABLE(out->drawables[i]), "gimp:brightness-contrast", "BIMP");
            GimpDrawableFilterConfig *config = gimp_drawable_filter_get_config(filter);
            g_object_set(config,
                "brightness", settings->brightness,
                "contrast", settings->contrast,
                NULL);
            gimp_drawable_filter_update(filter);
            gimp_drawable_merge_filter(GIMP_DRAWABLE(out->drawables[i]), filter);
        }
    }

    if (settings->grayscale && !gimp_drawable_is_gray(default_drawable)) {
        // do grayscale conversion
        success = gimp_image_convert_grayscale(out->image);
    }

    if (settings->levels_auto) {
        // do levels correction
        for (i = 0; i < out->drawable_count; i++) {
            success = gimp_drawable_levels_stretch(GIMP_DRAWABLE(out->drawables[i]));
        }
    }

    if (settings->curve_file != NULL && !gimp_drawable_is_indexed(default_drawable)) {
        // apply curve

        if (!colorcurve_init) { // read from the curve file only the first time
            colorcurve_num_points_v = colorcurve_num_points_r = colorcurve_num_points_g = 0;
            colorcurve_num_points_b = colorcurve_num_points_a = 0;
            colorcurve_ok = parse_curve_file(
                settings->curve_file,
                &colorcurve_num_points_v, &colorcurve_ctr_points_v,
                &colorcurve_num_points_r, &colorcurve_ctr_points_r,
                &colorcurve_num_points_g, &colorcurve_ctr_points_g,
                &colorcurve_num_points_b, &colorcurve_ctr_points_b,
                &colorcurve_num_points_a, &colorcurve_ctr_points_a
            );
            if (!colorcurve_ok) g_printerr("BIMP: could not read the curve file %s\n", settings->curve_file);

            colorcurve_init = TRUE;
        }
        success = colorcurve_ok;

        if (success) {
            /* gimp:curves needs a GimpCurve object, which plug-ins cannot
             * create yet: the spline call is the way for plug-ins */
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            for (i = 0; i < out->drawable_count; i++) {
                GimpDrawable *d = GIMP_DRAWABLE(out->drawables[i]);

                if (colorcurve_num_points_v >= 4 && colorcurve_num_points_v <= 34) {
                    success = gimp_drawable_curves_spline(d, GIMP_HISTOGRAM_VALUE, colorcurve_num_points_v, colorcurve_ctr_points_v);
                }

                if (colorcurve_num_points_r >= 4 && colorcurve_num_points_r <= 34) {
                    success = gimp_drawable_curves_spline(d, GIMP_HISTOGRAM_RED, colorcurve_num_points_r, colorcurve_ctr_points_r);
                }

                if (colorcurve_num_points_g >= 4 && colorcurve_num_points_g <= 34) {
                    success = gimp_drawable_curves_spline(d, GIMP_HISTOGRAM_GREEN, colorcurve_num_points_g, colorcurve_ctr_points_g);
                }

                if (colorcurve_num_points_b >= 4 && colorcurve_num_points_b <= 34) {
                    success = gimp_drawable_curves_spline(d, GIMP_HISTOGRAM_BLUE, colorcurve_num_points_b, colorcurve_ctr_points_b);
                }

                if (colorcurve_num_points_a >= 4 && colorcurve_num_points_a <= 34 && gimp_drawable_has_alpha(d)) {
                    success = gimp_drawable_curves_spline(d, GIMP_HISTOGRAM_ALPHA, colorcurve_num_points_a, colorcurve_ctr_points_a);
                }
            }
            G_GNUC_END_IGNORE_DEPRECATIONS
        }
    }

    return success;
}

/* GIMP 2's plug-in-gauss took a radius; GEGL's gaussian blur takes the
 * standard deviation. GIMP converts between them this way. */
static gdouble gauss_radius_to_std_dev(gdouble radius)
{
    return sqrt(-(radius * radius) / (2.0 * log(1.0 / 255.0)));
}

static gboolean apply_sharpblur(sharpblur_settings settings, image_output out)
{
    gboolean success = TRUE;
    int i;

    if (settings->amount < 0) {
        // sharpen: GIMP 2's plug-in-sharpen is gone; an unsharp mask with a
        // small radius gives the same kind of result, stronger with the amount
        for (i = 0; i < out->drawable_count; i++) {
            GimpDrawableFilter *filter = gimp_drawable_filter_new(
                GIMP_DRAWABLE(out->drawables[i]), "gegl:unsharp-mask", "BIMP");
            g_object_set(gimp_drawable_filter_get_config(filter),
                "std-dev", 1.0,
                "scale", -(settings->amount) / 100.0 * 2.0,
                "threshold", 0.0,
                NULL);
            gimp_drawable_filter_update(filter);
            gimp_drawable_merge_filter(GIMP_DRAWABLE(out->drawables[i]), filter);
        }
    } else if (settings->amount > 0){
        // blur
        float minsize = min(gimp_image_get_width(out->image)/4, gimp_image_get_height(out->image)/4);
        float radius = (minsize / 100) * settings->amount;
        gdouble std_dev = gauss_radius_to_std_dev(radius);

        for (i = 0; i < out->drawable_count; i++) {
            GimpDrawableFilter *filter = gimp_drawable_filter_new(
                GIMP_DRAWABLE(out->drawables[i]), "gegl:gaussian-blur", "BIMP");
            g_object_set(gimp_drawable_filter_get_config(filter),
                "std-dev-x", std_dev,
                "std-dev-y", std_dev,
                NULL);
            gimp_drawable_filter_update(filter);
            gimp_drawable_merge_filter(GIMP_DRAWABLE(out->drawables[i]), filter);
        }
    }

    return success;
}

/* finds the GIMP font for a Pango font description such as "DejaVu Sans Bold 16px" */
static GimpFont* find_font(PangoFontDescription *desc)
{
    PangoFontDescription *nosize = pango_font_description_copy(desc);
    GimpFont *font;
    gchar *name;

    pango_font_description_unset_fields(nosize, PANGO_FONT_MASK_SIZE);
    name = pango_font_description_to_string(nosize);
    font = gimp_font_get_by_name(name);
    g_free(name);
    pango_font_description_free(nosize);

    if (font == NULL && pango_font_description_get_family(desc) != NULL) {
        GimpFont **fonts = gimp_fonts_get_list(pango_font_description_get_family(desc));
        if (fonts != NULL && fonts[0] != NULL) font = fonts[0];
        g_free(fonts);
    }
    if (font == NULL) font = gimp_font_get_by_name("Sans-serif");

    return font;
}

static gboolean apply_watermark(watermark_settings settings, image_output out)
{
    gboolean success = TRUE;
    GimpLayer *layer;
    gdouble posX, posY;
    gint wmwidth, wmheight, wmasc, wmdesc;

    gint imgwidth = gimp_image_get_width(out->image);
    gint imgheight = gimp_image_get_height(out->image);

    if (settings->mode) {
        if (settings->text == NULL || strlen(settings->text) == 0) {
            return TRUE;
        }

        PangoFontDescription *desc = pango_font_description_from_string(settings->font);
        GimpFont *font = find_font(desc);
        gdouble size = pango_font_description_get_size(desc) / (gdouble)PANGO_SCALE;
        if (!pango_font_description_get_size_is_absolute(desc)) {
            // points: to pixels at the resolution of the image
            gdouble xres, yres;
            gimp_image_get_resolution(out->image, &xres, &yres);
            size = size * yres / 72.0;
        }
        pango_font_description_free(desc);
        if (size <= 0) size = 16;

        gimp_text_get_extents_font(settings->text, size, font,
            &wmwidth, &wmheight, &wmasc, &wmdesc);

        calc_watermark_xy (
            imgwidth, imgheight,
            wmwidth, wmheight,
            settings->position,
            settings->edge_distance,
            &posX, &posY);

        GeglColor *color = rgba_to_gegl(&(settings->color));
        gimp_context_push();
        gimp_context_set_foreground(color);
        layer = gimp_text_font(
            out->image,
            NULL,
            posX,
            posY,
            settings->text,
            -1,
            TRUE,
            size,
            font
        );
        gimp_context_pop();
        g_object_unref(color);

        if (layer == NULL) return FALSE;
        gimp_layer_set_opacity(layer, settings->opacity);
    }
    else {
        if (settings->image_file == NULL || !g_file_test(settings->image_file, G_FILE_TEST_IS_REGULAR)) {
            // error, can't access image file
            g_printerr("BIMP: no watermark image %s\n", settings->image_file ? settings->image_file : "");
            return FALSE;
        }

        GFile *file = g_file_new_for_path(settings->image_file);
        layer = gimp_file_load_layer(GIMP_RUN_NONINTERACTIVE, out->image, file);
        g_object_unref(file);
        if (layer == NULL) return FALSE;

        gimp_image_insert_layer(out->image, layer, NULL, 0);

        wmwidth = gimp_drawable_get_width(GIMP_DRAWABLE(layer));
        wmheight = gimp_drawable_get_height(GIMP_DRAWABLE(layer));
        if (settings->image_sizemode != WM_IMG_NOSIZE) {
            if (settings->image_sizemode == WM_IMG_SIZEW) {
                float wmwidth_ = (imgwidth * settings->image_size_percent) / 100.0;
                float diff = (wmwidth_ / wmwidth) * 100;
                wmheight = round((wmheight * diff) / 100.0);
                wmwidth = round(wmwidth_);
            }
            else if (settings->image_sizemode == WM_IMG_SIZEH) {
                float wmheight_ = (imgheight * settings->image_size_percent) / 100.0;
                float diff = (wmheight_ / wmheight) * 100;
                wmwidth = round((wmwidth * diff) / 100.0);
                wmheight = round(wmheight_);
            }

            gimp_context_push();
            gimp_context_set_interpolation (GIMP_INTERPOLATION_CUBIC);
            success = gimp_layer_scale (layer, wmwidth, wmheight, TRUE);
            gimp_context_pop();
        }

        gimp_layer_set_opacity(layer, settings->opacity);

        calc_watermark_xy (
            imgwidth, imgheight,
            wmwidth, wmheight,
            settings->position,
            settings->edge_distance,
            &posX, &posY);

        gimp_layer_set_offsets(layer, posX, posY);
    }

    // refresh all drawables
    refresh_drawables(out);

    return success;
}

static void calc_watermark_xy (int imgwidth, int imgheight, int wmwidth, int wmheight, watermark_position position, int edge, gdouble* posX, gdouble* posY) {
    if (position == WM_POS_TL) {
        *posX = edge;
        *posY = edge;
    }
    else if (position == WM_POS_TC) {
        *posX = (imgwidth / 2) - (wmwidth / 2);
        *posY = edge;
    }
    else if (position == WM_POS_TR) {
        *posX = imgwidth - wmwidth - edge;
        *posY = edge;
    }
    else if (position == WM_POS_BL) {
        *posX = edge;
        *posY = imgheight - wmheight - edge;
    }
    else if (position == WM_POS_BC) {
        *posX = (imgwidth / 2) - (wmwidth / 2);
        *posY = imgheight - wmheight - edge;
    }
    else if (position == WM_POS_BR) {
        *posX = imgwidth - wmwidth - edge;
        *posY = imgheight - wmheight - edge;
    }
    else if (position == WM_POS_CL) {
        *posX = edge;
        *posY = (imgheight / 2) - (wmheight / 2);
    }
    else if (position == WM_POS_CR) {
        *posX = imgwidth - wmwidth - edge;
        *posY = (imgheight / 2) - (wmheight / 2);
    }
    else {
        *posX = (imgwidth / 2) - (wmwidth / 2);
        *posY = (imgheight / 2) - (wmheight / 2);
    }
}

/* Creates the config of a user-defined procedure with the user's settings.
 * Returns NULL if the procedure does not exist. */
GimpProcedureConfig* bimp_userdef_create_config(userdef_settings settings, GimpProcedure** procedure, gboolean* settings_ok)
{
    GimpProcedure *proc = gimp_pdb_lookup_procedure(gimp_get_pdb(), settings->procedure);
    GimpProcedureConfig *config;

    if (settings_ok) *settings_ok = TRUE;
    if (proc == NULL) return NULL;
    if (procedure) *procedure = proc;

    config = gimp_procedure_create_config(proc);
    if (settings->config != NULL) {
        GError *error = NULL;
        if (!gimp_config_deserialize_string(GIMP_CONFIG(config), settings->config, -1, NULL, &error)) {
            g_printerr("BIMP: settings of %s: %s\n", settings->procedure, error ? error->message : "?");
            g_clear_error(&error);
            if (settings_ok) *settings_ok = FALSE;
        }
    }

    return config;
}

/* Fills in the arguments that come from the batch: the run mode, the image
 * and its (merged) drawable. */
void bimp_userdef_set_image(GimpProcedure* proc, GimpProcedureConfig* config, GimpImage* image, GimpDrawable* drawable)
{
    GParamSpec **specs;
    gint n_specs, i;

    specs = gimp_procedure_get_arguments(proc, &n_specs);
    for (i = 0; i < n_specs; i++) {
        const gchar *name = g_param_spec_get_name(specs[i]);
        GType type = G_PARAM_SPEC_VALUE_TYPE(specs[i]);

        if (type == GIMP_TYPE_RUN_MODE) {
            g_object_set(config, name, GIMP_RUN_NONINTERACTIVE, NULL);
        }
        else if (type == GIMP_TYPE_IMAGE) {
            g_object_set(config, name, image, NULL);
        }
        else if (g_type_is_a(type, GIMP_TYPE_ITEM)) {
            g_object_set(config, name, drawable, NULL);
        }
        else if (type == GIMP_TYPE_CORE_OBJECT_ARRAY) {
            GimpDrawable *drawables[2] = { drawable, NULL };
            g_object_set(config, name, drawable ? drawables : NULL, NULL);
        }
    }
}

static gboolean apply_userdef(userdef_settings settings, image_output out)
{
    gboolean success = TRUE;
    GimpProcedure *proc = NULL;
    gboolean settings_ok;
    GimpProcedureConfig *config = bimp_userdef_create_config(settings, &proc, &settings_ok);

    if (config == NULL) {
        g_printerr("BIMP: GIMP has no procedure %s\n", settings->procedure);
        return FALSE;
    }
    if (!settings_ok) {
        // not with other settings than the user's
        g_object_unref(config);
        return FALSE;
    }

    GimpLayer *single_drawable = merge_layers(out->image);
    gimp_selection_none(out->image);

    bimp_userdef_set_image(proc, config, out->image, GIMP_DRAWABLE(single_drawable));

    GimpValueArray *return_vals = gimp_procedure_run_config(proc, config);
    if (return_vals == NULL || GIMP_VALUES_GET_ENUM(return_vals, 0) != GIMP_PDB_SUCCESS) {
        g_printerr("BIMP: %s failed\n", settings->procedure);
        success = FALSE;
    }
    if (return_vals) gimp_value_array_unref(return_vals);
    g_object_unref(config);

    merge_layers(out->image);
    refresh_drawables(out);

    return success;
}

static gboolean apply_rename(rename_settings settings, image_output out, char* orig_basename)
{
    gchar *name = g_strdup(settings->pattern), *replaced;

    // search for 'RENAME_KEY_ORIG' occurrences and replace the final filename
    if(strstr(name, RENAME_KEY_ORIG) != NULL) {
        replaced = str_replace(name, RENAME_KEY_ORIG, orig_basename);
        g_free(name);
        name = replaced;
    }

    // same thing for count and datetime

    if(strstr(name, RENAME_KEY_COUNT) != NULL)    {
        char strcount[16];
        g_snprintf(strcount, sizeof(strcount), "%i", processed_count + 1);
        replaced = str_replace(name, RENAME_KEY_COUNT, strcount);
        g_free(name);
        name = replaced;
    }

    if(strstr(name, RENAME_KEY_DATETIME) != NULL)    {
        replaced = str_replace(name, RENAME_KEY_DATETIME, current_datetime);
        g_free(name);
        name = replaced;
    }

    out->filename = name;

    return TRUE;
}

// following: saving the image file in various formats

/* Sets a property of an export config if this GIMP has it: argument names
 * differ between exporters and GIMP versions, and a missing one must not
 * stop the export. */
static void cfg_set(GimpProcedureConfig *config, const gchar *name, ...)
{
    GParamSpec *spec = g_object_class_find_property(G_OBJECT_GET_CLASS(config), name);
    va_list args;

    if (spec == NULL) {
        g_print("BIMP: this GIMP's exporter has no \"%s\" setting, skipped\n", name);
        return;
    }

    va_start(args, name);
    g_object_set_valist(G_OBJECT(config), name, args);
    va_end(args);
}

/* GIMP 3's exporters take the metadata to write as arguments, which are off
 * unless given: they are set as the user's preferences say (Image Import &
 * Export), like GIMP 2's exporters did. An exporter still leaves out what the
 * image does not have. */
static void set_metadata_defaults(GimpProcedureConfig *config)
{
    static const struct { const gchar *name; gboolean (*preference)(void); } props[] = {
        { "include-exif", gimp_export_exif },
        { "include-xmp", gimp_export_xmp },
        { "include-iptc", gimp_export_iptc },
        { "include-color-profile", gimp_export_color_profile },
        { "include-thumbnail", gimp_export_thumbnail },
        { "include-comment", gimp_export_comment },
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS(props); i++) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(config), props[i].name))
            g_object_set(config, props[i].name, props[i].preference(), NULL);
    }
}

/* the exporter for a file that keeps its format, if it has metadata options */
static const gchar* exporter_with_metadata(char *filename)
{
    if (file_has_extension(filename, ".jpg") || file_has_extension(filename, ".jpeg") || file_has_extension(filename, ".jpe"))
        return "file-jpeg-export";
    if (file_has_extension(filename, ".png"))
        return "file-png-export";
    if (file_has_extension(filename, ".tif") || file_has_extension(filename, ".tiff"))
        return "file-tiff-export";
    if (file_has_extension(filename, ".webp"))
        return "file-webp-export";
    if (file_has_extension(filename, ".avif"))
        return "file-heif-av1-export";
    return NULL;
}

static GimpProcedureConfig* export_config(const gchar *proc_name, GimpProcedure **proc, image_output out)
{
    GimpProcedureConfig *config;
    GFile *file;

    *proc = gimp_pdb_lookup_procedure(gimp_get_pdb(), proc_name);
    if (*proc == NULL) {
        g_printerr("BIMP: this GIMP has no %s\n", proc_name);
        return NULL;
    }

    file = g_file_new_for_path(out->filepath);
    config = gimp_procedure_create_config(*proc);
    g_object_set(config,
        "run-mode", GIMP_RUN_NONINTERACTIVE,
        "image", out->image,
        "file", file,
        NULL);
    g_object_unref(file);
    set_metadata_defaults(config);

    return config;
}

static gboolean export_run(GimpProcedure *proc, GimpProcedureConfig *config)
{
    GimpValueArray *return_vals = gimp_procedure_run_config(proc, config);
    gboolean success = return_vals != NULL && GIMP_VALUES_GET_ENUM(return_vals, 0) == GIMP_PDB_SUCCESS;

    if (!success && return_vals != NULL && gimp_value_array_length(return_vals) > 1 &&
        G_VALUE_HOLDS_STRING(gimp_value_array_index(return_vals, 1))) {
        g_printerr("BIMP: %s\n", GIMP_VALUES_GET_STRING(return_vals, 1));
    }
    if (return_vals) gimp_value_array_unref(return_vals);
    g_object_unref(config);

    return success;
}

/* GIMP 2 numbered choices; GIMP 3 names them */
static const gchar* jpeg_subsampling_name(int subsampling)
{
    switch (subsampling) {
        case 0: return "sub-sampling-2x2";  /* 4:2:0 */
        case 1: return "sub-sampling-2x1";  /* 4:2:2 horizontal */
        case 3: return "sub-sampling-1x2";  /* 4:2:2 vertical */
        default: return "sub-sampling-1x1"; /* 4:4:4 */
    }
}

static const gchar* jpeg_dct_name(int dct)
{
    switch (dct) {
        case 1: return "fixed";
        case 2: return "float";
        default: return "integer";
    }
}

static const gchar* tiff_compression_name(int compression)
{
    /* in the order of the window's list, which is GIMP 2's */
    static const gchar *names[] = { "none", "lzw", "packbits", "adobe_deflate", "jpeg", "ccittfax3", "ccittfax4" };
    return (compression >= 0 && compression < G_N_ELEMENTS(names)) ? names[compression] : "none";
}

static const gchar* webp_preset_name(int preset)
{
    static const gchar *names[] = { "default", "picture", "photo", "drawing", "icon", "text" };
    return (preset >= 0 && preset < G_N_ELEMENTS(names)) ? names[preset] : "default";
}

static void convert_to_indexed(image_output out)
{
    GimpLayer *layer = merge_layers(out->image);

    if (!gimp_drawable_is_indexed(GIMP_DRAWABLE(layer))) {
        // GIMP converts only 8-bit images to indexed
        if (gimp_image_get_precision(out->image) != GIMP_PRECISION_U8_NON_LINEAR)
            gimp_image_convert_precision(out->image, GIMP_PRECISION_U8_NON_LINEAR);
        gimp_image_convert_indexed(
            out->image,
            GIMP_CONVERT_DITHER_FS,
            GIMP_CONVERT_PALETTE_GENERATE,
            gimp_drawable_has_alpha(GIMP_DRAWABLE(layer)) ? 255 : 256,
            TRUE,
            FALSE,
            "" /* not used with a generated palette, but GIMP refuses NULL */
        );
    }
}

static gboolean save_webp(image_output out, format_params_webp p)
{
    GimpProcedure *proc;
    GimpProcedureConfig *config = export_config("file-webp-export", &proc, out);
    if (config == NULL) return FALSE;

    cfg_set(config, "preset", webp_preset_name(p->preset), NULL);
    cfg_set(config, "lossless", p->lossless, NULL);
    cfg_set(config, "quality", (gdouble)p->quality, NULL);
    cfg_set(config, "alpha-quality", (gdouble)p->alpha_quality, NULL);
    cfg_set(config, "animation", p->animation, NULL);
    cfg_set(config, "animation-loop", p->anim_loop, NULL);
    cfg_set(config, "minimize-size", p->minimize_size, NULL);
    cfg_set(config, "keyframe-distance", p->kf_distance, NULL);
    cfg_set(config, "include-exif", p->exif, NULL);
    cfg_set(config, "include-iptc", p->iptc, NULL);
    cfg_set(config, "include-xmp", p->xmp, NULL);
    cfg_set(config, "default-delay", p->delay, NULL);
    cfg_set(config, "force-delay", (gboolean)p->force_delay, NULL);

    return export_run(proc, config);
}

static gboolean save_heif(image_output out, const gchar *proc_name, int quality, gboolean lossless)
{
    GimpProcedure *proc;
    GimpProcedureConfig *config = export_config(proc_name, &proc, out);
    if (config == NULL) return FALSE;

    cfg_set(config, "quality", quality, NULL);
    cfg_set(config, "lossless", lossless, NULL);

    return export_run(proc, config);
}

static gboolean image_save(format_type type, image_output out, format_params params)
{
    GimpProcedure *proc = NULL;
    GimpProcedureConfig *config = NULL;

    merge_layers(out->image);

    if (type == FORMAT_BMP) {
        config = export_config("file-bmp-export", &proc, out);
    }
    else if(type == FORMAT_GIF) {
        convert_to_indexed(out);
        config = export_config("file-gif-export", &proc, out);
        if (config) {
            cfg_set(config, "interlace", ((format_params_gif)params)->interlace, NULL);
            cfg_set(config, "loop", TRUE, NULL);
        }
    }
    else if(type == FORMAT_ICON) {
        config = export_config("file-ico-export", &proc, out);
    }
    else if(type == FORMAT_JPEG) {
        format_params_jpeg p = params;
        GimpLayer *layer = merge_layers(out->image);

        // the JPEG exporter does not take indexed images
        if (gimp_drawable_is_indexed(GIMP_DRAWABLE(layer))) {
            gimp_image_convert_rgb(out->image);
        }
        if (p->comment != NULL && strlen(p->comment) > 0) {
            GimpParasite *parasite = gimp_parasite_new("gimp-comment", GIMP_PARASITE_PERSISTENT,
                                                       strlen(p->comment) + 1, p->comment);
            gimp_image_attach_parasite(out->image, parasite);
            gimp_parasite_free(parasite);
        }

        config = export_config("file-jpeg-export", &proc, out);
        if (config) {
            // quality below 3 does not change the file any further
            cfg_set(config, "quality", p->quality >= 3 ? p->quality / 100.0 : 0.03, NULL);
            cfg_set(config, "smoothing", (gdouble)p->smoothing, NULL);
            cfg_set(config, "optimize", p->entropy, NULL);
            cfg_set(config, "progressive", p->progressive, NULL);
            cfg_set(config, "sub-sampling", jpeg_subsampling_name(p->subsampling), NULL);
            cfg_set(config, "baseline", p->baseline, NULL);
            cfg_set(config, "restart", p->markers, NULL);
            cfg_set(config, "dct", jpeg_dct_name(p->dct), NULL);
            cfg_set(config, "include-comment", p->comment != NULL && strlen(p->comment) > 0, NULL);
        }
    }
    else if(type == FORMAT_PNG) {
        format_params_png p = params;
        config = export_config("file-png-export", &proc, out);
        if (config) {
            cfg_set(config, "interlaced", p->interlace, NULL);
            cfg_set(config, "compression", p->compression, NULL);
            cfg_set(config, "bkgd", p->savebgc, NULL);
            cfg_set(config, "offs", p->saveoff, NULL);
            cfg_set(config, "phys", p->savephys, NULL);
            cfg_set(config, "time", p->savetime, NULL);
            cfg_set(config, "include-comment", p->savecomm, NULL);
            cfg_set(config, "save-transparent", p->savetrans, NULL);
            /* GIMP 3 has no gAMA option: it writes a color profile instead */
        }
    }
    else if(type == FORMAT_TGA) {
        config = export_config("file-tga-export", &proc, out);
        if (config) {
            cfg_set(config, "rle", ((format_params_tga)params)->rle, NULL);
            cfg_set(config, "origin", ((format_params_tga)params)->origin == 1 ? "top-left" : "bottom-left", NULL);
        }
    }
    else if(type == FORMAT_TIFF) {
        config = export_config("file-tiff-export", &proc, out);
        if (config) {
            cfg_set(config, "compression", tiff_compression_name(((format_params_tiff)params)->compression), NULL);
        }
    }
    else if(type == FORMAT_HEIF) {
        return save_heif(out, "file-heif-export", ((format_params_heif)params)->quality, ((format_params_heif)params)->lossless);
    }
    else if(type == FORMAT_WEBP) {
        return save_webp(out, (format_params_webp)params);
    }
    else if(type == FORMAT_AVIF) {
        return save_heif(out, "file-heif-av1-export", ((format_params_avif)params)->quality, ((format_params_avif)params)->lossless);
    }
    else if(type == FORMAT_EXR) {
        config = export_config("file-exr-export", &proc, out);
    }
    else {
        // save in the original format
        // but first check if the images was a GIF and it's palette has changed during the process
        if (file_has_extension(out->filename, ".gif")) {
            convert_to_indexed(out);
        }

        // for HEIF, the default values are "0 quality"... save lossless instead
        if ((file_has_extension(out->filename, ".heif") || file_has_extension(out->filename, ".heic"))) {
            return save_heif(out, "file-heif-export", 100, TRUE);
        }
        else if (exporter_with_metadata(out->filename) != NULL) {
            // with the exporter's default settings, as gimp_file_save() would, and the metadata
            config = export_config(exporter_with_metadata(out->filename), &proc, out);
        }
        else {
            GFile *file = g_file_new_for_path(out->filepath);
            gboolean result = gimp_file_save(GIMP_RUN_NONINTERACTIVE, out->image, file, NULL);
            g_object_unref(file);
            return result;
        }
    }

    if (config == NULL) return FALSE;

    return export_run(proc, config);
}

/* returns a result code following this schema:
 * 0 = user responses "don't overwrite" to a confirm dialog
 * 1 = old file was the same as the new one and user responses "yes, overwrite"
 * 2 = old file wasn't the same (implicit overwrite) */
static int overwrite_result(char* path, GtkWidget* parent) {
    gboolean oldfile_access = g_file_test(path, G_FILE_TEST_IS_REGULAR);

    if ( (bimp_opt_alertoverwrite == BIMP_ASK_OVERWRITE) && oldfile_access && bimp_interactive) {
        GtkWidget *dialog;
        GtkWidget *check_alertoverwrite;

        dialog = gtk_message_dialog_new(
            GTK_WINDOW(parent),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_NONE,
            _("File %s already exists, overwrite it?"), comp_get_filename(path)
        );

        // Add checkbox "Always apply decision"
        check_alertoverwrite = gtk_check_button_new_with_label(_("Always apply this decision"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_alertoverwrite), FALSE);
        gtk_box_pack_start (GTK_BOX(gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog))), check_alertoverwrite, FALSE, FALSE, 0);
        gtk_widget_show (check_alertoverwrite);

        gtk_dialog_add_buttons (
            GTK_DIALOG(dialog),
            _("_Yes"), GTK_RESPONSE_YES,
            _("_No"), GTK_RESPONSE_NO, NULL
        );

        gtk_window_set_title(GTK_WINDOW(dialog), _("Overwrite?"));
        gint result = gtk_dialog_run(GTK_DIALOG(dialog));
        gboolean dont_ask_anymore = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_alertoverwrite));
        gtk_widget_destroy(dialog);

        if (result == GTK_RESPONSE_YES) {
            if (dont_ask_anymore)
                bimp_opt_alertoverwrite = BIMP_OVERWRITE_SKIP_ASK;
            return 1;
        }
        else {
            if (dont_ask_anymore)
                bimp_opt_alertoverwrite = BIMP_DONT_OVERWRITE_SKIP_ASK;

            return 0;
        }
    }
    else {
        if (oldfile_access) {
            return (bimp_opt_alertoverwrite == BIMP_OVERWRITE_SKIP_ASK) ? 1 : 0;
        }
        else {
            return 2;
        }
    }
}
