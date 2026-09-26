/*
 * Unit tests of the .bimp file format (src/bimp-serialize.c) and of the
 * curve file parser, without GIMP running: every manipulation is written
 * and read back, also in a locale with a decimal comma, and broken or old
 * files must load as expected or fail cleanly.
 *
 *   meson test -C <build folder>
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <string.h>
#include "../../src/bimp-manipulations.h"
#include "../../src/bimp-serialize.h"

static gchar *tmpdir;

static gchar* write_file(const gchar *name, const gchar *text, gssize len)
{
    gchar *path = g_build_filename(tmpdir, name, NULL);
    g_assert_true(g_file_set_contents(path, text, len, NULL));
    return path;
}

static void clear_list(void)
{
    g_slist_free(bimp_selected_manipulations);
    bimp_selected_manipulations = NULL;
}

/* loads a set given as text; returns what bimp_deserialize_from_file said */
static gboolean load_text(const gchar *text)
{
    gchar *path = write_file("set.bimp", text, -1);
    gboolean ok;

    clear_list();
    ok = bimp_deserialize_from_file(path);
    g_free(path);
    return ok;
}

static manipulation nth(guint n)
{
    return g_slist_nth_data(bimp_selected_manipulations, n);
}

static void assert_rgba(const GdkRGBA *a, const GdkRGBA *b)
{
    g_assert_cmpfloat_with_epsilon(a->red, b->red, 1.0 / 65535);
    g_assert_cmpfloat_with_epsilon(a->green, b->green, 1.0 / 65535);
    g_assert_cmpfloat_with_epsilon(a->blue, b->blue, 1.0 / 65535);
}

