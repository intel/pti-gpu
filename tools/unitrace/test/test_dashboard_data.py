import io
import json
import sys
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

    def test_stalls_select_trace_kernel_not_gid_order(self):
        frame = app.pd.DataFrame({"Kernel": ["first", "second"], "IP[Address]": ["100", "200"]})
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
                                ("0x100", "0x0100"), (256, "0x0100")):
            self.assertEqual(app.fmt_ip(value), expected)
        frame = app.read_csv_smart(io.StringIO("Kernel,IP[Address],Active[Events]\nkernel,100,1\n"))
        self.assertEqual(app.fmt_ip(frame.iloc[0]["IP[Address]"]), "0x0100")

    def test_different_section_schemas(self):
        source = "=== Device #0 Metrics ===\nKernel,GlobalInstanceId,CounterA\nfirst,1,10\n=== Device #1 Metrics ===\nKernel,GlobalInstanceId,CounterB\nsecond,2,99\n"
        frame = app.read_csv_smart(io.StringIO(source))
        self.assertEqual(frame.loc[0, "CounterA"], 10)
        self.assertEqual(frame.loc[1, "CounterB"], 99)
        self.assertTrue(app.pd.isna(frame.loc[1, "CounterA"]))

    def test_repeated_header_and_quoted_kernel(self):
        source = 'Kernel, GlobalInstanceId, CounterA\n"kernel,one", 1, 10\nKernel, GlobalInstanceId, CounterA\nsecond, 2, 20\n'
        frame = app.read_csv_smart(io.BytesIO(source.encode()))
        self.assertEqual(frame["Kernel"].tolist(), ["kernel,one", "second"])
        self.assertEqual(frame["GlobalInstanceId"].tolist(), [1, 2])

    def test_sample_files(self):
        root = Path(__file__).resolve().parents[1] / "scripts" / "data"
        for filename in ("computebasics.metrics.2543699.csv", "stalls.metrics.1908650.csv"):
            if not (root / filename).exists():
                continue
            with (root / filename).open("rb") as source:
                self.assertFalse(app.read_csv_smart(source).empty)


if __name__ == "__main__":
    unittest.main()