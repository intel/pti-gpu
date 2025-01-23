import pandas as pd
import re
from io import StringIO
import math


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
    def __init__(self, config_file):
        config = pd.read_csv(config_file, index_col=0, header=None).squeeze().to_dict()
        self.PlatformName = config['PlatformName']
        self.FP16 = config['FP16_GFLOPS']
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
    html_code = '''
 <!DOCTYPE HTML>
<html>
<head>
    <style>
        .canvasjs-chart-credit {
            display: none !important;
        }
        html, body {
            margin: 0;
            padding: 0;
            height: 100%;
            overflow: hidden; /* Prevents page scrolling */
        }
        #container {
            display: flex; /* Use Flexbox to align the chart and the container */
            height: 100%; /* Use the full height of the window */
        }
        #chartContainer {
            display: flex; /* Use Flexbox to center the chart */
            justify-content: center; /* Center horizontally */
            align-items: center; /* Center vertically */
            position: relative; /* Relative position for the chart container */
            flex-grow: 1; /* Allow the chart container to take the remaining space */
            border: 1px solid #ccc; /* Border to visualize the container */
            transition: flex-grow 0.3s ease; /* Animation when resizing */
        }
        #chartContainer > div {
            width: 100%;
            height: 100%;
        }
        #dataContainer {
            position: relative; /* Relative position for the data container */
            background-color: rgba(255, 255, 255, 0.9); /* Semi-transparent white background */
            padding: 10px;
            border: 1px solid #ccc;
            z-index: 1000; /* Ensure the container is above the chart */
            max-height: 100%; /* Maximum height to align with the chart */
            overflow-y: auto; /* Add scrolling if necessary */
            width: 25%; /* Width of the data container */
            transition: width 0.3s ease, max-height 0.3s ease; /* Animation when resizing */
        }
        .data-section {
            margin-bottom: 10px;
        }
        .collapsed {
            max-height: 30px; /* Reduced height of the container when collapsed */
            overflow: hidden; /* Hide any overflowing content */
        }
    </style>
    <script type="text/javascript" src="https://cdn.canvasjs.com/canvasjs.min.js"></script>
    <script type="text/javascript">
        window.onload = function () {
            var chart = new CanvasJS.Chart("chartContent", {
                exportEnabled: true,
                title: { text: "", fontSize: 50 },
                legend: {
                    cursor: "pointer",
                    fontSize: 14,
                    verticalAlign: "top",
                    horizontalAlign: "left",
                    itemclick: function (e) {
                        e.dataSeries.visible = !e.dataSeries.visible;
                        e.chart.render();
                    }
                },
                toolTip: {   
                    content: "{label}<br/> {y} GF/s"   
                },
                axisX: {
                    logarithmic: true,
                    minimum: 0.001,
                    maximum: 100000,
                    title: "Arithmetic Intensity (Flop/Byte)",
                    titleFontSize: 20,
                    labelFontSize: 16
                },
                axisY: {
                    logarithmic: true,
                    minimum: 0.1,
                    title: "Performance (GFlop/s)",
                    titleFontSize: 20,
                    labelFontSize: 16
                },
                data: [
                    {
                        showInLegend: true, markerType: "none", markerColor: "blue", type: "spline", name: "L3_ROOF", dataPoints: [
                        ]
                    },
                    {
                        showInLegend: true, markerType: "none", markerColor: "red", type: "spline", name: "Gpu_memory_ROOF", dataPoints: [
                        ]
                    },
                    {
                        showInLegend: true, type: "scatter", markerColor: "blue", name: "L3_data", dataPoints: [
                        ]
                    },
                    {
                        showInLegend: true, type: "scatter", markerColor: "red", name: "Gpu_memory_data", dataPoints: [
                        ]
                    }
                ]
            });
            
            chart.render();
            generateDataList(chart);
        }

        function generateDataList(chart) {
            var dataContainer = document.getElementById('dataContainer');
            if (!dataContainer) return;
        
            // Add a "Select/Deselect All" checkbox
            var selectAllCheckbox = document.createElement('input');
            selectAllCheckbox.type = 'checkbox';
            selectAllCheckbox.id = 'selectAll';
            selectAllCheckbox.checked = true; // By default, all checkboxes are checked
        
            var selectAllLabel = document.createElement('label');
            selectAllLabel.htmlFor = 'selectAll';
            selectAllLabel.innerText = 'Select/Deselect All';
        
            var selectAllItem = document.createElement('div');
            selectAllItem.appendChild(selectAllCheckbox);
            selectAllItem.appendChild(selectAllLabel);
            dataContainer.appendChild(selectAllItem);
        
            // Event handler for the "Select/Deselect All" checkbox
            selectAllCheckbox.onchange = function() {
                var checkboxes = dataContainer.querySelectorAll('input[type="checkbox"]');
                checkboxes.forEach(function(checkbox) {
                    if (checkbox.id !== 'selectAll') {
                        checkbox.checked = selectAllCheckbox.checked;
                        checkbox.dispatchEvent(new Event('change'));
                    }
                });
            };
        
            // Only generate checkboxes for L3_data and Gpu_memory_data
            for (var i = 0; i < chart.options.data.length; i++) {
                var dataSeries = chart.options.data[i];
                if (dataSeries.name === "L3_data" || dataSeries.name === "Gpu_memory_data") {
                    var section = document.createElement('div');
                    section.className = 'data-section';
                    
                    var title = document.createElement('h3');
                    title.innerText = dataSeries.name;
                    section.appendChild(title);
        
                    // Sort data points by the custom order tag
                    var sortedDataPoints = dataSeries.dataPoints.slice().sort(function(a, b) {
                        return a.order - b.order;
                    });
        
                    // Log the order of data points for debugging
                    console.log('Order of data points for series:', dataSeries.name);
                    for (var j = 0; j < sortedDataPoints.length; j++) {
                        console.log('Data point', j, ':', sortedDataPoints[j]);
        
                        var dataPoint = sortedDataPoints[j];
                        var checkbox = document.createElement('input');
                        checkbox.type = 'checkbox';
                        checkbox.checked = true; // By default, all checkboxes are checked
                        checkbox.id = dataSeries.name + '_' + j;
        
                        // Use a closure to capture the current dataPoint
                        (function(dp) {
                            checkbox.onchange = function() {
                                // Update data point visibility
                                if (this.checked) {
                                    dp.visible = true;
                                    dp.y = dp.originalY; // Restore original value
                                } else {
                                    dp.visible = false;
                                    dp.y = null; // Set y to null to hide it
                                }
                                
                                chart.render(); // Render the chart after modification
                            };
                        })(dataPoint);
        
                        var label = document.createElement('label');
                        label.htmlFor = checkbox.id;
                        label.innerText = dataPoint.label;
        
                        var listItem = document.createElement('div');
                        listItem.appendChild(checkbox);
                        listItem.appendChild(label);
                        section.appendChild(listItem);
                    }
        
                    dataContainer.appendChild(section);
                }
            }
        }


        function toggleDataContainer() {
            var dataContainer = document.getElementById('dataContainer');
            var chartContainer = document.getElementById('chartContainer');
            if (dataContainer.classList.contains("collapsed")) {
                dataContainer.classList.remove("collapsed"); // Make the container visible
                dataContainer.style.width = "25%"; // Restore the width
                chartContainer.style.flexGrow = "1"; // Reduce the chart width
            } else {
                dataContainer.classList.add("collapsed"); // Collapse the container
                dataContainer.style.width = "5%"; // Reduce the container width
                chartContainer.style.flexGrow = "4"; // Increase the chart width
            }
            setTimeout(function() {
                chart.render(); // Redraw the chart after the transition
            }, 300); // Wait for the transition to complete
        }
    </script>
</head>
<body>
    <div id="container">
        <div id="chartContainer">
            <div id="chartContent"></div> <!-- Container for the chart -->
        </div> 
        <div id="dataContainer">
            <button onclick="toggleDataContainer()">Kernel Names</button> <!-- Button to collapse/expand -->
        </div> 
    </div>
</body>
</html>
    '''

    variable_dict = {
        'FP16': sum(entry.FP16 for entry in entry_list),
        'FP16_XMX': sum(entry.FP16_XMX for entry in entry_list),
        'BF16_XMX': sum(entry.BF16_XMX for entry in entry_list),
        'FP32': sum(entry.FP32 for entry in entry_list),
        'FP64': sum(entry.FP64 for entry in entry_list),
    }
    precision = max(variable_dict, key=variable_dict.get)
    html_code = html_code.replace('title: { text: "", fontSize: 50 }',
                                  'title: { text: "' + test_case + '  ' + precision + '", fontSize: 50 }')
    ai = [0.001 * 2 ** n for n in range(0, int(math.log2(1000000 / 0.001)) + 1)]
    peak_l3 = [min(x * platform.L3, getattr(platform, precision)) for x in ai]
    peak_gpu_memory = [min(x * platform.GPU_MEMORY_BW, getattr(platform, precision)) for x in ai]
    html_peak_l3 = [{'x': x_val, 'y': y_val} for x_val, y_val in zip(ai, peak_l3)]
    html_peak_gpu_memory = [{'x': x_val, 'y': y_val} for x_val, y_val in zip(ai, peak_gpu_memory)]
    html_code = add_to_html(html_code, 'L3_ROOF', html_peak_l3)
    html_code = add_to_html(html_code, 'Gpu_memory_ROOF', html_peak_gpu_memory)
    l3_roof = []
    gpu_memory_roof = []
    min_pixel = 10
    max_pixel = 100
    order = 0
    for index, entry in enumerate(entry_list):

        if (entry.kernel_time * 100 / entry.total_gpu_time > 0.001) and getattr(entry, precision) > 0:
            order += 1
            scale_factor = min(1, math.log(entry.total_gpu_time) / math.log(100))
            size_cercle = min_pixel + (max_pixel - min_pixel) * scale_factor * entry.kernel_time / entry.total_gpu_time
            porcentage_total_time = entry.kernel_time * 100 / entry.total_gpu_time
            new_l3_point = {
                "label": str("{:.2f}".format(porcentage_total_time)) + '  % ' + entry.kernel_name,
                "x": (float(getattr(entry, precision))) / float(entry.L3),
                "y": (float(getattr(entry, precision))) / float(entry.kernel_time),
                'markerSize': float(size_cercle),
                'originalY': (float(getattr(entry, precision))) / float(entry.kernel_time),
                'order': order

            }
            new_gpu_memory_point = {
                "label": str("{:.2f}".format(porcentage_total_time)) + '  % ' + entry.kernel_name,
                "x": float((getattr(entry, precision))) / float(entry.gpu_memory),
                "y": float((getattr(entry, precision))) / float(entry.kernel_time),
                'markerSize': float(size_cercle),
                'originalY': float((getattr(entry, precision))) / float(entry.kernel_time),
                'order': order
            }
            l3_roof.append(new_l3_point)
            gpu_memory_roof.append(new_gpu_memory_point)

    html_code = add_to_html(html_code, 'L3_data', l3_roof)
    html_code = add_to_html(html_code, 'Gpu_memory_data', gpu_memory_roof)
    with open(test_case + ".html", "w+") as file:
        file.write(html_code)