/* a set with every manipulation, none of them at its defaults */
static void build_full_set(format_type format)
{
    manipulation m;

    clear_list();

    m = bimp_append_manipulation(MANIP_RESIZE);
    {
        resize_settings s = m->settings;
        s->new_w_pc = 33.25;
        s->new_h_pc = 12.5;
        s->new_w_px = 1234;
        s->new_h_px = 567;
        s->resize_mode_width = RESIZE_PIXEL;
        s->resize_mode_height = RESIZE_DISABLE;
        s->stretch_mode = STRETCH_PADDED;
        s->padding_color = (GdkRGBA) { 0.25, 0.5, 0.75, 0.5 };
        s->interpolation = GIMP_INTERPOLATION_LINEAR;
        s->change_res = TRUE;
        s->new_res_x = 300.5;
        s->new_res_y = 150.25;
    }
    m = bimp_append_manipulation(MANIP_CROP);
    {
        crop_settings s = m->settings;
        s->new_w = 99;
        s->new_h = 77;
        s->manual = TRUE;
        s->ratio = CROP_PRESET_CUSTOM;
        s->custom_ratio1 = 2.5;
        s->custom_ratio2 = 1.25;
        s->start_pos = CROP_START_BR;
    }
    m = bimp_append_manipulation(MANIP_FLIPROTATE);
    {
        fliprotate_settings s = m->settings;
        s->flip_h = TRUE;
        s->flip_v = TRUE;
        s->rotate = TRUE;
        s->rotation_type = GIMP_ROTATE_DEGREES270;
    }
    m = bimp_append_manipulation(MANIP_COLOR);
    {
        color_settings s = m->settings;
        s->brightness = 0.125;
        s->contrast = -0.375;
        s->levels_auto = TRUE;
        s->grayscale = TRUE;
        s->curve_file = g_strdup("/tmp/some folder/curve ä.txt");
    }
    m = bimp_append_manipulation(MANIP_SHARPBLUR);
    ((sharpblur_settings)m->settings)->amount = -42;
    m = bimp_append_manipulation(MANIP_WATERMARK);
    {
        watermark_settings s = m->settings;
        s->mode = TRUE;
        s->text = g_strdup(" (c) 2026; \"BIMP\" #1 = ok\\n\nsecond line åäö ");
        s->font = g_strdup("DejaVu Sans Bold 21px");
        s->color = (GdkRGBA) { 1.0, 0.0, 0.5, 1.0 };
        s->image_sizemode = WM_IMG_SIZEH;
        s->image_size_percent = 12.5;
        s->opacity = 42;
        s->edge_distance = 7;
        s->position = WM_POS_TC;
    }
    m = bimp_append_manipulation(MANIP_WATERMARK);
    {
        watermark_settings s = m->settings;
        s->mode = FALSE;
        s->image_file = g_strdup("/home/me/logo 1.png");
        s->position = WM_POS_CL;
    }
    m = bimp_append_manipulation(MANIP_CHANGEFORMAT);
    {
        changeformat_settings s = m->settings;
        s->format = format;
        s->params = format_params_new(format);
        if (format == FORMAT_GIF) {
            ((format_params_gif)s->params)->interlace = TRUE;
        }
        else if (format == FORMAT_JPEG) {
            format_params_jpeg p = s->params;
            p->quality = 72.5;
            p->smoothing = 0.25;
            p->entropy = FALSE;
            p->progressive = TRUE;
            p->comment = g_strdup("made by; BIMP\n# not a comment");
            p->subsampling = 3;
            p->baseline = FALSE;
            p->markers = 4;
            p->dct = 2;
        }
        else if (format == FORMAT_PNG) {
            format_params_png p = s->params;
            p->interlace = TRUE;
            p->compression = 3;
            p->savebgc = FALSE;
            p->savegamma = TRUE;
            p->saveoff = TRUE;
            p->savephys = FALSE;
            p->savetime = FALSE;
            p->savecomm = TRUE;
            p->savetrans = TRUE;
        }
        else if (format == FORMAT_TGA) {
            ((format_params_tga)s->params)->rle = FALSE;
            ((format_params_tga)s->params)->origin = 1;
        }
        else if (format == FORMAT_TIFF) {
            ((format_params_tiff)s->params)->compression = 6;
        }
        else if (format == FORMAT_HEIF) {
            ((format_params_heif)s->params)->lossless = TRUE;
            ((format_params_heif)s->params)->quality = 77;
        }
        else if (format == FORMAT_WEBP) {
            format_params_webp p = s->params;
            p->preset = 4;
            p->lossless = TRUE;
            p->quality = 66.5;
            p->alpha_quality = 55.5;
            p->animation = TRUE;
            p->anim_loop = FALSE;
            p->minimize_size = FALSE;
            p->kf_distance = 12;
            p->exif = TRUE;
            p->iptc = TRUE;
            p->xmp = TRUE;
            p->delay = 123;
            p->force_delay = TRUE;
        }
        else if (format == FORMAT_AVIF) {
            ((format_params_avif)s->params)->lossless = TRUE;
            ((format_params_avif)s->params)->quality = 33;
        }
    }
    m = bimp_append_manipulation(MANIP_RENAME);
    g_free(((rename_settings)m->settings)->pattern);
    ((rename_settings)m->settings)->pattern = g_strdup("$$ copy ##; @@ å");
    m = bimp_append_manipulation(MANIP_USERDEF);
    ((userdef_settings)m->settings)->procedure = g_strdup("gimp-drawable-posterize");
    ((userdef_settings)m->settings)->config = g_strdup("(levels 5)\n(name \"a \\\"quoted\\\" ; value\")\n");
    m = bimp_append_manipulation(MANIP_USERDEF);
    ((userdef_settings)m->settings)->procedure = g_strdup("gimp-drawable-invert");
}

