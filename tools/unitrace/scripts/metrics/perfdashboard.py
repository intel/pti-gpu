# perfdashboard.py
#
# Self-contained cutdown dashboard.
# Features:
#   - Upload a CSV performance file        (file_csv_input1)
#   - Choose graph display mode            (graph_mode_group)
#   - Select counters to visualise         (counter_multi_choice)
#
# All helper logic from visual_elements.py is inlined here.

from bokeh.plotting import figure, curdoc
from bokeh.models import (
    TabPanel, Tabs,
    MultiChoice, RadioButtonGroup, TextInput, CheckboxGroup, CustomJS,
    ColumnDataSource, HoverTool, Range1d, FactorRange,
    FixedTicker, BasicTickFormatter, BasicTicker,
)
from bokeh.models.widgets import Div, FileInput
from bokeh.layouts import column, row
from bokeh.palettes import Category10
from bokeh.transform import dodge

import argparse
from html import escape
import base64
import csv
import io
import json
import os.path as osp
import sys
import pandas as pd
import re


# ── Startup arguments (bokeh serve --args --csv PATH) ─────────────────────────

def parse_startup_args():
    """Parse the --csv path forwarded by bokeh serve via --args.

    Uses parse_known_args so that Bokeh's own argv entries are ignored silently.

    Only a browser session of the dashboard takes its file paths from the command
    line: the Bokeh server executes this file once per session, with the session
    context set and the arguments create_server() passes in sys.argv.  Running
    this file as the server launcher, see main() below, or importing it, has a
    command line of its own, which the dashboard reads nothing from.
    """
    p = argparse.ArgumentParser(add_help=False)
    p.add_argument("--csv", dest="csv_path", default=None)
    p.add_argument("--eustall", action="store_true", default=False)
    p.add_argument("--trace", dest="trace_path", default=None)
    session = curdoc().session_context is not None
    known, _ = p.parse_known_args(sys.argv[1:] if session else [])
    return known

startup_args = parse_startup_args()


# ── Counter sort helper ───────────────────────────────────────────────────────

def counter_unit_sort_key(name):
    """Sort key that groups counters by the unit inside trailing [ ] brackets.

    Primary key  : unit text, lower-cased  (e.g. '%', 'bytes', 'events', 'mhz')
    Secondary key: counter name, lower-cased (alphabetical within each group)

    Counters with no unit bracket sort last.
    """
    m = re.search(r'\[([^\]]+)\]\s*$', name)
    unit = m.group(1).lower() if m else "\xff"   # \xff sorts after all ASCII
    return (unit, name.lower())


# ── CSV column classification ────────────────────────────────────────────────

# Metadata columns that are NOT plottable counters.
CSV_EXCLUDE_COLS = {
    "querybegintime[ns]",
    "corefrequencymhz[mhz]",
}


def counter_cols_from_df(df):
    """Return a sorted list of plottable counter column names from the CSV DataFrame.

    Requires a trailing [unit] and skips metadata columns ()
    and columns whose sampled values are non-numeric strings.
    """
    counters = []
    for col in df.columns:
        unit = re.search(r'\[([^\[\]]+)\]\s*$', col)
        if not unit or not unit.group(1).strip() or col.strip().lower() in CSV_EXCLUDE_COLS:
            continue
        # Quick check: if every value is a string, skip it
        if not pd.api.types.is_numeric_dtype(df[col]):
            sample = df[col].dropna().head(5)
            if sample.empty:
                continue
            try:
                pd.to_numeric(sample, errors="raise")
            except (ValueError, TypeError):
                continue
        counters.append(col)
    return sorted(counters, key=counter_unit_sort_key)


# ── Data-access helpers ──────────────────────────────────────────────────────

# Global map: id(df) -> column_map, populated after every CSV upload.
df_col_map_dict = {}


def build_counter_column_map(df):
    """Build a case-insensitive lookup from short column names to full column names."""
    col_map = {}
    for col in df.columns:
        last_part = col.split('.')[-1].lower()
        col_map[last_part] = col
        col_map[col.lower()] = col
    return col_map


# ── File-path loaders (used when invoked via bokeh serve --args) ──────────────

def init_from_csv_path(csv_path):
    """Load a metrics CSV directly from *csv_path* (no base64 / file-upload)."""
    global df_input1, df_kernel, csv1_col_map
    try:
        with open(csv_path, "rb") as raw:
            content = raw.read()
        df_input1 = read_csv_smart(io.BytesIO(content))
    except Exception as exc:
        print(f"Error loading CSV from {csv_path}: {exc}")
        return False
    csv1_col_map = build_counter_column_map(df_input1)
    df_col_map_dict[id(df_input1)] = csv1_col_map
    df_kernel = df_input1
    index_kernels(df_input1)

    # Populate the counter list from the CSV headers
    csv_counters = counter_cols_from_df(df_input1)
    counter_multi_choice.options = csv_counters
    counter_multi_choice.value = []

    status_div.text = (
        f"<b>CSV:</b> {df_input1.shape[0]} rows × {df_input1.shape[1]} columns. "
        f"<b>{len(csv_counters)}</b> counters available."
    )
    return True


# ── EU Stall Sampling ─────────────────────────────────────────────────────────
#
# Column names in the EU stall CSV match the official Intel EU Stall Sampling
# field names (with [Events] suffix added by the unitrace collector).

EU_STALL_COLUMNS = [
    ('Active[Events]',          'Active (No Stall)',   '#2ca02c'),  # green  — healthy
    ('SbidStall[Events]',       'SBID (DRAM latency)', '#d62728'),  # red    — memory latency
    ('SendStall[Events]',       'Send (Memory access)','#ff7f0e'),  # orange — memory access
    ('DistStall[Events]',       'Dist/Acc Dep',        '#bcbd22'),  # yellow — ALU dependency
    ('PipeStall[Events]',       'Pipe Busy',           '#1f77b4'),  # blue   — compute bound
    ('SyncStall[Events]',       'Sync/Barrier',        '#9467bd'),  # purple — barrier
    ('InstrFetchStall[Events]', 'Instr Fetch',         '#e377c2'),  # pink   — I-cache miss
    ('ControlStall[Events]',    'Control/Branch',      '#17becf'),  # cyan   — branch
    ('PSDepStall[Events]',      'PS Dep (TDR)',        '#8c564b'),  # brown  — graphics only
    ('OtherStall[Events]',      'Other',               '#7f7f7f'),  # gray   — misc
]

# Global EU stall chart container (populated by init_eustall_from_csv_path)
eustall_status = Div(text='Loading EU stall data…', width=700)
eustall_plot_layout = column()


