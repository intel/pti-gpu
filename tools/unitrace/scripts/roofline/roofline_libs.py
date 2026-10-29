import pandas as pd
import csv
import re
import math

from bokeh.embed import file_html
from bokeh.layouts import column, row
from bokeh.models import (Button, CheckboxGroup, ColumnDataSource, CustomJS, DataRange1d, Div, Dropdown,
                          HoverTool, InlineStyleSheet, Legend, LegendItem, NumeralTickFormatter,
                          Spacer)
from bokeh.plotting import figure
from bokeh.resources import INLINE


class Roofline:
    def __init__(self, kernel_name, l3_read, l3_write, l3_bytes, gpu_memory_read, gpu_memory_write, gpu_memory_bytes,
                 fp64, fp32, fp16, fp16_xmx, bf16_xmx, kernel_time, total_gpu_time, total_gpu_time_flops,
                 average_core_freq, xve_threads_occupacy, fp16_xmx_utilization):
        self.kernel_name = kernel_name
        self.l3_read = l3_read
        self.l3_write = l3_write
        self.L3 = l3_bytes
        self.gpu_memory_read = gpu_memory_read
        self.gpu_memory_write = gpu_memory_write
        self.gpu_memory = gpu_memory_bytes
        self.FP64 = fp64 * 32
        self.FP32 = fp32 * 32
        self.FP16 = fp16 * 64
        self.FP16_XMX = fp16_xmx * 512
        self.BF16_XMX = bf16_xmx * 512
        self.kernel_time = kernel_time
        self.total_gpu_time = total_gpu_time
        self.total_gpu_time_flops = total_gpu_time_flops
        self.average_core_freq = average_core_freq
        self.XVE_THREADS_OCCUPANCY = xve_threads_occupacy
        self.fp16_xmx_utilization = fp16_xmx_utilization


class Platform:
    REQUIRED_KEYS = ('PlatformName', 'FP16_GFLOPS', 'FP16_XMX_GFLOPS', 'FP32_GFLOPS', 'FP64_GFLOPS',
                     'BF16_XMX_GFLOPS', 'GPU_MEMORY_BW_in_GB_per_sec', 'L3_BW_in_GB_per_sec')

    def __init__(self, config_file):
        # squeeze("columns"), not squeeze(): a one-row file would collapse to a scalar.
        config = pd.read_csv(config_file, index_col=0, header=None).squeeze("columns")
        if not isinstance(config, pd.Series):
            raise ValueError(f"Device config {config_file} must have two columns: name, value")
        config = config.to_dict()
        missing = [key for key in self.REQUIRED_KEYS if key not in config]
        if missing:
            raise ValueError(f"Device config {config_file} is missing {', '.join(missing)}")
        self.PlatformName = config['PlatformName']
        self.FP16 = float(config['FP16_GFLOPS'])
        self.FP16_XMX = float(config['FP16_XMX_GFLOPS'])
        self.FP32 = float(config['FP32_GFLOPS'])
        self.FP64 = float(config['FP64_GFLOPS'])
        self.BF16_XMX = float(config['BF16_XMX_GFLOPS'])
        self.GPU_MEMORY_BW = float(config['GPU_MEMORY_BW_in_GB_per_sec'])
        self.L3 = float(config['L3_BW_in_GB_per_sec'])