static void check_full_set(format_type format)
{
    GSList *expected = bimp_selected_manipulations;
    GSList *got, *e, *g;
    gchar *path = g_build_filename(tmpdir, "full.bimp", NULL);

    g_assert_true(bimp_serialize_to_file(path));
    bimp_selected_manipulations = NULL;
    g_assert_true(bimp_deserialize_from_file(path));
    got = bimp_selected_manipulations;
    g_free(path);

    g_assert_cmpuint(g_slist_length(got), ==, g_slist_length(expected));
    for (e = expected, g = got; e != NULL; e = e->next, g = g->next) {
        manipulation me = e->data, mg = g->data;
        g_assert_cmpint(me->type, ==, mg->type);

        if (me->type == MANIP_RESIZE) {
            resize_settings a = me->settings, b = mg->settings;
            g_assert_cmpfloat(a->new_w_pc, ==, b->new_w_pc);
            g_assert_cmpfloat(a->new_h_pc, ==, b->new_h_pc);
            g_assert_cmpint(a->new_w_px, ==, b->new_w_px);
            g_assert_cmpint(a->new_h_px, ==, b->new_h_px);
            g_assert_cmpint(a->resize_mode_width, ==, b->resize_mode_width);
            g_assert_cmpint(a->resize_mode_height, ==, b->resize_mode_height);
            g_assert_cmpint(a->stretch_mode, ==, b->stretch_mode);
            assert_rgba(&a->padding_color, &b->padding_color);
            g_assert_cmpfloat_with_epsilon(a->padding_color.alpha, b->padding_color.alpha, 1.0 / 65535);
            g_assert_cmpint(a->interpolation, ==, b->interpolation);
            g_assert_cmpint(a->change_res, ==, b->change_res);
            g_assert_cmpfloat(a->new_res_x, ==, b->new_res_x);
            g_assert_cmpfloat(a->new_res_y, ==, b->new_res_y);
        }
        else if (me->type == MANIP_CROP) {
            crop_settings a = me->settings, b = mg->settings;
            g_assert_cmpint(a->new_w, ==, b->new_w);
            g_assert_cmpint(a->new_h, ==, b->new_h);
            g_assert_cmpint(a->manual, ==, b->manual);
            g_assert_cmpint(a->ratio, ==, b->ratio);
            g_assert_cmpfloat(a->custom_ratio1, ==, b->custom_ratio1);
            g_assert_cmpfloat(a->custom_ratio2, ==, b->custom_ratio2);
            g_assert_cmpint(a->start_pos, ==, b->start_pos);
        }
        else if (me->type == MANIP_FLIPROTATE) {
            fliprotate_settings a = me->settings, b = mg->settings;
            g_assert_cmpint(a->flip_h, ==, b->flip_h);
            g_assert_cmpint(a->flip_v, ==, b->flip_v);
            g_assert_cmpint(a->rotate, ==, b->rotate);
            g_assert_cmpint(a->rotation_type, ==, b->rotation_type);
        }
        else if (me->type == MANIP_COLOR) {
            color_settings a = me->settings, b = mg->settings;
            g_assert_cmpfloat(a->brightness, ==, b->brightness);
            g_assert_cmpfloat(a->contrast, ==, b->contrast);
            g_assert_cmpint(a->levels_auto, ==, b->levels_auto);
            g_assert_cmpint(a->grayscale, ==, b->grayscale);
            g_assert_cmpstr(a->curve_file, ==, b->curve_file);
        }
        else if (me->type == MANIP_SHARPBLUR) {
            g_assert_cmpint(((sharpblur_settings)me->settings)->amount, ==, ((sharpblur_settings)mg->settings)->amount);
        }
        else if (me->type == MANIP_WATERMARK) {
            watermark_settings a = me->settings, b = mg->settings;
            g_assert_cmpint(a->mode, ==, b->mode);
            g_assert_cmpstr(a->text, ==, b->text);
            g_assert_cmpstr(a->font, ==, b->font);
            assert_rgba(&a->color, &b->color);
            g_assert_cmpstr(a->image_file, ==, b->image_file);
            g_assert_cmpint(a->image_sizemode, ==, b->image_sizemode);
            g_assert_cmpfloat(a->image_size_percent, ==, b->image_size_percent);
            g_assert_cmpfloat(a->opacity, ==, b->opacity);
            g_assert_cmpint(a->edge_distance, ==, b->edge_distance);
            g_assert_cmpint(a->position, ==, b->position);
        }
        else if (me->type == MANIP_CHANGEFORMAT) {
            changeformat_settings a = me->settings, b = mg->settings;
            g_assert_cmpint(a->format, ==, b->format);
            g_assert_cmpint(b->format, ==, format);
            if (format == FORMAT_GIF) {
                g_assert_cmpint(((format_params_gif)a->params)->interlace, ==, ((format_params_gif)b->params)->interlace);
            }
            else if (format == FORMAT_JPEG) {
                format_params_jpeg pa = a->params, pb = b->params;
                g_assert_cmpfloat(pa->quality, ==, pb->quality);
                g_assert_cmpfloat(pa->smoothing, ==, pb->smoothing);
                g_assert_cmpint(pa->entropy, ==, pb->entropy);
                g_assert_cmpint(pa->progressive, ==, pb->progressive);
                g_assert_cmpstr(pa->comment, ==, pb->comment);
                g_assert_cmpint(pa->subsampling, ==, pb->subsampling);
                g_assert_cmpint(pa->baseline, ==, pb->baseline);
                g_assert_cmpint(pa->markers, ==, pb->markers);
                g_assert_cmpint(pa->dct, ==, pb->dct);
            }
            else if (format == FORMAT_PNG) {
                g_assert_cmpmem(a->params, sizeof(struct changeformat_params_png), b->params, sizeof(struct changeformat_params_png));
            }
            else if (format == FORMAT_TGA) {
                g_assert_cmpmem(a->params, sizeof(struct changeformat_params_tga), b->params, sizeof(struct changeformat_params_tga));
            }
            else if (format == FORMAT_TIFF) {
                g_assert_cmpmem(a->params, sizeof(struct changeformat_params_tiff), b->params, sizeof(struct changeformat_params_tiff));
            }
            else if (format == FORMAT_HEIF) {
                g_assert_cmpmem(a->params, sizeof(struct changeformat_params_heif), b->params, sizeof(struct changeformat_params_heif));
            }
            else if (format == FORMAT_WEBP) {
                g_assert_cmpmem(a->params, sizeof(struct changeformat_params_webp), b->params, sizeof(struct changeformat_params_webp));
            }
            else if (format == FORMAT_AVIF) {
                g_assert_cmpmem(a->params, sizeof(struct changeformat_params_avif), b->params, sizeof(struct changeformat_params_avif));
            }
        }
        else if (me->type == MANIP_RENAME) {
            g_assert_cmpstr(((rename_settings)me->settings)->pattern, ==, ((rename_settings)mg->settings)->pattern);
        }
        else if (me->type == MANIP_USERDEF) {
            userdef_settings a = me->settings, b = mg->settings;
            g_assert_cmpstr(a->procedure, ==, b->procedure);
            g_assert_cmpstr(a->config, ==, b->config);
        }
    }

    g_slist_free(expected);
}