def add_to_html(html_str, sep, data_to_add):
    start_index = html_str.find(sep) + len(sep)
    end_index = html_str.find(']', start_index)
    new_str = ',\n\t\t'.join(map(str, data_to_add))
    html_str = html_str[:end_index] + new_str + html_str[end_index:]
    return html_str


def file_to_json_old(file_name):
    results = []
    start_reading = False  # Flag to control reading
    found_kernel = False  # Flag to indicate if "Kernel" was found
    try:
        with open(file_name, "r") as file:
            for line in file:
                # Start reading after finding the marker line
                if "Metrics ==" in line:
                    start_reading = True

                # If reading started and haven't found Kernel yet
                if start_reading and not found_kernel:
                    if re.search(fr"{'Kernel,'}", line):
                        found_kernel = True
                        clean_line = line.strip()
                        if clean_line not in results:
                            results.append(clean_line)

                # If reading started, found Kernel, and encountering SIMD
                if start_reading and found_kernel and re.search(fr"{'SIMD'}", line):
                    clean_line = line.strip()
                    if clean_line not in results:
                        results.append(clean_line)

        # Handle the case where the marker line is not found
        if not start_reading:
            print("Warning: Marker line ' Metrics ==' not found in the file.")

        data_string = "\n".join(results)
        data_io = StringIO(data_string)
        df = pd.read_csv(data_io)
        return df
    except FileNotFoundError:
        print(f"The file {file_name} not found.")
    except Exception as e:
        print(f"An error occurred : {e}")