def fmt_ip(val):
    """Format an IP address value as a hex string (e.g. 0x0280)."""
    try:
        address = int(val.strip(), 16) if isinstance(val, str) else int(val)
        return f'0x{address:04x}'
    except Exception:
        return str(val)


def build_eu_stall_chart(df):
    """Build a two-panel EU stall dashboard.

    Panel 1 — Kernel Summary (vertical bars):
        X axis : stall category names (one bar per category per kernel)
        Y axis : total event count (summed across all IPs for that kernel)

    Panel 2 — IP Hotspots (horizontal stacked bars):
        Y axis : top-20 IP addresses ranked by total stall count
        X axis : stall event counts, stacked by category
    """
    df = df.copy()
    df.columns = df.columns.str.strip()

    kernel_col = df.columns[0]
    ip_col = df.columns[1] if len(df.columns) > 1 else None

    # Filter to stall columns present in this CSV
    available = [(col, label, color)
                 for col, label, color in EU_STALL_COLUMNS
                 if col in df.columns]
    if not available:
        return Div(text=(
            "<b style='color:red'>EU stall columns not found in CSV.</b><br>"
            f"Columns found: {list(df.columns)}<br>"
            "Expected: 'Active[Events]', 'SbidStall[Events]', etc."
        ))

    stall_cols   = [col for col, _, _ in available]
    stall_only   = [(col, lbl, clr) for col, lbl, clr in available if col != 'Active[Events]']
    stall_only_cols = [col for col, _, _ in stall_only]

    for c in stall_cols:
        df[c] = pd.to_numeric(df[c], errors='coerce').fillna(0)

    # ── Panel 1: Kernel Summary ───────────────────────────────────────────
    df_agg = df.groupby(kernel_col, sort=False)[stall_cols].sum().reset_index()

    cat_labels  = [lbl  for _, lbl,  _ in available]
    cat_colors  = [clr  for _, _,    clr in available]
    cat_cols    = [col  for col, _,  _ in available]
    n_kernels   = len(df_agg)
    bar_width   = 0.7 / max(n_kernels, 1)
    offsets     = [i * bar_width - (n_kernels - 1) * bar_width / 2
                   for i in range(n_kernels)]

    n_cats      = len(available)
    x_cat_pos   = list(range(n_cats))

    fig1 = figure(
        x_range=Range1d(-0.5, n_cats - 0.5),
        height=420,
        width=max(800, n_cats * 80 + 200),
        title='EU Stall Sampling — Stall Category Totals per Kernel',
        y_axis_label='Event Count (Samples)',
        toolbar_location='above',
        tools='pan,wheel_zoom,box_zoom,reset,save',
    )
    fig1.toolbar.logo = None
    fig1.y_range.start = 0
    fig1.xaxis.ticker = FixedTicker(ticks=x_cat_pos)
    fig1.xaxis.major_label_overrides = {x_cat_pos[i]: cat_labels[i] for i in range(n_cats)}
    fig1.xaxis.major_label_orientation = 0.6
    fig1.xaxis.major_label_text_font_size = '10px'
    fig1.yaxis.formatter = BasicTickFormatter(use_scientific=False)

    for ki in range(n_kernels):
        k_name = str(df_agg[kernel_col].iloc[ki]).strip('"').strip()
        k_label = (k_name[:40] + '…') if len(k_name) > 40 else k_name
        vals = [float(df_agg[col].iloc[ki]) for col in cat_cols]
        src1 = ColumnDataSource({
            'x':     [xp + offsets[ki] for xp in x_cat_pos],
            'top':   vals,
            'color': cat_colors,
            'cat':   cat_labels,
            'kern':  [k_label] * n_cats,
        })
        renderer = fig1.vbar(
            x='x', top='top', width=bar_width * 0.9,
            color='color', alpha=0.85,
            legend_label=k_label,
            source=src1,
        )
        fig1.add_tools(HoverTool(
            renderers=[renderer],
            tooltips=[
                ('Kernel',   '@kern'),
                ('Category', '@cat'),
                ('Count',    '@top{0,}'),
            ],
        ))

    fig1.legend.location    = 'top_right'
    fig1.legend.click_policy = 'hide'

    # ── Panel 2: IP Hotspots ─────────────────────────────────────────────
    if ip_col and stall_only_cols:
        df_ip = df.copy()
        df_ip['_ip_str']   = df_ip[ip_col].apply(fmt_ip)
        df_ip['_total']    = df_ip[stall_only_cols].sum(axis=1)
        df_ip = (df_ip.nlargest(20, '_total')
                      .sort_values('_total', ascending=True)
                      .reset_index(drop=True))

        m = len(df_ip)
        y_pos    = list(range(m))
        ip_labels = df_ip['_ip_str'].tolist()

        data2 = {'y': y_pos, 'ip': ip_labels}
        # the bars of a category start where the bars of the ones before it end, so
        # the stack is summed up column by column; the rows of df_ip are numbered
        # from 0 by the reset_index() above, which is the index left_arr aligns on
        left_arr = pd.Series(0.0, index=df_ip.index)
        for i, (col, lbl, clr) in enumerate(stall_only):
            vals = df_ip[col]
            right_arr = left_arr + vals
            data2[f'left_{i}']  = left_arr.tolist()
            data2[f'right_{i}'] = right_arr.tolist()
            data2[f'val_{i}']   = vals.tolist()
            left_arr = right_arr
        src2 = ColumnDataSource(data2)

        fig2 = figure(
            y_range=Range1d(-0.5, m - 0.5),
            height=max(350, m * 28 + 120),
            width=max(800, 1000),
            title='EU Stall Sampling — Top 20 IP Hotspots (Stall Events by Category)',
            x_axis_label='Stall Event Count',
            toolbar_location='above',
            tools='pan,wheel_zoom,box_zoom,reset,save',
        )
        fig2.toolbar.logo = None
        fig2.x_range.start = 0
        fig2.yaxis.ticker = FixedTicker(ticks=y_pos)
        fig2.yaxis.major_label_overrides = {y_pos[i]: ip_labels[i] for i in range(m)}
        fig2.yaxis.major_label_text_font_size = '11px'
        fig2.xaxis.formatter = BasicTickFormatter(use_scientific=False)

        for i, (col, lbl, clr) in enumerate(stall_only):
            renderer = fig2.hbar(
                y='y', left=f'left_{i}', right=f'right_{i}',
                height=0.65, color=clr, alpha=0.85,
                legend_label=lbl, source=src2,
            )
            fig2.add_tools(HoverTool(
                renderers=[renderer],
                tooltips=[
                    ('IP',       '@ip'),
                    ('Category', lbl),
                    ('Count',    f'@val_{i}{{0,}}'),
                ],
            ))

        fig2.legend.location    = 'bottom_right'
        fig2.legend.click_policy = 'hide'
        fig2.legend.orientation  = 'vertical'

        return column(fig1, fig2)

    return fig1