static void test_roundtrip(void)
{
    int format;

    for (format = 0; format < FORMAT_END; format++) {
        build_full_set(format);
        check_full_set(format);
    }
}

/* numbers must be written and read with a decimal point in any locale */
static void test_roundtrip_decimal_comma(void)
{
    const gchar *locales[] = { "sv_SE.UTF-8", "de_DE.UTF-8", "en_DK.UTF-8", NULL };
    gchar *old = g_strdup(setlocale(LC_ALL, NULL));
    gchar *text = NULL;
    const gchar *found = NULL;
    int i;

    for (i = 0; locales[i] != NULL && found == NULL; i++) {
        if (setlocale(LC_ALL, locales[i]) != NULL && strcmp(localeconv()->decimal_point, ",") == 0)
            found = locales[i];
    }
    if (found == NULL) {
        setlocale(LC_ALL, old);
        g_free(old);
        g_test_skip("no locale with a decimal comma installed");
        return;
    }
    g_test_message("locale %s", found);

    build_full_set(FORMAT_WEBP);
    check_full_set(FORMAT_WEBP);

    /* and the file has decimal points */
    build_full_set(FORMAT_JPEG);
    {
        gchar *path = g_build_filename(tmpdir, "comma.bimp", NULL);
        g_assert_true(bimp_serialize_to_file(path));
        g_assert_true(g_file_get_contents(path, &text, NULL, NULL));
        g_free(path);
    }
    g_assert_nonnull(strstr(text, "new_w_pc=33.25"));
    g_assert_nonnull(strstr(text, "quality=72.5"));
    g_free(text);

    /* a file written by hand or by BIMP in the C locale */
    g_assert_true(load_text("#BIMP 3.0\n[CROP]\ncustom_ratio1=2.5\ncustom_ratio2=1.25\nratio=9\n"));
    g_assert_cmpfloat(((crop_settings)nth(0)->settings)->custom_ratio1, ==, 2.5);

    setlocale(LC_ALL, old);
    g_free(old);
}