def read_unitrace_report(path_to_flops, path_to_bytes):
    entry_list = []
    df = file_to_json(path_to_bytes)
    df_flops = file_to_json(path_to_flops)
    # Memory

    grouped_df = df.groupby('Kernel').agg({'GpuTime[ns]': ['sum']})
    sorted_df = grouped_df.sort_values(by=('GpuTime[ns]', 'sum'), ascending=False)
    average_freq = df.groupby('Kernel').agg({'AvgGpuCoreFrequencyMHz[MHz]': ['mean']})
    average_xve_threads_occupancy = df.groupby('Kernel').agg({'XVE_THREADS_OCCUPANCY_ALL[%]': ['mean']})
    total_gpu_time = sorted_df[('GpuTime[ns]', 'sum')].sum()
    sum_l3_byte_read = df.groupby('Kernel').agg({'L3_BYTE_READ[bytes]': ['sum']})
    sum_l3_byte_write = df.groupby('Kernel').agg({'L3_BYTE_WRITE[bytes]': ['sum']})
    sum_gpu_memory_byte_read = df.groupby('Kernel').agg({'GPU_MEMORY_BYTE_READ[bytes]': ['sum']})
    sum_gpu_memory_byte_write = df.groupby('Kernel').agg({'GPU_MEMORY_BYTE_WRITE[bytes]': ['sum']})
    # Flops
    grouped_df_flops = df_flops.groupby('Kernel').agg({'GpuTime[ns]': ['sum']})
    sorted_df_flops = grouped_df_flops.sort_values(by=('GpuTime[ns]', 'sum'), ascending=False)
    total_gpu_time_flops = sorted_df_flops[('GpuTime[ns]', 'sum')].sum()

    xve_inst_executed_fp64 = df_flops.groupby('Kernel').agg({'XVE_INST_EXECUTED_FP64[events]': ['sum']})
    xve_inst_executed_fp32 = df_flops.groupby('Kernel').agg({'XVE_INST_EXECUTED_FP32[events]': ['sum']})
    xve_inst_executed_fp16 = df_flops.groupby('Kernel').agg({'XVE_INST_EXECUTED_FP16[events]': ['sum']})
    xve_inst_executed_xmx_bf16 = df_flops.groupby('Kernel').agg({'XVE_INST_EXECUTED_XMX_BF16[events]': ['sum']})
    xve_inst_executed_xmx_fp16 = df_flops.groupby('Kernel').agg({'XVE_INST_EXECUTED_XMX_FP16[events]': ['sum']})
    xve_inst_executed_xmx_fp16_utilization = df_flops.groupby('Kernel').agg(
        {'XVE_INST_EXECUTED_XMX_FP16_UTILIZATION[%]': ['mean']})
    for kernel in sorted_df.index.to_list():
        if kernel in xve_inst_executed_fp64.index:
            kernel_name = kernel
            kernel_time = sorted_df.loc[kernel, ('GpuTime[ns]', 'sum')]
            average_core_freq = average_freq.loc[kernel, ('AvgGpuCoreFrequencyMHz[MHz]', 'mean')]
            xve_threads_occupancy = average_xve_threads_occupancy.loc[kernel, ('XVE_THREADS_OCCUPANCY_ALL[%]', 'mean')]
            l3_byte_write = sum_l3_byte_write.loc[kernel, ('L3_BYTE_WRITE[bytes]', 'sum')]
            l3_byte_read = sum_l3_byte_read.loc[kernel, ('L3_BYTE_READ[bytes]', 'sum')]
            l3_byte = l3_byte_write + l3_byte_read
            gpu_byte_read = sum_gpu_memory_byte_read.loc[kernel, ('GPU_MEMORY_BYTE_READ[bytes]', 'sum')]
            gpu_byte_write = sum_gpu_memory_byte_write.loc[kernel, ('GPU_MEMORY_BYTE_WRITE[bytes]', 'sum')]
            gpu_memory_bytes = gpu_byte_read + gpu_byte_write
            fp64 = xve_inst_executed_fp64.loc[kernel, ('XVE_INST_EXECUTED_FP64[events]', 'sum')]
            fp32 = xve_inst_executed_fp32.loc[kernel, ('XVE_INST_EXECUTED_FP32[events]', 'sum')]
            fp16 = xve_inst_executed_fp16.loc[kernel, ('XVE_INST_EXECUTED_FP16[events]', 'sum')]
            bf16_xmx = xve_inst_executed_xmx_bf16.loc[kernel, ('XVE_INST_EXECUTED_XMX_BF16[events]', 'sum')]
            fp16_xmx = xve_inst_executed_xmx_fp16.loc[kernel, ('XVE_INST_EXECUTED_XMX_FP16[events]', 'sum')]
            fp16_xmx_utilization = xve_inst_executed_xmx_fp16_utilization.loc[kernel, ('XVE_INST_EXECUTED_XMX_FP16_UTILIZATION[%]', 'mean')]
            entry = Roofline(kernel_name, l3_byte_read, l3_byte_write, l3_byte, gpu_byte_read,
                             gpu_byte_write, gpu_memory_bytes, fp64, fp32,
                             fp16, fp16_xmx, bf16_xmx, kernel_time, total_gpu_time, total_gpu_time_flops,
                             average_core_freq, xve_threads_occupancy, fp16_xmx_utilization)
            entry_list.append(entry)
    return entry_list