def select_eustall_kernel(frame, gid=None, kernel_name=None, trace_path=None):
    if gid is None and kernel_name is None:
        return frame
    if trace_path and gid is not None:
        with open(trace_path, encoding="utf-8") as source:
            trace = json.load(source)
        events = trace.get("traceEvents", []) if isinstance(trace, dict) else trace
        kernel_name = next((event.get("name") for event in events
                            if str(event.get("args", {}).get("id")) == str(gid)
                            and "metrics" in event.get("args", {})), None)
    if not kernel_name:
        raise ValueError(f"Cannot resolve kernel for GID {gid}; provide the matching trace JSON.")
    kernel_name = re.sub(r"\[SIMD.*\]$", "", kernel_name.strip().strip('"'))
    names = frame.iloc[:, 0].astype(str).str.strip().str.strip('"')
    selected = frame.loc[names == kernel_name].copy()
    if selected.empty:
        raise ValueError(f"No EU stall samples found for kernel {kernel_name}")
    return selected


def session_argument(name):
    context = curdoc().session_context
    if context is None:
        return None
    value = context.request.arguments.get(name, [None])[0]
    return value.decode("utf-8") if isinstance(value, bytes) else value


def init_eustall_from_csv_path(csv_path):
    """Load a EU stall CSV from disk and render the stall breakdown chart."""
    import traceback
    global df_input1
    try:
        with open(csv_path, 'rb') as raw:
            content = raw.read()
        df_input1 = read_csv_smart(io.BytesIO(content))
        df_input1 = select_eustall_kernel(
            df_input1, session_argument("gid"), session_argument("kernel"), startup_args.trace_path
        )
    except Exception as exc:
        print(f'Error loading EU stall CSV from {csv_path}: {exc}')
        eustall_status.text = f"<b style='color:red'>Error loading CSV: {exc}</b>"
        return False

    try:
        chart = build_eu_stall_chart(df_input1)
    except Exception as exc:
        tb = traceback.format_exc()
        print(f'Error building EU stall chart: {tb}')
        chart = Div(text=(
            f"<b style='color:red'>Chart build error: {exc}</b>"
            f"<pre style='font-size:11px'>{tb}</pre>"
        ))

    eustall_plot_layout.children = [chart] if chart is not None else [
        Div(text='<b>No chart generated.</b>')
    ]
    eustall_status.text = (
        f"<b>EU Stall CSV loaded:</b> {osp.basename(csv_path)} — "
        f"{df_input1.shape[0]} IP row(s), aggregated by kernel name across invocations."
    )
    return True


def select_kernel_by_gid(gid):
    """Select the primary kernel from a Perfetto link."""
    global updating_kernel_inputs, primary_kernel_id
    primary_kernel_id = str(gid)
    updating_kernel_inputs = True
    try:
        compare_kernel_input.value = ""
    finally:
        updating_kernel_inputs = False
    kernel_id_callback("value", "", str(gid))


# ── Visualisation helpers ─────────────────────────────────────────────────────


def normalize_series(series):
    """Clean and sort a list of (frame, value) tuples into parallel x/y lists."""
    cleaned = []
    for frame, value in series:
        try:
            frame_val = float(frame)
        except (TypeError, ValueError):
            continue
        if isinstance(value, str):
            value = value.replace(",", "")
        try:
            value_val = float(value)
        except (TypeError, ValueError):
            continue
        cleaned.append((frame_val, value_val))
    cleaned.sort(key=lambda t: t[0])
    return [f for f, _ in cleaned], [v for _, v in cleaned]


def show_counters_overlaid(metric_name, counter_series_1, counter_series_2=None,
                          x_axis_label="Time (ns)"):
    """Plot every sample of each selected counter on a shared numeric axis."""
    counter_series_1 = counter_series_1 or []
    counter_series_2 = counter_series_2 or []
    if not counter_series_1 and not counter_series_2:
        return None

    palette = Category10[10]
    fig = figure(
        height=550, width=900, title=metric_name,
        x_axis_label=x_axis_label, y_axis_label="Counter Value",
        toolbar_location="above", tools="pan,wheel_zoom,box_zoom,reset,save",
        sizing_mode="stretch_width",
    )
    labels = list(dict.fromkeys(label for label, _ in counter_series_1 + counter_series_2))
    for kernel_index, counters in enumerate((counter_series_1, counter_series_2)):
        for label, series in counters:
            xs, ys = normalize_series(series)
            if not xs:
                continue
            legend = f"Kernel {kernel_index + 1}: {label}" if counter_series_2 else label
            source = ColumnDataSource({"x": xs, "y": ys})
            color = palette[labels.index(label) % len(palette)]
            renderer = fig.line(
                "x", "y", source=source, color=color, line_width=2,
                line_dash="solid" if kernel_index == 0 else "dashed",
                legend_label=legend,
            )
            if len(xs) == 1:
                fig.scatter("x", "y", source=source, color=color, size=7, legend_label=legend)
            fig.add_tools(HoverTool(
                renderers=[renderer], tooltips=[("Counter", legend),
                    (x_axis_label, "@x{0,0.###}"), ("Value", "@y{0,0.######}")],
            ))
    if not fig.renderers:
        return None
    fig.toolbar.logo = None
    fig.y_range.start = 0
    fig.xaxis.ticker = BasicTicker(desired_num_ticks=8)
    fig.xaxis.formatter = BasicTickFormatter(use_scientific=False)
    fig.xaxis.major_label_text_font_size = "10px"
    fig.legend.visible = False
    fig.legend.click_policy = "hide"
    return fig


def is_query_sampling(df):
    columns = {str(col).strip().lower() for col in df.columns}
    return {"querysplitoccurred", "reportid", "reportscount"}.issubset(columns)