static void test_header_version(void)
{
    /* BIMP 1 had brightness and contrast in [-127, 127], mapped to [-0.5, 0.5] */
    g_assert_true(load_text("#BIMP 1.18\n#MANIPULATION SET DEFINITION\n[COLOR]\nbrightness=127\ncontrast=-127\n"));
    g_assert_cmpfloat_with_epsilon(((color_settings)nth(0)->settings)->brightness, 0.5, 1e-9);
    g_assert_cmpfloat_with_epsilon(((color_settings)nth(0)->settings)->contrast, -0.5, 1e-9);

    g_assert_true(load_text("#BIMP 2.6\n#MANIPULATION SET DEFINITION\n[COLOR]\nbrightness=0.25\n"));
    g_assert_cmpfloat(((color_settings)nth(0)->settings)->brightness, ==, 0.25);

    /* written by hand */
    g_assert_true(load_text("# BIMP 3.0\n[COLOR]\nbrightness=0.25\n"));
    g_assert_cmpfloat(((color_settings)nth(0)->settings)->brightness, ==, 0.25);
    g_assert_true(load_text("#BIMP 3.0.0-dev\n#MANIPULATION SET DEFINITION\n[COLOR]\nbrightness=0.25\n"));
    g_assert_cmpfloat(((color_settings)nth(0)->settings)->brightness, ==, 0.25);
}

/* every BIMP since 1.0 writes the header; a file without one was written
 * by hand (or by a script) for the current format */
static void test_no_header(void)
{
    g_assert_true(load_text("[COLOR]\nbrightness=0.25\n"));
    g_assert_cmpuint(g_slist_length(bimp_selected_manipulations), ==, 1);
    g_assert_cmpfloat(((color_settings)nth(0)->settings)->brightness, ==, 0.25);

    g_assert_true(load_text("\n\n[RESIZE]\nnew_w_pc=50\n"));
    g_assert_true(load_text("# just a note\n[RESIZE]\nnew_w_pc=50\n"));
}

static void test_broken_files(void)
{
    const gchar garbage[] = "\x89PNG\r\n\x1a\n\0\0\0\rIHDR\xff\xfe [RESIZE";
    gchar *path;

    g_assert_false(load_text(""));
    g_assert_false(load_text("#BIMP 3.0\n"));
    g_assert_false(load_text("#BIMP 3.0\n[RESIZE"));
    g_assert_false(load_text("#BIMP 3.0\n[RESIZE]\nnew_w_p"));
    g_assert_false(load_text("#BIMP 3.0\nnew_w_pc=50\n"));

    path = write_file("garbage.bimp", garbage, sizeof(garbage));
    clear_list();
    g_assert_false(bimp_deserialize_from_file(path));
    g_free(path);

    clear_list();
    g_assert_false(bimp_deserialize_from_file("/nonexistent/folder/set.bimp"));

    /* a file cut after a complete line is still a valid set */
    g_assert_true(load_text("#BIMP 3.0\n[RESIZE]\nnew_w_pc=50"));
    g_assert_cmpfloat(((resize_settings)nth(0)->settings)->new_w_pc, ==, 50);

    /* values of the wrong type read as 0, the set still loads */
    g_assert_true(load_text("#BIMP 3.0\n[RESIZE]\nnew_w_pc=abc\nnew_w_px=1.5\nchange_res=maybe\n"));
}

