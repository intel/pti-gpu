import base64
import sys
import unittest
from pathlib import Path

import pandas as pd
from bokeh.models import Button, Div, FileInput, Line, TextInput, VBar

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts" / "metrics"))
import perfdashboard as app


class TimeseriesTest(unittest.TestCase):
    def setUp(self):
        app.metric_plot_layout.children = []
        app.counter_multi_choice.value = []
        app.graph_mode_group.active = 0
        app.df_kernel2 = None
        app.df_input1 = pd.DataFrame({
            "Kernel": ["first"] * 3 + ["second"] * 3,
            "GlobalInstanceId": [4] * 3 + [9] * 3,
            "QueryBeginTime[ns]": [100, 110, 140, 900, 920, 980],
            "Active[%]": [0.123456, 20, 0, 30, 40, 50],
            "Stall[%]": [80, 60, 0, 70, 60, 50],
        })
        app.csv1_col_map = app.build_counter_column_map(app.df_input1)
        app.df_col_map_dict.clear()
        app.df_col_map_dict[id(app.df_input1)] = app.csv1_col_map
        app.index_kernels(app.df_input1)
        app.counter_multi_choice.options = ["Active[%]", "Stall[%]"]
        app.select_kernel_by_gid(4)
        app.counter_multi_choice.value = ["Active[%]"]

    def test_samples_and_numeric_axis(self):
        app.plot_custom_counters_callback()
        plot = app.metric_plot_layout.children[0].children[0]
        renderer = plot.renderers[0]
        self.assertEqual(plot.title.text, "first")
        self.assertIsInstance(renderer.glyph, Line)
        self.assertEqual(renderer.data_source.data["x"], [100, 110, 140])
        self.assertEqual(renderer.data_source.data["y"], [0.123456, 20, 0])
        self.assertEqual(plot.xaxis[0].ticker.desired_num_ticks, 8)
        self.assertEqual(plot.xaxis[0].axis_label, "Time (ns)")

    def test_comparison_and_counter_callbacks(self):
        app.compare_kernel_input.value = "9"
        plots = [panel.children[0] for panel in app.metric_plot_layout.children[0].children]
        self.assertEqual(len(plots), 2)
        self.assertEqual(plots[0].renderers[0].data_source.data["x"], [100, 110, 140])
        self.assertEqual(plots[1].renderers[0].data_source.data["x"], [900, 920, 980])
        app.counter_multi_choice.value = ["Active[%]", "Stall[%]"]
        self.assertTrue(all(len(panel.children[0].renderers) == 2 for panel in app.metric_plot_layout.children[0].children))
        app.counter_multi_choice.value = ["Stall[%]"]
        self.assertTrue(all(len(panel.children[0].renderers) == 1 for panel in app.metric_plot_layout.children[0].children))
        app.counter_multi_choice.value = []
        self.assertEqual(app.metric_plot_layout.children, [])

    def test_simplified_controls(self):
        self.assertFalse(list(app.controls.select({"type": Button})))
        self.assertEqual(list(app.controls.select({"type": FileInput})), [app.file_csv_input1])
        self.assertEqual(list(app.controls.select({"type": TextInput})), [app.compare_kernel_input])
        self.assertIs(app.controls.children[0], app.counter_multi_choice)
        self.assertIs(app.controls.children[1], app.compare_kernel_input)
        self.assertNotIn(app.kernel_name, app.controls.children)
        self.assertIn(app.compare_kernel_name, app.controls.children)
        for widget in app.controls.select({"type": Div}):
            self.assertNotIn("Step ", widget.text)
            self.assertNotIn("Plot Custom Counters", widget.text)
            self.assertNotIn("UniTrace Counter Viewer", widget.text)

    def test_csv_upload_populates_counters(self):
        encoded = base64.b64encode(app.df_input1.to_csv(index=False).encode()).decode()
        app.upload_csv_input1("value", "", encoded)
        self.assertIn("Active[%]", app.counter_multi_choice.options)
        self.assertEqual(app.counter_multi_choice.value, [])
        app.counter_multi_choice.value = ["Active[%]"]
        self.assertEqual(app.metric_plot_layout.children, [])
        app.select_kernel_by_gid(4)
        plot = app.metric_plot_layout.children[0].children[0]
        self.assertEqual(plot.renderers[0].data_source.data["x"], [100, 110, 140])

    def test_clearing_comparison_redraws(self):
        app.compare_kernel_input.value = "9"
        self.assertEqual(app.primary_kernel_id, "4")
        self.assertEqual(app.df_kernel["GlobalInstanceId"].tolist(), [4, 4, 4])
        app.compare_kernel_input.value = ""
        plot = app.metric_plot_layout.children[0].children[0]
        self.assertEqual(plot.renderers[0].data_source.data["x"], [100, 110, 140])
        self.assertIsNone(app.df_kernel2)
        self.assertEqual(app.primary_kernel_id, "4")

    def test_invalid_ids_clear_stale_plots(self):
        for value in ("999", "-1", "1.5", "abc", "4e0", ""):
            with self.subTest(value=value):
                app.select_kernel_by_gid(value)
                self.assertFalse(app.selection_valid)
                self.assertEqual(app.metric_plot_layout.children, [])
                self.assertTrue(app.kernel_name.text)
                app.select_kernel_by_gid(4)
                self.assertTrue(app.selection_valid)

    def test_missing_perfetto_id_displays_message(self):
        app.select_kernel_by_gid(2)
        self.assertEqual(app.status_div.text, "No metric data collected or no metrics specified")
        self.assertIn(app.status_div, app.controls.children)
        self.assertEqual(app.layout.children, [app.status_div])
        self.assertFalse(list(app.layout.select({"type": TextInput})))
        self.assertNotIn(app.counter_multi_choice, list(app.layout.references()))
        self.assertFalse(app.selection_valid)
        self.assertIsNone(app.df_kernel)
        self.assertEqual(app.metric_plot_layout.children, [])
        app.select_kernel_by_gid(4)
        self.assertTrue(app.selection_valid)
        self.assertNotIn("No metric data", app.status_div.text)
        self.assertEqual(app.layout.children, [app.controls, app.metric_plot_layout])
        self.assertTrue(app.metric_plot_layout.children)

    def test_duplicate_and_missing_comparison(self):
        for value, message in (("004", "must differ"), ("999", "not found")):
            app.compare_kernel_input.value = value
            self.assertFalse(app.selection_valid)
            self.assertIn(message, app.compare_kernel_name.text)
            self.assertEqual(app.metric_plot_layout.children, [])
        app.compare_kernel_input.value = "9"
        self.assertTrue(app.selection_valid)

    def test_perfetto_selection_and_names(self):
        app.compare_kernel_input.value = "9"
        app.select_kernel_by_gid(9)
        self.assertEqual(app.primary_kernel_id, "9")
        self.assertEqual(app.compare_kernel_input.value, "")
        self.assertIn("second", app.kernel_name.text)
        self.assertEqual(app.df_kernel["GlobalInstanceId"].tolist(), [9, 9, 9])

    def test_kernel_selection_has_no_sample_status(self):
        app.counter_multi_choice.value = []
        app.select_kernel_by_gid(4)
        self.assertTrue(app.selection_valid)
        self.assertEqual(app.df_kernel["GlobalInstanceId"].tolist(), [4, 4, 4])
        self.assertEqual(app.status_div.text, "")
        app.compare_kernel_input.value = "9"
        self.assertEqual(app.df_kernel2["GlobalInstanceId"].tolist(), [9, 9, 9])
        self.assertEqual(app.status_div.text, "")
        app.compare_kernel_input.value = ""
        self.assertIsNone(app.df_kernel2)
        self.assertEqual(app.status_div.text, "")

    def test_comparison_name_matches_plot_truncation(self):
        for name, expected in (
            ("second", "second"),
            ("x" * 50, "x" * 50),
            ("x" * 51, "x" * 50 + "..."),
            ("<kernel>" + "x" * 60, "&lt;kernel&gt;" + "x" * 42 + "..."),
        ):
            with self.subTest(name=name):
                app.compare_kernel_input.value = ""
                app.df_input1.loc[app.df_input1["GlobalInstanceId"] == 9, "Kernel"] = name
                app.compare_kernel_input.value = "9"
                self.assertEqual(app.compare_kernel_name.text, f"Kernel 9: {expected}")
                plot = app.metric_plot_layout.children[0].children[1].children[0]
                short_name = name[:50] + ("..." if len(name) > 50 else "")
                self.assertEqual(plot.title.text, short_name)

    def test_missing_id_column_preserves_sample_plotting(self):
        app.df_input1 = app.df_input1.drop(columns="GlobalInstanceId")
        app.index_kernels(app.df_input1)
        app.plot_custom_counters_callback()
        self.assertTrue(app.compare_kernel_input.disabled)
        self.assertEqual(len(app.metric_plot_layout.children[0].children[0].renderers[0].data_source.data["x"]), 6)

    def test_kernel_execution_timestamps(self):
        timestamps = [
            51982842880, 51984583680, 51986324480,
            51991649280, 51993395200, 51995141120,
        ]
        app.df_input1["QueryBeginTime[ns]"] = timestamps
        app.select_kernel_by_gid(4)
        app.compare_kernel_input.value = "9"
        plots = [panel.children[0] for panel in app.metric_plot_layout.children[0].children]
        for plot, expected in zip(plots, (timestamps[:3], timestamps[3:])):
            self.assertEqual(plot.renderers[0].data_source.data["x"], expected)
            self.assertEqual(plot.xaxis[0].axis_label, "Time (ns)")
        self.assertLess(
            plots[0].renderers[0].data_source.data["x"][-1],
            plots[1].renderers[0].data_source.data["x"][0],
        )

    def test_sample_number_fallback(self):
        app.df_input1 = app.df_input1.drop(columns="QueryBeginTime[ns]")
        app.select_kernel_by_gid(4)
        app.plot_custom_counters_callback()
        plot = app.metric_plot_layout.children[0].children[0]
        self.assertEqual(plot.renderers[0].data_source.data["x"], [1, 2, 3])
        self.assertEqual(plot.xaxis[0].axis_label, "Sample number")

    def test_external_legend(self):
        panel = app.counter_plot_with_legend(app.show_counters_overlaid(
            "single", [("Active[%]", [(100, 42)])],
        ))
        plot, legend = panel.children
        self.assertFalse(plot.legend[0].visible)
        self.assertEqual(legend.labels, ["Active[%]"])
        self.assertEqual(legend.active, [0])
        self.assertTrue(legend.inline)
        self.assertEqual(legend.max_height, 140)
        self.assertEqual(plot.margin, 0)
        self.assertEqual(legend.margin, (0, 0, 0, plot.min_border_left))
        callback = legend.js_property_callbacks["change:active"][0]
        self.assertEqual(callback.args["groups"][0], plot.renderers)

    def test_sampling_mode_uses_metadata_not_sample_count(self):
        self.assertFalse(app.is_query_sampling(app.df_input1.iloc[:1]))
        self.assertFalse(app.is_query_sampling(app.df_input1.assign(ReportId=1)))
        query = app.df_input1.assign(QuerySplitOccurred=0, ReportId=1, ReportsCount=1)
        query.columns = [f" {column.upper()} " for column in query.columns]
        self.assertTrue(app.is_query_sampling(query))

    def test_query_sampling_grouped_bars_and_comparison(self):
        app.df_input1 = app.df_input1.iloc[[0, 3]].copy().assign(
            QuerySplitOccurred=0, ReportId=1, ReportsCount=1,
        )
        app.index_kernels(app.df_input1)
        app.select_kernel_by_gid(4)
        app.counter_multi_choice.value = ["Active[%]", "Stall[%]"]
        app.compare_kernel_input.value = "9"
        panels = app.metric_plot_layout.children[0].children
        for panel, expected in zip(panels, ([0.123456, 80], [30, 70])):
            plot, legend = panel.children
            self.assertEqual(plot.x_range.factors, ["1"])
            self.assertEqual(plot.xaxis[0].axis_label, "Query report")
            self.assertEqual(len(plot.renderers), 2)
            for renderer, value in zip(plot.renderers, expected):
                self.assertIsInstance(renderer.glyph, VBar)
                self.assertEqual(renderer.data_source.data["top"], [value])
            self.assertEqual(legend.labels, ["Active[%]", "Stall[%]"])
            self.assertNotEqual(plot.renderers[0].glyph.x.transform.value,
                                plot.renderers[1].glyph.x.transform.value)

    def test_more_than_ten_counters(self):
        plot = app.show_counters_overlaid("many", [
            (f"counter{index}", [(0, 1), (10, 2)]) for index in range(12)
        ])
        self.assertEqual(len(plot.renderers), 12)


if __name__ == "__main__":
    unittest.main()