def show_query_counter_bars(metric_name, counter_series):
    if not counter_series:
        return None
    reports = sorted({sample for _, series in counter_series for sample, _ in series})
    factors = [f"{report:g}" for report in reports]
    fig = figure(
        x_range=FactorRange(factors=factors), height=550, width=900,
        title=metric_name, x_axis_label="Query report", y_axis_label="Counter Value",
        toolbar_location="above", tools="pan,wheel_zoom,box_zoom,reset,save",
        sizing_mode="stretch_width",
    )
    width = 0.8 / len(counter_series)
    for index, (label, series) in enumerate(counter_series):
        samples, values = normalize_series(series)
        source = ColumnDataSource({"x": [f"{sample:g}" for sample in samples], "top": values})
        renderer = fig.vbar(
            x=dodge("x", -0.4 + width * (index + 0.5), range=fig.x_range),
            top="top", width=width, source=source,
            color=Category10[10][index % 10], legend_label=label,
        )
        fig.add_tools(HoverTool(renderers=[renderer], tooltips=[
            ("Counter", label), ("Query report", "@x"), ("Value", "@top{0,0.######}"),
        ]))
    fig.toolbar.logo = None
    fig.y_range.start = 0
    fig.legend.visible = False
    return fig


def counter_plot_with_legend(fig):
    if fig is None:
        return None
    fig.min_border_left = 70
    fig.margin = 0
    items = list(fig.legend[0].items)
    renderers = [item.renderers for item in items]
    colors = [group[0].glyph.line_color for group in renderers]
    legend = CheckboxGroup(
        labels=[item.label.value for item in items],
        active=list(range(len(items))), inline=True,
        sizing_mode="stretch_width", max_height=140,
        margin=(0, 0, 0, fig.min_border_left),
        styles={"overflow-y": "auto", "overflow-x": "hidden"},
        stylesheets=["""
            .bk-input-group {
                display: flex;
                flex-wrap: wrap;
                gap: 6px 16px;
                max-height: 140px;
                overflow-y: auto;
                overflow-x: hidden;
            }
            label {
                display: inline-flex;
                align-items: center;
                margin: 0;
                min-width: 0;
                max-width: 100%;
                overflow-wrap: anywhere;
                white-space: normal;
            }
            input { flex-shrink: 0; }
        """ + "\n".join(
            f"label:nth-child({index + 1}) {{ border-bottom: 3px solid {color}; }}"
            for index, color in enumerate(colors)
        )],
    )
    legend.js_on_change("active", CustomJS(args={"groups": renderers}, code="""
        for (let index = 0; index < groups.length; index++) {
            for (const renderer of groups[index]) {
                renderer.visible = cb_obj.active.includes(index);
            }
        }
    """))
    return column(fig, legend, sizing_mode="stretch_width")


def show_counter_timeseries_tabs(metric_name, counter_series_1, counter_series_2):
    """One tab per counter showing a bar chart across all ReportIds."""
    counter_series_1 = counter_series_1 or []
    counter_series_2 = counter_series_2 or []
    if not counter_series_1 and not counter_series_2:
        return None

    palette = Category10[10]

    # Preserve counter ordering: series_1 first, then any extras from series_2
    label_map = dict(counter_series_1)
    for label, _ in counter_series_2:
        if label not in label_map:
            label_map[label] = []

    panels = []
    for idx, label in enumerate(label_map.keys()):
        series1 = dict(counter_series_1).get(label, [])
        series2 = dict(counter_series_2).get(label, [])
        xs1, ys1 = normalize_series(series1)
        xs2, ys2 = normalize_series(series2)
        if not xs1 and not xs2:
            continue

        # Convert x values to strings for categorical axis
        x_labels_1 = [str(int(x)) for x in xs1]
        x_labels_2 = [str(int(x)) for x in xs2]
        all_x = sorted(set(x_labels_1 + x_labels_2), key=lambda s: int(s))

        fig = figure(
            x_range=FactorRange(factors=all_x),
            height=500,
            width=max(900, len(all_x) * 40 + 200),
            title=f"{metric_name} – {label}",
            x_axis_label="ReportId",
            y_axis_label="Counter Value",
            toolbar_location="above",
            tools="pan,wheel_zoom,box_zoom,reset,save",
        )
        fig.toolbar.logo = None
        fig.y_range.start = 0

        has_both = bool(xs1) and bool(xs2)
        if xs1:
            src1 = ColumnDataSource({"x": x_labels_1, "top": ys1})
            if has_both:
                fig.vbar(x=dodge("x", -0.2, range=fig.x_range), top="top", width=0.35,
                         source=src1, color=palette[0], alpha=0.85, legend_label="Kernel 1")
            else:
                fig.vbar(x="x", top="top", width=0.65, source=src1,
                         color=palette[idx % len(palette)], alpha=0.85, legend_label="Kernel 1")
        if xs2:
            src2 = ColumnDataSource({"x": x_labels_2, "top": ys2})
            if has_both:
                fig.vbar(x=dodge("x", 0.2, range=fig.x_range), top="top", width=0.35,
                         source=src2, color=palette[1], alpha=0.85, legend_label="Kernel 2")
            else:
                fig.vbar(x="x", top="top", width=0.65, source=src2,
                         color=palette[idx % len(palette)], alpha=0.85, legend_label="Kernel 2")

        fig.add_tools(HoverTool(tooltips=[("ReportId", "@x"), ("Value", "@top{0,0.00}")]))
        fig.xaxis.major_label_orientation = 0.8
        fig.legend.location = "top_left"
        fig.legend.click_policy = "hide"
        panels.append(TabPanel(child=fig, title=label))

    return Tabs(tabs=panels) if panels else None