/* a group BIMP does not know (from a newer BIMP, or a typo) is skipped, and
 * must not repeat the manipulation before it */
static void test_unknown_groups(void)
{
    g_assert_true(load_text("#BIMP 3.0\n[RESIZE]\nnew_w_pc=50\n[FUTURE]\nx=1\n[USERDEFX]\n[USERDEF0]\nconfig=\n[WATERMARKX]\n[FLIPROTATE]\nflip_h=true\n[OTHER]\n"));
    g_assert_cmpuint(g_slist_length(bimp_selected_manipulations), ==, 2);
    g_assert_cmpint(nth(0)->type, ==, MANIP_RESIZE);
    g_assert_cmpint(nth(1)->type, ==, MANIP_FLIPROTATE);

    /* only unknown groups: nothing to do */
    g_assert_false(load_text("#BIMP 3.0\n[FUTURE]\nx=1\n"));
}

/* values that would index past BIMP's tables or divide by zero */
static void test_invalid_values(void)
{
    g_assert_false(load_text("#BIMP 3.0\n[CHANGEFORMAT]\nformat=99\n"));
    g_assert_false(load_text("#BIMP 3.0\n[CHANGEFORMAT]\nformat=-1\n"));
    g_assert_false(load_text("#BIMP 3.0\n[CROP]\nratio=10\n"));
    g_assert_false(load_text("#BIMP 3.0\n[CROP]\nratio=-3\n"));
    g_assert_false(load_text("#BIMP 3.0\n[CROP]\nratio=9\ncustom_ratio1=1\ncustom_ratio2=0\n"));
    g_assert_false(load_text("#BIMP 3.0\n[CROP]\nratio=9\ncustom_ratio1=-1\ncustom_ratio2=1\n"));

    /* the last valid ones */
    g_assert_true(load_text("#BIMP 3.0\n[CHANGEFORMAT]\nformat=10\n"));
    g_assert_cmpint(((changeformat_settings)nth(0)->settings)->format, ==, FORMAT_EXR);
    g_assert_true(load_text("#BIMP 3.0\n[CROP]\nratio=8\n"));
    /* a manual crop does not use the ratio */
    g_assert_true(load_text("#BIMP 3.0\n[CROP]\nmanual=true\nnew_w=10\nnew_h=10\nratio=9\ncustom_ratio2=0\n"));
}

/* a format without options in the file gets the defaults of its exporter */
static void test_changeformat_defaults(void)
{
    g_assert_true(load_text("#BIMP 3.0\n[CHANGEFORMAT]\nformat=3\n"));
    format_params_jpeg p = ((changeformat_settings)nth(0)->settings)->params;
    g_assert_nonnull(p);
    g_assert_cmpfloat(p->quality, ==, 85.0);
    g_assert_cmpstr(p->comment, ==, "");

    g_assert_true(load_text("#BIMP 3.0\n[CHANGEFORMAT]\nformat=8\nquality=10\n"));
    format_params_webp w = ((changeformat_settings)nth(0)->settings)->params;
    g_assert_cmpfloat(w->quality, ==, 10);
    g_assert_cmpfloat(w->alpha_quality, ==, 100);
    g_assert_cmpint(w->kf_distance, ==, 50);

    /* a CHANGEFORMAT without format keeps the default (JPEG) with its options */
    g_assert_true(load_text("#BIMP 3.0\n[CHANGEFORMAT]\n"));
    g_assert_cmpint(((changeformat_settings)nth(0)->settings)->format, ==, FORMAT_JPEG);
    g_assert_nonnull(((changeformat_settings)nth(0)->settings)->params);
}