def summary(entry_list, platform):
    flop_fp16 = 0
    flop_fp32 = 0
    flop_fp64 = 0
    flop_bf16_xmx = 0
    flop_fp16_xmx = 0
    total_kernel = 0
    total_flops = 0
    print("Kernel_name ,time_elapsed (s) ,% of Total time,  Bounded_by , FP16 (GFLOPS), FP32 (GFLOPS), FP64 (GFLOPS) ,"
          " BF16_XMX (GFLOPS),FP16_XMX(Gops), FP16_XMX (GFLOPS),fp16_xmx_utilization [%], AI (gpu_memory), AI (L3) ,gpu_memory (GB/s), L3 (GB/s)")

    for index, entry in enumerate(entry_list):

        ops = {
            'FP16': entry.FP16 / 1e9,
            'BF16_XMX': entry.BF16_XMX / 1e9,
            'FP32': entry.FP32 / 1e9,
            'FP64': entry.FP64 / 1e9,
            'FP16_XMX': entry.FP16_XMX / 1e9,
        }

        num_flops = max(ops.values()) + 1e-7
        type_flops = max(ops, key=ops.get)
        gflops = num_flops / (entry.kernel_time / 1e9)  # Tflops/s
        headroom_current = {
            'L3': ((num_flops / ((float(entry.L3) / 1024 ** 3) + 1e-7)) / (((num_flops / (
                        (float(entry.L3) / 1024 ** 3) + 1e-7)) * (1 / float(getattr(platform, type_flops)))) + (
                                                                                       1 / float(platform.L3)))) / (
                              gflops + 1e-7),
            'gpu_memory': ((num_flops / ((entry.gpu_memory / 1024 ** 3) + 1e-7)) / (((num_flops / (
                        (entry.gpu_memory / 1024 ** 3) + 1e-7)) * (1 / float(getattr(platform, type_flops)))) + (
                                                                                  1 / platform.GPU_MEMORY_BW))) / (gflops + 1e-7),
        }

        intesect_platform = {
            'gpu_memory': platform.FP32 / platform.GPU_MEMORY_BW,
            'L3': platform.FP32 / platform.L3,
        }

        limitation = min(headroom_current, key=headroom_current.get)
        ai = num_flops / (getattr(entry, limitation) / 1024 ** 3)

        if ai < intesect_platform[limitation]:
            bounded_by = limitation
        else:
            bounded_by = "compute"

        time_kernel = entry.kernel_time / 1e9
        cleaned_kernel_name = re.sub(r'[^a-zA-Z]', '', entry.kernel_name)
        print(cleaned_kernel_name, ",", entry.kernel_time / 1e9, ",", entry.kernel_time * 100 / entry.total_gpu_time, "%",
              ',', bounded_by, ',', 1e9 * ops['FP16'] / entry.kernel_time, ',', 1e9 * ops['FP32'] / entry.kernel_time, ',', 1e9 * ops['FP64'] / entry.kernel_time, ',',
              1e9 * ops['BF16_XMX'] / entry.kernel_time, ',', 1e9 * ops['FP16_XMX'], ',',
              1e9 * ops['FP16_XMX'] / entry.kernel_time, ',',
              entry.fp16_xmx_utilization, ',', 1e9 * ops['FP16_XMX'] / entry.gpu_memory, ',', 1e9 * ops['FP16_XMX'] / entry.L3,
              ',', (entry.gpu_memory / 1000 ** 3) / (entry.kernel_time / 1e9), ',',
              (entry.L3 / 1000 ** 3) / (entry.kernel_time / 1e9))

        flop_bf16_xmx += entry.BF16_XMX
        flop_fp16 += entry.FP16
        flop_fp16_xmx += entry.FP16_XMX
        flop_fp32 += entry.FP32
        flop_fp64 += entry.FP64
        total_flops += sum(ops.values())
        total_kernel += time_kernel
    print('FP16_total TFlop :', flop_fp16)
    print('FP16_XMX_total TFlop :', flop_fp16_xmx)
    print('BF16_XMX_total TFlop :', flop_bf16_xmx)
    print('FP32_total Tflop :', flop_fp32)
    print('FP64_total Tflop :', flop_fp64)
    print('Total_time', total_kernel)
    print('Total achieved Flops', total_flops / total_kernel)