def show_counter_series_stack(metric_name, counter_series_1, counter_series_2=None):
    """Stacked individual bar charts (one per counter) across all ReportIds."""
    counter_series_1 = counter_series_1 or []
    counter_series_2 = counter_series_2 or []
    if not counter_series_1 and not counter_series_2:
        return None

    palette = Category10[10]
    ordered_labels = []
    seen = set()
    for label, _ in counter_series_1:
        if label not in seen:
            ordered_labels.append(label)
            seen.add(label)
    for label, _ in counter_series_2:
        if label not in seen:
            ordered_labels.append(label)
            seen.add(label)

    figs = []
    for idx, label in enumerate(ordered_labels):
        series1 = dict(counter_series_1).get(label, [])
        series2 = dict(counter_series_2).get(label, [])
        xs1, ys1 = normalize_series(series1)
        xs2, ys2 = normalize_series(series2)
        if not xs1 and not xs2:
            continue

        x_labels_1 = [str(int(x)) for x in xs1]
        x_labels_2 = [str(int(x)) for x in xs2]
        all_x = sorted(set(x_labels_1 + x_labels_2), key=lambda s: int(s))

        fig = figure(
            x_range=FactorRange(factors=all_x),
            height=300,
            width=max(900, len(all_x) * 40 + 200),
            title=f"{metric_name} – {label}",
            x_axis_label="ReportId",
            y_axis_label="Counter Value",
            toolbar_location="above",
            tools="pan,wheel_zoom,box_zoom,reset,save",
        )
        fig.toolbar.logo = None
        fig.y_range.start = 0

        has_both = bool(xs1) and bool(xs2)
        if xs1:
            src1 = ColumnDataSource({"x": x_labels_1, "top": ys1})
            if has_both:
                fig.vbar(x=dodge("x", -0.2, range=fig.x_range), top="top", width=0.35,
                         source=src1, color=palette[0], alpha=0.85, legend_label="Kernel 1")
            else:
                fig.vbar(x="x", top="top", width=0.65, source=src1,
                         color=palette[idx % len(palette)], alpha=0.85, legend_label="Kernel 1")
        if xs2:
            src2 = ColumnDataSource({"x": x_labels_2, "top": ys2})
            if has_both:
                fig.vbar(x=dodge("x", 0.2, range=fig.x_range), top="top", width=0.35,
                         source=src2, color=palette[1], alpha=0.85, legend_label="Kernel 2")
            else:
                fig.vbar(x="x", top="top", width=0.65, source=src2,
                         color=palette[idx % len(palette)], alpha=0.85, legend_label="Kernel 2")

        fig.add_tools(HoverTool(tooltips=[("ReportId", "@x"), ("Value", "@top{0,0.00}")]))
        fig.xaxis.major_label_orientation = 0.8
        fig.legend.location = "top_left"
        fig.legend.click_policy = "hide"
        figs.append(fig)

    return column(*figs) if figs else None


# ── Widgets ───────────────────────────────────────────────────────────────────

file_csv_input1 = FileInput(accept=".csv", width=400)
compare_kernel_input = TextInput(title="Compare With", max_width=400, sizing_mode="stretch_width")
kernel_name = Div(text="", sizing_mode="stretch_width", styles={"overflow-wrap": "anywhere"})
compare_kernel_name = Div(text="", sizing_mode="stretch_width", styles={"overflow-wrap": "anywhere"})
graph_mode_group = RadioButtonGroup(
    labels=["plotOverlaid", "plotAllSampleAsTabs", "plotAllSampleAsStack"],
    active=0,
)
counter_multi_choice = MultiChoice(
    title="Counters to Plot", options=[], value=[], sizing_mode="stretch_width",
    stylesheets=["""
        .choices__list--multiple {
            display: flex;
            flex-wrap: wrap;
            gap: 4px;
        }
        .choices__list--multiple .choices__item {
            display: inline-flex;
            align-items: center;
            width: auto;
            max-width: 100%;
            min-width: 0;
            box-sizing: border-box;
            margin: 0;
            white-space: normal;
            overflow-wrap: anywhere;
        }
        .choices__list--multiple .choices__button {
            flex-shrink: 0;
        }
    """],
)
status_div = Div(text="No data loaded.", width=600)


# ── Global state ──────────────────────────────────────────────────────────────

df_input1 = None
df_kernel = None        # filtered view of df_input1 for the first selected kernel
df_kernel2 = None       # filtered view of df_input1 for the second selected kernel
csv1_col_map = {}
gid_col = None         # name of the GlobalInstanceId column once detected
kernel_rows = {}
kernel_labels = []
selection_valid = False
updating_kernel_inputs = False
primary_kernel_id = ""

# Dynamic plot container — its .children list is replaced on every plot action.
metric_plot_layout = column(sizing_mode="stretch_width")

GRAPH_MODES = ["plotOverlaid", "plotAllSampleAsTabs", "plotAllSampleAsStack"]


# ── Callbacks ─────────────────────────────────────────────────────────────────

def read_csv_smart(f):
    """Parse each metric section with its own header and align columns by name."""
    f.seek(0)
    content = f.read()
    if isinstance(content, bytes):
        content = content.decode("utf-8-sig")
    sections = []
    header = None
    rows = []

    def finish_section():
        if header is not None and rows:
            buffer = io.StringIO()
            writer = csv.writer(buffer)
            writer.writerow(header)
            writer.writerows(rows)
            buffer.seek(0)
            sections.append(pd.read_csv(buffer, skipinitialspace=True,
                                        dtype={"IP[Address]": str}))

    for fields in csv.reader(io.StringIO(content), skipinitialspace=True):
        if not fields or not any(value.strip() for value in fields):
            continue
        first = fields[0].strip().lstrip("\ufeff")
        if first.startswith("==="):
            finish_section()
            header, rows = None, []
            continue
        if len(fields) < 2 and header is None:
            continue
        is_header = first.lower() == "kernel" and any(
            value.strip().lower() in ("globalinstanceid", "ip[address]") for value in fields[1:]
        )
        if header is None or is_header:
            finish_section()
            header = [value.strip().lstrip("\ufeff") for value in fields]
            rows = []
        else:
            if len(fields) != len(header):
                raise ValueError(f"Metrics row has {len(fields)} values; expected {len(header)}")
            rows.append(fields)
    finish_section()
    if not sections:
        raise ValueError("No metric data rows found in CSV")
    return clean_csv_df(pd.concat(sections, ignore_index=True, sort=False))


def clean_csv_df(df):
    """Remove device-banner rows and repeated header rows from a DataFrame.

    unitrace multi-device CSVs look like:
        === Device #0 Metrics ===
        Kernel,GlobalInstanceId,...
        data rows ...
        === Device #1 Metrics ===
        Kernel,GlobalInstanceId,...
        data rows ...

    After pd.read_csv, the banners and duplicate headers become data rows
    with string values in numeric columns.  Drop them and coerce types.
    """
    if df.empty:
        return df
    # Strip leading/trailing whitespace from column names (unitrace CSVs
    # use "Kernel, GlobalInstanceId, ..." with spaces after commas).
    df.columns = df.columns.str.strip()
    first_col = df.columns[0]
    # Drop rows whose first column looks like a '=== Device' banner
    mask_banner = df[first_col].astype(str).str.contains(r"^===\s*Device", na=False)
    # Drop rows that are repeated header lines (first column equals its header name)
    mask_header = df[first_col].astype(str).str.strip() == first_col.strip()
    df = df[~mask_banner & ~mask_header].reset_index(drop=True)

    # Coerce columns that should be numeric (GlobalInstanceId, counters)
    for col in df.columns:
        if col.strip().lower() in ("globalinstanceid", "reportid"):
            df[col] = pd.to_numeric(df[col], errors="coerce")
    return df


