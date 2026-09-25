#!/usr/bin/env python3
# One-off helper used for the GIMP 3 port: rewrites the mechanical GTK 2
# calls of BIMP's windows into GTK 3 (boxes, scales, combo boxes, tables to
# grids, alignments, stock buttons, dialog areas). What it cannot do was
# ported by hand. Kept for reference.
import re
import sys


def split_args(s):
    """Splits a C argument list at top-level commas."""
    args, depth, cur = [], 0, ''
    for ch in s:
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        if ch == ',' and depth == 0:
            args.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        args.append(cur.strip())
    return args


def replace_calls(src, name, fn):
    """Replaces every call name(...) with fn(args)."""
    out, i = '', 0
    pat = re.compile(r'\b' + re.escape(name) + r'\s*\(')
    while True:
        m = pat.search(src, i)
        if not m:
            return out + src[i:]
        j, depth = m.end(), 1
        while depth:
            if src[j] == '(':
                depth += 1
            elif src[j] == ')':
                depth -= 1
            j += 1
        out += src[i:m.start()] + fn(split_args(src[m.end():j - 1]))
        i = j


def unwrap(a, macro):
    m = re.fullmatch(macro + r'\((.*)\)', a)
    return m.group(1) if m else a


STOCK = {
    'GTK_STOCK_OK': '_("_OK")', 'GTK_STOCK_CANCEL': '_("_Cancel")',
    'GTK_STOCK_CLOSE': '_("_Close")', 'GTK_STOCK_APPLY': '_("_Apply")',
    'GTK_STOCK_STOP': '_("_Stop")', 'GTK_STOCK_ABOUT': '_("_About")',
    'GTK_STOCK_ADD': '_("_Add")', 'GTK_STOCK_SAVE': '_("_Save")',
    'GTK_STOCK_OPEN': '_("_Open")', 'GTK_STOCK_YES': '_("_Yes")',
    'GTK_STOCK_NO': '_("_No")',
}


def convert(s):
    s = re.sub(r'gtk_vbox_new\s*\(\s*(TRUE|FALSE)\s*,', r'gtk_box_new(GTK_ORIENTATION_VERTICAL,', s)
    s = re.sub(r'gtk_hbox_new\s*\(\s*(TRUE|FALSE)\s*,', r'gtk_box_new(GTK_ORIENTATION_HORIZONTAL,', s)
    s = s.replace('gtk_hseparator_new()', 'gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)')
    s = s.replace('gtk_vseparator_new()', 'gtk_separator_new(GTK_ORIENTATION_VERTICAL)')
    s = s.replace('gtk_hscale_new_with_range(', 'gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, ')
    s = s.replace('gtk_combo_box_new_text()', 'gtk_combo_box_text_new()')
    s = replace_calls(s, 'gtk_combo_box_append_text', lambda a:
        'gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(%s), %s)' % (unwrap(a[0], 'GTK_COMBO_BOX'), a[1]))
    s = re.sub(r'GTK_DIALOG\s*\(([^()]*)\)->vbox', r'gtk_dialog_get_content_area(GTK_DIALOG(\1))', s)

    def misc(a):
        w = unwrap(a[0], 'GTK_MISC')
        return 'gtk_label_set_xalign(GTK_LABEL(%s), %s)' % (w, a[1])
    s = replace_calls(s, 'gtk_misc_set_alignment', misc)

    s = replace_calls(s, 'gtk_table_new', lambda a: 'bimp_grid_new(0, 0)')
    s = replace_calls(s, 'gtk_table_set_row_spacings', lambda a:
        'gtk_grid_set_row_spacing(GTK_GRID(%s), %s)' % (unwrap(a[0], 'GTK_TABLE'), a[1]))
    s = replace_calls(s, 'gtk_table_set_col_spacings', lambda a:
        'gtk_grid_set_column_spacing(GTK_GRID(%s), %s)' % (unwrap(a[0], 'GTK_TABLE'), a[1]))

    def attach(a):
        t = unwrap(a[0], 'GTK_TABLE')
        hx = 'TRUE' if 'GTK_EXPAND' in a[6] else 'FALSE'
        vx = 'TRUE' if 'GTK_EXPAND' in a[7] else 'FALSE'
        return 'bimp_grid_attach(%s, %s, %s, %s, %s, %s, %s, %s)' % (t, a[1], a[2], a[3], a[4], a[5], hx, vx)
    s = replace_calls(s, 'gtk_table_attach', attach)
    s = replace_calls(s, 'gtk_table_attach_defaults', lambda a:
        'bimp_grid_attach(%s, %s, %s, %s, %s, %s, TRUE, TRUE)' % (unwrap(a[0], 'GTK_TABLE'), a[1], a[2], a[3], a[4], a[5]))

    s = s.replace('gtk_alignment_new(', 'bimp_align_new(')
    s = replace_calls(s, 'gtk_alignment_set_padding', lambda a:
        'bimp_align_set_padding(%s, %s, %s, %s, %s)' % (unwrap(a[0], 'GTK_ALIGNMENT'), a[1], a[2], a[3], a[4]))

    s = replace_calls(s, 'gtk_menu_popup', lambda a: 'gtk_menu_popup_at_pointer(%s, NULL)' % a[0])
    s = s.replace('gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_sequence), hbox_sequence)',
                  'gtk_container_add(GTK_CONTAINER(scroll_sequence), hbox_sequence)')
    for k, v in STOCK.items():
        s = re.sub(r'\b' + k + r'\b', v, s)
    return s


for path in sys.argv[1:]:
    src = open(path).read()
    new = convert(src)
    if new != src:
        open(path, 'w').write(new)
        print('converted', path)
