import json
import sys
import tempfile
import unittest
from unittest.mock import mock_open, patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts" / "metrics"))
import perfdashboard as app


class DataTest(unittest.TestCase):
    def test_counter_columns_require_units(self):
        frame = app.pd.DataFrame({
            "ContextID": [1], "SourceID": [2], "ReportReason": [3],
            "Counter[]": [4], "Counter[ ]": [5], "Counter[%]extra": [6],
            " QueryBeginTime[ns] ": [100], "CoreFrequencyMHz[MHz]": [900],
            "Active[%]": [42], "Reads[events]": [12],
            "Bandwidth[GB/s]": [1.5], "Duration[ns] ": [100],
            "Label[%]": ["invalid"],
        })
        self.assertEqual(set(app.counter_cols_from_df(frame)), {
            "Active[%]", "Reads[events]", "Bandwidth[GB/s]", "Duration[ns] ",
        })

    def test_counters_come_from_csv_headers(self):
        frame = app.pd.DataFrame({"Kernel": ["first"], "GlobalInstanceId": [1], "XVE_ACTIVE[%]": [42.], "ContextID": [123]})
        app.df_input1 = frame
        app.df_kernel = frame
        app.df_kernel2 = None
        app.csv1_col_map = app.build_counter_column_map(frame)
        app.df_col_map_dict[id(frame)] = app.csv1_col_map
        app.index_kernels(frame)
        app.counter_multi_choice.options = app.counter_cols_from_df(frame)
        self.assertEqual(app.counter_multi_choice.options, ["XVE_ACTIVE[%]"])
        app.select_kernel_by_gid(1)
        app.counter_multi_choice.value = ["XVE_ACTIVE[%]"]
        app.plot_custom_counters_callback()
        self.assertEqual(len(app.metric_plot_layout.children), 1)
        self.assertEqual(app.metric_plot_layout.children[0].children[0].renderers[0].data_source.data["y"], [42.])

    def read(self, source):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "metrics.csv"
            path.write_text(source)
            return app.read_metrics(str(path))

    def test_stalls_select_trace_kernel_not_gid_order(self):
        frame = app.pd.DataFrame({"Kernel": ["first", "second"], "Offset": ["0x00000100", "0x00000200"]})
        trace = {"traceEvents": [{"name": "second[SIMD32 {1} {2}]",
                                 "args": {"id": "19", "metrics": "http://localhost:8000/second/19"}}]}
        with patch("builtins.open", mock_open(read_data=json.dumps(trace))):
            selected = app.select_eustall_kernel(frame, "19", trace_path="trace.json")
        self.assertEqual(selected["Kernel"].tolist(), ["second"])
        self.assertEqual(app.select_eustall_kernel(frame, kernel_name="first")["Kernel"].tolist(), ["first"])
        with self.assertRaises(ValueError):
            app.select_eustall_kernel(frame, "99")

    def test_hex_offsets(self):
        for value, expected in (("100", "0x0100"), (" 1a0", "0x01a0"),
                                ("0x100", "0x0100"), ("0x000001a8", "0x01a8"), (256, "0x0100")):
            self.assertEqual(app.fmt_ip(value), expected)

    def test_stall_sections(self):
        source = (
            '\nKernel,Pid,KernelId\n"GEMM",7,1\n'
            'Source,Offset,Instruction,Dependency,Active[Events],SbidStall[Events]\n'
            '"gemm.cl:12",0x00000080,"mov (1|M0) r1.0<1>:ud r0.0<0;1,0>:ud","NA",1,2\n'
            '\nKernel,Pid,KernelId\n"GEMM",7,2\n'
            'Source,Offset,Instruction,Dependency,Active[Events],SbidStall[Events]\n'
            '"NA",0x000001a8,"unknown","NA",0,5\n'
        )
        frame = self.read(source)
        self.assertEqual(frame["KernelId"].tolist(), [1, 2])
        self.assertEqual(frame["Instruction"].tolist(), ["mov (1|M0) r1.0<1>:ud r0.0<0;1,0>:ud", "unknown"])
        self.assertEqual(app.fmt_ip(frame.loc[1, "Offset"]), "0x01a8")
        self.assertEqual(frame["SbidStall[Events]"].tolist(), [2, 5])
        chart = app.build_eu_stall_chart(frame)
        self.assertEqual(len(chart.children), 2)

    def test_stall_sections_with_spaces(self):
        source = (
            '\nKernel, Pid, KernelId\n"GEMM", 7, 1\n'
            'Source, Offset, Instruction, Dependency, Active[Events], SbidStall[Events]\n'
            '"gemm.cl:12", 0x00000080, "mov (1|M0) r1.0<1>:ud r0.0<0;1,0>:ud", "NA", 1, 2\n'
        )
        frame = self.read(source)
        self.assertEqual(frame["Pid"].tolist(), [7])
        self.assertEqual(frame["KernelId"].tolist(), [1])
        self.assertEqual(frame["Instruction"].tolist(), ["mov (1|M0) r1.0<1>:ud r0.0<0;1,0>:ud"])
        self.assertEqual(frame["SbidStall[Events]"].tolist(), [2])

    def test_classify_header_row(self):
        for row in ("Kernel,Pid,KernelId\n", "Kernel, Pid, KernelId\n", " Kernel , Pid , KernelId \n"):
            self.assertEqual(app.apm.ClassifyHeaderRow(row), "stall", row)
        for row in ("Kernel, GlobalInstanceId\n", "Kernel ,GlobalInstanceId\n"):
            self.assertEqual(app.apm.ClassifyHeaderRow(row), "time", row)
        for row in ("Kernel\n", "Source,Offset\n", "\n", "kernel,pid,kernelid\n"):
            self.assertIsNone(app.apm.ClassifyHeaderRow(row), row)

    def test_query_sections_of_two_devices(self):
        source = (
            "\n=== Device #0 Metrics ===\n\n"
            'Kernel, GlobalInstanceId\n"kernel,one", 1\nSubDeviceId, GpuTime[ns], CounterA[%]\n'
            "0, 10, 1.5\n1, 11, 2.5\n"
            "\n=== Device #1 Metrics ===\n\n"
            'Kernel, GlobalInstanceId\n"second", 2\nSubDeviceId, GpuTime[ns], CounterB[%]\n0, 20, 99\n'
        )
        frame = self.read(source)
        self.assertEqual(frame["Kernel"].tolist(), ["kernel,one", "kernel,one", "second"])
        self.assertEqual(frame["GlobalInstanceId"].tolist(), [1, 1, 2])
        self.assertEqual(frame["SubDeviceId"].tolist(), [0, 1, 0])
        self.assertEqual(frame["CounterA[%]"].tolist()[:2], [1.5, 2.5])
        self.assertEqual(frame.loc[2, "CounterB[%]"], 99)
        self.assertTrue(app.pd.isna(frame.loc[2, "CounterA[%]"]))

    def test_time_sections_drop_instance_separators(self):
        source = (
            'Kernel, GlobalInstanceId\n"GEMM", 1\nGpuTime[ns], XVE_ACTIVE[%]\n1, 90\n2, 80\n'
            'Kernel, GlobalInstanceId\n"GEMM", 2\nGpuTime[ns], XVE_ACTIVE[%]\n3, 70\n'
        )
        frame = self.read(source)
        self.assertEqual(frame["GlobalInstanceId"].tolist(), [1, 1, 2])
        self.assertEqual(frame["XVE_ACTIVE[%]"].tolist(), [90, 80, 70])

    def test_no_sections(self):
        with self.assertRaises(ValueError):
            self.read("nothing here\n")

if __name__ == "__main__":
    unittest.main()