def index_kernels(df):
    """Index sample row positions without building per-kernel UI options."""
    global gid_col, kernel_rows, kernel_labels, selection_valid
    global updating_kernel_inputs, primary_kernel_id, df_kernel, df_kernel2
    gid_col = next(
        (c for c in df.columns if c.strip().lower() == "globalinstanceid"), None
    )
    if gid_col is not None:
        ids = pd.to_numeric(df[gid_col], errors="coerce")
        ids = ids.where((ids >= 0) & (ids % 1 == 0))
        kernel_rows = df.groupby(ids, sort=False).indices
    else:
        kernel_rows = {}
    kernel_labels = []
    selection_valid = gid_col is None
    df_kernel = df if selection_valid else None
    df_kernel2 = None
    df_col_map_dict.clear()
    df_col_map_dict[id(df)] = csv1_col_map
    primary_kernel_id = ""
    updating_kernel_inputs = True
    try:
        compare_kernel_input.value = ""
    finally:
        updating_kernel_inputs = False
    compare_kernel_input.disabled = gid_col is None
    kernel_name.text = ""
    compare_kernel_name.text = ""
    metric_plot_layout.children = []


def upload_csv_input1(attr, old, new):
    global df_input1, df_kernel, df_kernel2, csv1_col_map

    decoded = base64.b64decode(new)
    f = io.BytesIO(decoded)

    try:
        df_input1 = read_csv_smart(f)
    except Exception as e:
        print("Error: Could not read file as CSV.", e)
        status_div.text = "<b style='color:red'>Error: Could not read file as CSV.</b>"
        metric_plot_layout.children = []
        return

    csv1_col_map = build_counter_column_map(df_input1)
    df_col_map_dict[id(df_input1)] = csv1_col_map
    df_kernel = df_input1   # default: no filter applied

    index_kernels(df_input1)

    df_kernel2 = None
    metric_plot_layout.children = []
    counter_multi_choice.value = []
    counter_multi_choice.options = counter_cols_from_df(df_input1)
    status_div.text = (
        f"<b>CSV loaded:</b> {df_input1.shape[0]} rows × {df_input1.shape[1]} columns. "
        f"<b>{len(counter_multi_choice.options)}</b> counters available."
    )
    print(f"CSV loaded: {df_input1.shape}")


def build_counter_plot(metric_name, counters_1, counters_2,
                        x_axis_label="Time (ns)", query_sampling=False):
    """Dispatch to the correct plot type based on the active graph mode."""
    if query_sampling:
        return counter_plot_with_legend(show_query_counter_bars(metric_name, counters_1))
    mode = GRAPH_MODES[graph_mode_group.active] if 0 <= graph_mode_group.active < len(GRAPH_MODES) else "plotOverlaid"
    if mode == "plotOverlaid":
        return counter_plot_with_legend(
            show_counters_overlaid(metric_name, counters_1, counters_2, x_axis_label)
        )
    if mode == "plotAllSampleAsStack":
        return show_counter_series_stack(metric_name, counters_1, counters_2)
    return show_counter_timeseries_tabs(metric_name, counters_1, counters_2)


def plot_custom_counters_callback():
    """Main action: collect counter series from selected kernels and render plots."""
    if df_input1 is None or df_input1.empty:
        status_div.text = "<b style='color:red'>No data available. Upload a CSV first.</b>"
        return
    if not selection_valid:
        metric_plot_layout.children = []
        return

    mode = GRAPH_MODES[graph_mode_group.active] if 0 <= graph_mode_group.active < len(GRAPH_MODES) else "plotAllSampleAsTabs"

    # The counters are the CSV headers, so a counter is its own column name.
    if not counter_multi_choice.options:
        metric_plot_layout.children = []
        status_div.text = "<b style='color:orange'>No counters available to plot.</b>"
        return

    selected_counters = counter_multi_choice.value or list(counter_multi_choice.options)

    def resolve_column(df, counter_name):
        """Resolve a counter name to its DataFrame column (once per counter)."""
        col_map = df_col_map_dict.get(id(df))
        if not col_map:
            return None
        key = counter_name.lower()
        # Exact match
        matched = col_map.get(key)
        # Fallback: exact match after stripping [unit] suffix
        if not matched:
            for k, v in col_map.items():
                if re.sub(r'\[.*?\]$', '', k).strip() == key:
                    matched = v
                    break
        return matched

    def collect_series(df):
        if df is None or df.empty:
            return []
        query_sampling = is_query_sampling(df)
        time_col = next(
            (col for col in df.columns if col.strip().lower() == "querybegintime[ns]"), None
        )
        # Detect ReportId column for use as X axis
        report_id_col = next(
            (c for c in df.columns if c.strip().lower() == "reportid"), None
        )
        # Pre-compute X axis values once (vectorized)
        if not query_sampling and mode == "plotOverlaid" and time_col is not None:
            x_values = pd.to_numeric(df[time_col], errors="coerce")
        elif report_id_col is not None:
            x_values = pd.to_numeric(df[report_id_col], errors="coerce")
        else:
            x_values = None

        series_list = []
        for counter in selected_counters:
            # Resolve column once per counter, then read entire column
            col = resolve_column(df, counter)
            if col is None or col not in df.columns:
                continue
            y_values = pd.to_numeric(
                df[col].astype(str).str.replace(",", "", regex=False),
                errors="coerce",
            )
            series = []
            for idx in range(len(df)):
                y = y_values.iloc[idx]
                if pd.isna(y):
                    continue
                if not query_sampling and mode == "plotOverlaid" and time_col is not None and pd.isna(x_values.iloc[idx]):
                    continue
                x = x_values.iloc[idx] if x_values is not None and not pd.isna(x_values.iloc[idx]) else idx + 1
                series.append((float(x), float(y)))
            if series:
                # Label the series with the counter the user selected, which
                # keeps the [unit] suffix of the CSV header.
                series_list.append((counter, series))
        return series_list

    selected = kernel_labels
    active_df1 = df_kernel if df_kernel is not None else df_input1
    label1 = selected[0] if len(selected) >= 1 else "Kernel 1"

    counters_1 = collect_series(active_df1)
    if any(col.strip().lower() == "querybegintime[ns]" for col in active_df1.columns):
        x_axis_label = "Time (ns)"
    elif any(col.strip().lower() == "reportid" for col in active_df1.columns):
        x_axis_label = "ReportId"
    else:
        x_axis_label = "Sample number"
    plot1 = build_counter_plot(label1, counters_1, [], x_axis_label,
                                query_sampling=is_query_sampling(active_df1))

    if df_kernel2 is not None:
        label2 = selected[1] if len(selected) >= 2 else "Kernel 2"
        counters_2 = collect_series(df_kernel2)
        plot2 = build_counter_plot(label2, counters_2, [], x_axis_label,
                        query_sampling=is_query_sampling(df_kernel2))
        children = [p for p in (plot1, plot2) if p is not None]
        result = row(*children, sizing_mode="stretch_width") if children else None
    else:
        result = plot1

    metric_plot_layout.children = [result] if result else []
    if not result:
        status_div.text = "<b style='color:orange'>No data to plot for the selected counters.</b>"
    elif df_kernel2 is not None:
        status_div.text = f"<b>Plots updated</b> — two kernels compared, {len(counters_1)} counter series each, mode: {mode}."
    else:
        status_div.text = f"<b>Plot updated</b> ({len(counters_1)} counter series, mode: {mode})."