static void test_bimp2_file(void)
{
    g_assert_true(load_text(
        "#BIMP 2.6\n#MANIPULATION SET DEFINITION\n"
        "[RESIZE]\nresize_mode=2\nnew_w_px=320\npadding_color=#ffff00000000\npadding_color_alpha=32768\n"
        "[WATERMARK]\nmode=true\ntext=old\n"
        "[USERDEF0]\nprocedure=plug-in-gauss\nnum_params=6\nPARAM0=NOT_USED\nPARAM3=5\n"
        "[USERDEF1]\nnum_params=1\n"));
    g_assert_cmpuint(g_slist_length(bimp_selected_manipulations), ==, 3);

    resize_settings r = nth(0)->settings;
    g_assert_cmpint(r->resize_mode_width, ==, RESIZE_PIXEL);
    g_assert_cmpint(r->resize_mode_height, ==, RESIZE_DISABLE);
    g_assert_cmpfloat(r->padding_color.red, ==, 1.0);
    g_assert_cmpfloat_with_epsilon(r->padding_color.alpha, 0.5, 0.001);

    g_assert_cmpstr(((watermark_settings)nth(1)->settings)->text, ==, "old");

    userdef_settings u = nth(2)->settings;
    g_assert_cmpstr(u->procedure, ==, "plug-in-gauss");
    g_assert_null(u->config);
}

/* curve files: GIMP writes them with "Export Current Settings to File" */
#define CURVE_CHANNEL(name, points) \
    "(channel " name ")\n" \
    "(curve\n" \
    "    (curve-type smooth)\n" \
    "    (n-points 3)\n" \
    "    (points " points ")\n" \
    "    (point-types 3 smooth smooth smooth)\n" \
    "    (n-samples 256)\n" \
    "    (samples 256 0 0.1 0.2))\n"

static const gchar curve_file[] =
    "# GIMP curves tool settings\n\n"
    "(time 0)\n"
    "(trc non-linear)\n"
    CURVE_CHANNEL("value", "6 0 0 0.5 0.25 1 1")
    CURVE_CHANNEL("red", "4 0 0.125 1 1")
    CURVE_CHANNEL("green", "4 0 0 1 1")
    CURVE_CHANNEL("blue", "4 0 0 1 1")
    CURVE_CHANNEL("alpha", "4 0 0 1 1")
    "\n# end of curves tool settings\n";

static gboolean parse_curve(const gchar *text, int *nv, gdouble **pv, int *nr, gdouble **pr)
{
    gchar *path = write_file("curve.txt", text, -1);
    int ng = 0, nb = 0, na = 0;
    gdouble *pg = NULL, *pb = NULL, *pa = NULL;
    gboolean ok;

    *nv = *nr = 0;
    *pv = *pr = NULL;
    ok = parse_curve_file(path, nv, pv, nr, pr, &ng, &pg, &nb, &pb, &na, &pa);
    g_free(pg);
    g_free(pb);
    g_free(pa);
    g_free(path);
    return ok;
}

static void check_curve_values(void)
{
    int nv, nr;
    gdouble *pv, *pr;

    g_assert_true(parse_curve(curve_file, &nv, &pv, &nr, &pr));
    g_assert_cmpint(nv, ==, 6);
    g_assert_cmpfloat(pv[2], ==, 0.5);
    g_assert_cmpfloat(pv[3], ==, 0.25);
    g_assert_cmpint(nr, ==, 4);
    g_assert_cmpfloat(pr[1], ==, 0.125);
    g_free(pv);
    g_free(pr);
}

static void test_curve_file(void)
{
    check_curve_values();
}

/* numbers in curve files have a decimal point in any locale, and parsing
 * one must leave the locale of the plug-in as it was */
