import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import uniview


class RoutingTest(unittest.TestCase):
    def test_trace_access_logging_keeps_errors(self):
        handler = object.__new__(uniview.TraceLoadingHttpHandler)
        with patch.object(uniview.http.server.SimpleHTTPRequestHandler, "log_request") as log:
            handler.log_request(200, 100)
            handler.log_request(304)
            log.assert_not_called()
            handler.log_request(404)
            log.assert_called_once_with(404, '-')

    def test_dashboard_is_opt_in(self):
        base = ["uniview.py", "-t", "trace.json", "-m", "metrics.csv"]
        with patch.object(sys, "argv", base):
            self.assertFalse(uniview.ParseArguments().dashboard)
        with patch.object(sys, "argv", base + ["--dashboard"]):
            self.assertTrue(uniview.ParseArguments().dashboard)
        with patch.object(sys, "argv", base + ["--pdf"]), contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                uniview.ParseArguments()

    def run_pdf_mode(self, directory, metrics_header, link, options):
        """Run the default PDF mode and return the options it forwards."""
        root = Path(directory)
        trace = root / "trace.json"
        trace.write_text(json.dumps({"traceEvents": [{"args": {"metrics": link}}]}))
        metrics = root / "metrics.csv"
        metrics.write_text(metrics_header + "\nkernel,100,1\n")
        argv = ["uniview.py", "-t", str(trace), "-m", str(metrics)] + options
        with patch.object(sys, "argv", argv), patch.object(uniview, "LoadTrace"), patch.object(
            uniview.apm, "main"
        ) as run, patch.object(uniview.apm, "ParseArguments") as parse:
            self.assertIsNone(uniview.main())
        run.assert_called_once_with(parse.return_value)
        return parse.call_args[0][0], str(metrics)

    def test_pdf_mode_forwards_stall_options(self):
        with tempfile.TemporaryDirectory() as directory:
            dump = Path(directory) / "shaders"
            dump.mkdir()
            forwarded, metrics = self.run_pdf_mode(
                directory, "Kernel,IP[Address],Active[Events]",
                "http://localhost:8000/kernel/9",
                ["-s", str(dump), "-g", "llvm-cxxfilt", "-n", "3"],
            )
        self.assertEqual(forwarded, ["-s", str(dump), "-g", "llvm-cxxfilt", "-n", "3",
                                     "-q", metrics])

    def test_pdf_mode_forwards_config_and_https_links(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "view.txt"
            config.write_text("")
            forwarded, metrics = self.run_pdf_mode(
                directory, "Kernel,GlobalInstanceId,Active[%]",
                "https://localhost:8000/kernel/9", ["-f", str(config)],
            )
        self.assertEqual(forwarded, ["-f", str(config), "-p", metrics])


if __name__ == "__main__":
    unittest.main()