def kernel_id_callback(attr, old, new):
    global df_kernel, df_kernel2, kernel_labels, selection_valid
    if updating_kernel_inputs or df_input1 is None or df_input1.empty:
        return
    layout.children = [controls, metric_plot_layout]
    for frame in (df_kernel, df_kernel2):
        if frame is not None and frame is not df_input1:
            df_col_map_dict.pop(id(frame), None)
    df_kernel = None
    df_kernel2 = None
    kernel_labels = []
    selection_valid = False
    metric_plot_layout.children = []
    kernel_name.text = ""
    compare_kernel_name.text = ""
    status_div.text = ""
    primary = primary_kernel_id.strip()
    comparison = compare_kernel_input.value.strip()
    if not primary:
        kernel_name.text = "No primary kernel selected."
        return
    frames = []
    selected_ids = []
    for value, name_widget in ((primary, kernel_name), (comparison, compare_kernel_name)):
        if not value:
            continue
        if not re.fullmatch(r"[0-9]{1,20}", value):
            name_widget.text = "Kernel ID must be a non-negative whole number (up to 20 digits)."
            return
        gid = int(value)
        positions = kernel_rows.get(gid)
        if positions is None:
            name_widget.text = f"Kernel ID {gid} not found."
            if name_widget is kernel_name:
                status_div.text = "No metric data collected or no metrics specified"
                layout.children = [status_div]
            return
        if gid in selected_ids:
            name_widget.text = "Comparison kernel must differ from the primary kernel."
            return
        selected_ids.append(gid)
        filtered = df_input1.iloc[positions].reset_index(drop=True)
        frames.append(filtered)
        name_col = next((col for col in filtered.columns if col.strip().lower() == "kernel"), filtered.columns[0])
        name = str(filtered.iloc[0][name_col])
        short_name = name[:50] + ("..." if len(name) > 50 else "")
        name_widget.text = f"Kernel {gid}: {escape(short_name)}"
        kernel_labels.append(short_name)
    df_kernel = frames[0]
    df_kernel2 = frames[1] if len(frames) == 2 else None
    for frame in frames:
        df_col_map_dict[id(frame)] = csv1_col_map
    selection_valid = True
    status_div.text = ""
    if counter_multi_choice.value:
        plot_custom_counters_callback()


def counter_selection_callback(attr, old, new):
    if not new:
        # All counters deselected — clear the plot
        metric_plot_layout.children = []
        status_div.text = "<b>Counters cleared.</b>"
        return
    # Always re-render when counter selection changes
    try:
        plot_custom_counters_callback()
    except Exception as exc:
        import traceback
        traceback.print_exc()
        status_div.text = f"<b style='color:red'>Error updating plot: {exc}</b>"
        metric_plot_layout.children = []


def graph_mode_changed(attr, old, new):
    # Re-render with the new mode if a plot is already visible.
    if metric_plot_layout.children:
        plot_custom_counters_callback()


# ── Wire up callbacks ─────────────────────────────────────────────────────────

file_csv_input1.on_change('value', upload_csv_input1)
compare_kernel_input.on_change('value', kernel_id_callback)
counter_multi_choice.on_change("value", counter_selection_callback)
graph_mode_group.on_change("active", graph_mode_changed)


# ── Layout ────────────────────────────────────────────────────────────────────

controls = column(
    counter_multi_choice,
    compare_kernel_input,
    compare_kernel_name,
    file_csv_input1,
    status_div,
    sizing_mode="stretch_width",
)

layout = column(
    controls,
    metric_plot_layout,
    sizing_mode="stretch_width",
)

# ── Pre-load from startup arguments and/or session URL query params ───────────
#
# When invoked as:
#   bokeh serve perfdashboard.py --args --csv PATH [--trace PATH] [--eustall]
#
# startup_args carries the file paths.  Each browser session also receives the
# kernel GlobalInstanceId via the URL query parameter ?gid=N, which
# KernelLinkHandler below appends when the user clicks a kernel in Perfetto.

# ── EU Stall Sampling mode ────────────────────────────────────────────────────
#
# When --eustall is passed alongside --csv, render the EU stall chart directly
# instead of the regular counter viewer.

if startup_args.eustall and startup_args.csv_path:
    init_eustall_from_csv_path(startup_args.csv_path)
    eustall_root = column(
        Div(text='<h2>EU Stall Sampling</h2>'),
        eustall_status,
        eustall_plot_layout,
    )
    curdoc().title = 'EU Stall Sampling'
    curdoc().add_root(eustall_root)

else:
    # ── Regular OA metrics viewer ─────────────────────────────────────────────
    if startup_args.csv_path:
        init_from_csv_path(startup_args.csv_path)

    # Read per-session query parameter ?gid=N (set by KernelLinkHandler below
    # when Perfetto fires a kernel-selection request).
    session_gid = None
    try:
        req_args = curdoc().session_context.request.arguments
        gid_raw = req_args.get("gid", [None])[0]
        if gid_raw is not None:
            session_gid = int(
                gid_raw.decode("utf-8") if isinstance(gid_raw, bytes) else str(gid_raw)
            )
    except Exception:
        pass

    if session_gid is not None:
        select_kernel_by_gid(session_gid)

    if startup_args.csv_path:
        file_csv_input1.visible = False
        curdoc().title = "UniTrace Counter Viewer"

    curdoc().add_root(layout)


# ── Server of the dashboard and of the kernel links of an event trace ─────────
#
# Everything above is the Bokeh application: the Bokeh server executes this file
# once per browser session, so every session gets widgets and data of its own.
# The launcher below serves that application — this very file, given to a
# ScriptHandler — together with the kernel links of an event trace, on one port.
# It runs when the file is the main program; sessions do not reach it, since
# Bokeh executes them as a module of the application, not as __main__.