def plot_html_roofline(entry_list, platform, test_case):
    variable_dict = {
        'FP16': sum(entry.FP16 for entry in entry_list),
        'FP16_XMX': sum(entry.FP16_XMX for entry in entry_list),
        'BF16_XMX': sum(entry.BF16_XMX for entry in entry_list),
        'FP32': sum(entry.FP32 for entry in entry_list),
        'FP64': sum(entry.FP64 for entry in entry_list),
    }
    precision = max(variable_dict, key=variable_dict.get)
    ai = [0.001 * 2 ** n for n in range(0, int(math.log2(1000000 / 0.001)) + 1)]
    peak_flops = float(getattr(platform, precision))
    roofs = []
    for bandwidth in (platform.L3, platform.GPU_MEMORY_BW):
        # The ridge, where the roof turns flat, is a vertex of its own: the samples rarely
        # hit it, and a straight segment between the two around it cuts below the roof.
        roof_ai = sorted(set(ai + [peak_flops / bandwidth]))
        roofs.append((roof_ai, [min(x * bandwidth, peak_flops) for x in roof_ai]))

    l3_points = {'x': [], 'y': [], 'size': [], 'label': []}
    gpu_memory_points = {'x': [], 'y': [], 'size': [], 'label': []}
    min_pixel = 10
    max_pixel = 100
    for entry in entry_list:
        if (entry.kernel_time * 100 / entry.total_gpu_time > 0.001) and getattr(entry, precision) > 0:
            scale_factor = min(1, math.log(entry.total_gpu_time) / math.log(100))
            size_cercle = min_pixel + (max_pixel - min_pixel) * scale_factor * entry.kernel_time / entry.total_gpu_time
            porcentage_total_time = entry.kernel_time * 100 / entry.total_gpu_time
            label = str("{:.2f}".format(porcentage_total_time)) + '  % ' + entry.kernel_name
            gflops = float(getattr(entry, precision)) / float(entry.kernel_time)
            for points, num_bytes in ((l3_points, entry.L3), (gpu_memory_points, entry.gpu_memory)):
                points['x'].append(float(getattr(entry, precision)) / float(num_bytes))
                points['y'].append(gflops)
                points['size'].append(float(size_cercle))
                points['label'].append(label)

    # Bokeh (BSD-3, in requirements.txt) with its JavaScript inlined: the report is one
    # self-contained file that opens offline and loads nothing from a third-party server.
    # It keeps the look and interactions of the CanvasJS chart it replaced: no pan or zoom,
    # a menu to print or save the chart, the legend above the plot (click to hide a series),
    # and a kernel panel that collapses to widen the chart.
    # The range fits the dot centres, not their size: a decade of headroom above the highest
    # point keeps the biggest dots (up to max_pixel wide) inside the plot.
    top = max([peak_flops] + l3_points['y'] + gpu_memory_points['y'])
    plot = figure(title=test_case + '  ' + precision, x_axis_type='log', y_axis_type='log',
                  x_range=(0.001, 100000), y_range=DataRange1d(start=0.1, end=top * 10),
                  x_axis_label='Arithmetic Intensity (Flop/Byte)', y_axis_label='Performance (GFlop/s)',
                  tools='', toolbar_location=None, sizing_mode='stretch_both')
    plot.title.text_font_size = '50px'
    plot.title.text_font_style = 'normal'
    plot.title.align = 'center'
    plot.xgrid.visible = False
    for axis in (plot.xaxis, plot.yaxis):
        axis.axis_label_text_font_size = '20px'
        axis.axis_label_text_font_style = 'normal'
        axis.major_label_text_font_size = '16px'
        axis.formatter = NumeralTickFormatter(format='0,0.[000]')  # 1,000 rather than 10^3
    legend_items = []
    for (name, color), (roof_ai, peak) in zip((('L3_ROOF', '#4F81BC'), ('Gpu_memory_ROOF', '#C0504E')), roofs):
        roof = plot.line(roof_ai, peak, color=color, line_width=2)
        plot.add_tools(HoverTool(renderers=[roof], tooltips='@y{0,0.[00]} GF/s'))
        legend_items.append(LegendItem(label=name, renderers=[roof]))

    # Wrap long (templated C++) kernel names onto several lines instead of cutting them off.
    wrap_labels = InlineStyleSheet(css='label { white-space: normal; overflow-wrap: anywhere; align-items: flex-start; }')
    sections = []
    groups = []
    for name, color, points in (('L3_data', 'blue', l3_points), ('Gpu_memory_data', 'red', gpu_memory_points)):
        all_points = ColumnDataSource(points)
        shown_points = ColumnDataSource(dict(points))
        scatter = plot.scatter('x', 'y', size='size', source=shown_points, color=color,
                               hover_fill_color=color, hover_line_color='black', hover_line_width=2)
        plot.add_tools(HoverTool(renderers=[scatter], tooltips='@label<br/>@y{0,0.[00]} GF/s'))
        legend_items.append(LegendItem(label=name, renderers=[scatter]))
        group = CheckboxGroup(labels=points['label'], active=list(range(len(points['label']))),
                              sizing_mode='stretch_width', stylesheets=[wrap_labels])
        # Show only the checked kernels: copy them from the full data into the plotted source.
        group.js_on_change('active', CustomJS(args=dict(all_points=all_points, shown_points=shown_points), code='''
            const keep = new Set(cb_obj.active);
            const data = {};
            for (const key of Object.keys(all_points.data)) {
                data[key] = Array.from(all_points.data[key]).filter((_, i) => keep.has(i));
            }
            shown_points.data = data;
        '''))
        sections += [Div(text='<h3>' + name + '</h3>'), group]
        groups.append(group)
    plot.add_layout(Legend(items=legend_items, orientation='horizontal', location='top_left',
                           click_policy='hide', label_text_font_size='14px', label_text_font_style='bold',
                           border_line_color=None), 'above')

    menu = Dropdown(label='\u2630', width=50, menu=[('Print', 'print'), ('Save as JPEG', 'jpeg'),
                                                ('Save as PNG', 'png')])
    menu.js_on_event('menu_item_click', CustomJS(args=dict(plot=plot, name=test_case), code='''
        if (this.item === 'print') {
            window.print();
            return;
        }
        const canvas = Bokeh.index.get_one(plot).export('png').canvas;
        const link = document.createElement('a');
        link.href = canvas.toDataURL('image/' + this.item);
        link.download = name + (this.item === 'jpeg' ? '.jpg' : '.png');
        link.click();
    '''))
    chart = column(row(Spacer(sizing_mode='stretch_width'), menu, sizing_mode='stretch_width'), plot,
                   sizing_mode='stretch_both')

    select_all = CheckboxGroup(labels=['Select/Deselect All'], active=[0])
    select_all.js_on_change('active', CustomJS(args=dict(groups=groups), code='''
        for (const group of groups) {
            group.active = cb_obj.active.length ? group.labels.map((_, i) => i) : [];
        }
    '''))
    kernel_list = column([select_all] + sections)
    toggle = Button(label='Kernel Names', width=120)
    panel = column(toggle, kernel_list, width=350, sizing_mode='stretch_height',
                   styles={'border': '1px solid #ccc', 'padding': '10px', 'overflow-y': 'auto'})
    toggle.js_on_event('button_click', CustomJS(args=dict(kernel_list=kernel_list, panel=panel), code='''
        kernel_list.visible = !kernel_list.visible;
        panel.width = kernel_list.visible ? 350 : 140;  // collapsed, the chart takes the room
    '''))
    layout = row(chart, panel, sizing_mode='stretch_both')
    with open(test_case + ".html", "w+") as file:
        file.write(file_html(layout, INLINE, title=test_case))