static void test_curve_file_decimal_comma(void)
{
    const gchar *locales[] = { "sv_SE.UTF-8", "de_DE.UTF-8", "en_DK.UTF-8", NULL };
    gchar *old = g_strdup(setlocale(LC_ALL, NULL));
    const gchar *found = NULL;
    int i;

    for (i = 0; locales[i] != NULL && found == NULL; i++) {
        if (setlocale(LC_ALL, locales[i]) != NULL && strcmp(localeconv()->decimal_point, ",") == 0)
            found = locales[i];
    }
    if (found == NULL) {
        setlocale(LC_ALL, old);
        g_free(old);
        g_test_skip("no locale with a decimal comma installed");
        return;
    }

    /* the plug-in's own locale for numbers, which parsing must not change */
    setlocale(LC_NUMERIC, "C");
    check_curve_values();
    g_assert_cmpstr(setlocale(LC_NUMERIC, NULL), ==, "C");

    setlocale(LC_NUMERIC, found);
    check_curve_values();
    g_assert_cmpstr(setlocale(LC_NUMERIC, NULL), ==, found);

    setlocale(LC_ALL, old);
    g_free(old);
}

static void test_curve_file_broken(void)
{
    int nv, nr;
    gdouble *pv, *pr;
    gchar *long_line = g_strnfill(5000, '1');
    gchar *text;

    g_assert_false(parse_curve("", &nv, &pv, &nr, &pr));
    g_assert_false(parse_curve("not a curve\n", &nv, &pv, &nr, &pr));
    g_assert_false(parse_curve("# GIMP curves tool settings\n(time 0)\n", &nv, &pv, &nr, &pr));
    /* a channel name longer than any GIMP writes */
    g_assert_false(parse_curve("# GIMP curves tool settings\n(channel valuevaluevaluevaluevaluevalue)\n(curve\n"
                               "    (points 4 0 0 1 1)\n# end\n", &nv, &pv, &nr, &pr));
    /* no points line where it should be */
    g_assert_false(parse_curve("# GIMP curves tool settings\n(channel value)\n(curve\n    (curve-type smooth)\n", &nv, &pv, &nr, &pr));
    g_assert_false(parse_curve("# GIMP curves tool settings\n(channel value)\n(curve\n    (curve-type smooth)\n"
                               "    (n-points 3)\n)\n# end\n", &nv, &pv, &nr, &pr));
    /* an odd number of coordinates */
    g_assert_false(parse_curve("# GIMP curves tool settings\n(channel value)\n(curve\n    (points 3 0 0 1)\n# end\n",
                               &nv, &pv, &nr, &pr));
    /* very long lines */
    text = g_strconcat("# GIMP curves tool settings\n(channel value)\n(curve\n    (points ", long_line, ")\n# end\n", NULL);
    parse_curve(text, &nv, &pv, &nr, &pr);
    g_free(pv);
    g_free(pr);
    g_free(text);
    g_free(long_line);
}

int main(int argc, char **argv)
{
    int result;

    g_test_init(&argc, &argv, NULL);

    tmpdir = g_dir_make_tmp("bimp-test-XXXXXX", NULL);
    g_assert_nonnull(tmpdir);

    g_test_add_func("/serialize/roundtrip", test_roundtrip);
    g_test_add_func("/serialize/roundtrip-decimal-comma", test_roundtrip_decimal_comma);
    g_test_add_func("/serialize/header-version", test_header_version);
    g_test_add_func("/serialize/no-header", test_no_header);
    g_test_add_func("/serialize/broken-files", test_broken_files);
    g_test_add_func("/serialize/unknown-groups", test_unknown_groups);
    g_test_add_func("/serialize/invalid-values", test_invalid_values);
    g_test_add_func("/serialize/changeformat-defaults", test_changeformat_defaults);
    g_test_add_func("/serialize/bimp2-file", test_bimp2_file);
    g_test_add_func("/curve/file", test_curve_file);
    g_test_add_func("/curve/decimal-comma", test_curve_file_decimal_comma);
    g_test_add_func("/curve/broken", test_curve_file_broken);

    result = g_test_run();

    {
        GDir *dir = g_dir_open(tmpdir, 0, NULL);
        const gchar *name;
        while (dir && (name = g_dir_read_name(dir)) != NULL) {
            gchar *path = g_build_filename(tmpdir, name, NULL);
            g_unlink(path);
            g_free(path);
        }
        if (dir) g_dir_close(dir);
        g_rmdir(tmpdir);
    }

    return result;
}