import logging
import os
import socket
import subprocess
import time
import urllib.request
import warnings
from urllib.parse import urlencode

from bokeh.application import Application
from bokeh.application.handlers.script import ScriptHandler
from bokeh.core.validation import silence
from bokeh.core.validation.warnings import EMPTY_LAYOUT
from bokeh.server.server import Server
from bokeh.util.warnings import BokehUserWarning
from tornado.ioloop import IOLoop
from tornado.web import RequestHandler


APP = "perfdashboard"           # name of the dashboard application
ROUTE = "/" + APP               # its path on the server
DEFAULT_PORT = 8000             # the port the kernel links of an event trace use


def check_port_available(port):
    """Raise OSError if a server already listens on port, on either address family."""
    for host in ("127.0.0.1", "::1"):
        try:
            connection = socket.create_connection((host, port), timeout=0.5)
        except OSError:
            continue
        connection.close()
        raise OSError(f"Port {port} is in use. Stop its server explicitly before retrying.")


def stop_owned_process(proc):
    """Stop a server this process started; never touches an unrelated process."""
    if proc is not None and proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)


class KernelLinkHandler(RequestHandler):
    def get(self, kernel_name, instance):
        if not instance.isascii() or not instance.isdigit():
            self.set_status(400)
            self.finish("Missing or invalid kernel instance id")
            return
        query = urlencode({"gid": instance, "kernel": kernel_name.strip('"')})
        self.redirect(ROUTE + "?" + query)


def create_server(args, io_loop=None, port=DEFAULT_PORT):
    warnings.filterwarnings(
        "ignore", message=r"^reference already known '[^']+'$", category=BokehUserWarning,
    )
    for name in ("bokeh", "tornado"):
        logging.getLogger(name).setLevel(logging.WARNING)
    silence(EMPTY_LAYOUT, True)
    script = os.path.abspath(__file__)      # the application is this file
    arguments = ["--csv", os.path.abspath(args.input)]
    if args.trace:
        arguments.extend(["--trace", os.path.abspath(args.trace)])
    eustall = args.eustall
    if not eustall:
        with open(args.input) as source:
            for line in source:
                if "IP[Address]" in line:
                    eustall = True
                    break
                if "GlobalInstanceId" in line:
                    break
    if eustall:
        arguments.append("--eustall")
    handler = ScriptHandler(filename=script, argv=arguments)
    return Server(
        {ROUTE: Application(handler)},
        io_loop=io_loop,
        address="localhost",
        port=port,
        session_token_expiration=3600,
        allow_websocket_origin=[f"localhost:{port}", f"127.0.0.1:{port}"],
        extra_patterns=[
            (rf"/(?!{APP}(?:/|$)|static/)(.+)/([^/]+)", KernelLinkHandler),
        ],
    )


def run_server(args, port=DEFAULT_PORT):
    check_port_available(port)
    loop = IOLoop()
    server = None
    try:
        server = create_server(args, io_loop=loop, port=port)
        server.start()
        print(f"Server ready at {dashboard_url(port)}", flush=True)
        print("Press Ctrl+C to exit.", flush=True)
        loop.start()
    except KeyboardInterrupt:
        pass
    finally:
        if server is not None:
            server.stop()
        loop.close(all_fds=True)


# ── Serving the dashboard for a viewer of an event trace ──────────────────────
#
# A viewer of an event trace, see scripts/uniview.py, opens the trace itself and
# leaves the metrics of its kernels to the dashboard.  It therefore runs the
# dashboard as a child process of its own, started and awaited below, instead of
# calling run_server() and giving up its own control flow.

def dashboard_url(port=DEFAULT_PORT):
    """Return the URL a kernel link of an event trace redirects to."""
    return f"http://localhost:{port}{ROUTE}"


def start_server_process(metrics, trace=None, eustall=False,
                         port=DEFAULT_PORT, timeout=30, prewarm=True):
    """Serve metrics in a child process and return its handle, None if it fails.

    The child is this file, run as the program of the command line parse_arguments()
    reads.  It is ready, and warmed up if prewarm is set, when this returns.
    """
    check_port_available(port)
    command = [sys.executable, os.path.abspath(__file__), os.path.abspath(metrics)]
    if trace is not None:
        command.extend(["--trace", os.path.abspath(trace)])
    if eustall:
        command.append("--eustall")

    proc = subprocess.Popen(command, stdout=subprocess.DEVNULL)

    for _ in range(2 * timeout):        # wait in steps of half a second
        if proc.poll() is not None:
            print(f"[ERROR] Server exited with code {proc.returncode}")
            return None
        time.sleep(0.5)
        try:
            with socket.create_connection(("localhost", port), timeout=1):
                print(f"Server ready at {dashboard_url(port)}")
                if prewarm:
                    prewarm_session(port)
                return proc
        except OSError:
            pass

    print(f"[WARNING] Server did not start within {timeout} seconds.")
    stop_owned_process(proc)
    return None


def prewarm_session(port=DEFAULT_PORT, timeout=60):
    """Block until the server has built its first session.

    This loads the metric data and populates the widgets before the viewer of the
    event trace opens, so that the first kernel a user clicks renders at once.
    """
    try:
        # Constant-scheme localhost URL (see dashboard_url); this only prewarms
        # the Bokeh server this process just started, so B310 does not apply.
        with urllib.request.urlopen(dashboard_url(port), timeout=timeout):  # nosec B310
            pass
    except Exception:
        pass    # non-fatal; the first kernel click is just slightly slower


def wait_for_exit(proc):
    """Serve kernel links of an event trace until the server of proc exits."""
    return proc.wait()


def parse_arguments(argv=None):
    """Parse the command line of the server: the metric data file and its options."""
    p = argparse.ArgumentParser(
        description="GPU Kernel Performance Hardware Metrics Dashboard")
    p.add_argument("--trace",
                   help="event trace in JSON format, used to resolve EU stall kernel selections")
    p.add_argument("--eustall", action="store_true",
                   help="treat the input file as EU stall sampling data (detected from the file if not specified)")
    p.add_argument("input",
                   help="hardware performance metric data file in .csv format generated by unitrace -k/-q/--stall-sampling")
    return p.parse_args(argv)


def main(argv=None):
    """Serve the dashboard of a metric data file, see run_server()."""
    try:
        run_server(parse_arguments(argv))
    except OSError as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