def file_to_json(file_name):
    """Read the metrics of a unitrace query profile into one flat frame, a row per instance.

    Metrics come in blocks: a label line naming the identity columns the block carries, then
    those values, then the counter names, then the counter values. The identity is copied
    onto every counter row so the frame can be grouped by 'Kernel', and because the label is
    what names the identity columns, the SubDeviceId or Device an older unitrace wrote there
    needs no special case here; a current one writes SubDeviceId as the first counter when
    there is more than one sub-device.

    Reading starts at the metrics banner, as it always has: the timing summary ahead of it
    has a 'Kernel'-headed table of its own that would otherwise be taken for a block. Only
    kernels are of interest, so rows whose name carries no SIMD width - memory copies and
    the like - are dropped at the end, as they were when this read the flat format.
    """
    identity_columns = None
    identity = None
    columns = None
    rows = []
    pending = None
    reading = False

    try:
        with open(file_name, "r", newline="") as file:
            lines = file.readlines()

        for fields in csv.reader(lines, skipinitialspace=True):
            if len(fields) == 1 and 'Metrics ==' in fields[0]:
                reading = True    # a banner, and in a merged file there is one per profile
                continue
            if not reading:
                continue
            if len(fields) == 0:
                # The blank line between blocks. Nothing a block is waiting for, so skip it rather
                # than let it fill the pending identity or counter-header slot: a file truncated
                # between an identity line and its counter header would otherwise latch an empty
                # column list and drop every row that follows.
                continue
            if fields[0] == 'Kernel':
                if identity_columns is None:
                    identity_columns = fields  # identical across every block
                identity = None
                pending = 'identity'
                continue
            if pending == 'identity':
                identity = fields[0:len(identity_columns)]
                pending = 'columns'
                continue
            if pending == 'columns':
                if columns is None:
                    columns = fields
                pending = None
                continue
            # A counter row of the block in progress. Anything of the wrong width - a stray
            # banner, a truncated last line - is skipped rather than padded.
            if columns is not None and identity is not None \
                    and len(identity) == len(identity_columns) and len(fields) == len(columns):
                rows.append(identity + fields)

        if not reading:
            print(f"Warning: marker line 'Metrics ==' not found in {file_name}.")
            return None
        if columns is None:
            print(f"Warning: no metric blocks found in {file_name}.")
            return None

        df = pd.DataFrame(rows, columns=identity_columns + columns)
        for column in df.columns:
            if column != 'Kernel':
                df[column] = pd.to_numeric(df[column], errors='coerce')

        return df[df['Kernel'].str.contains('SIMD')]
    except FileNotFoundError:
        print(f"The file {file_name} not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

# Example usage
# df = process_file("your_file.txt")
# print(df)