def file_to_json(file_name):
    results = []
    start_reading = False  # Flag to control reading
    found_kernel = False  # Flag to indicate if "Kernel" was found
    unique_lines = set()  # Use a set for faster membership testing

    try:
        with open(file_name, "r") as file:
            for line in file:
                # Start reading after finding the marker line
                if "Metrics ==" in line:
                    start_reading = True

                # If reading started and haven't found Kernel yet
                if start_reading and not found_kernel:
                    if 'Kernel,' in line:
                        found_kernel = True
                        clean_line = line.strip()
                        if clean_line not in unique_lines:
                            unique_lines.add(clean_line)
                            results.append(clean_line)

                # If reading started, found Kernel, and encountering SIMD
                if start_reading and found_kernel and 'SIMD' in line:
                    clean_line = line.strip()
                    if clean_line not in unique_lines:
                        unique_lines.add(clean_line)
                        results.append(clean_line)

        # Handle the case where the marker line is not found
        if not start_reading:
            print("Warning: Marker line 'Metrics ==' not found in the file.")

        data_string = "\n".join(results)
        data_io = StringIO(data_string)
        df = pd.read_csv(data_io)
        return df
    except FileNotFoundError:
        print(f"The file {file_name} not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

# Example usage
# df = process_file("your_file.txt")
# print(df)
