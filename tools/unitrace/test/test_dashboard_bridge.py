import contextlib
import http.client
import io
import socket
import sys
import tempfile
import threading
import unittest
import warnings
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

from bokeh.client import pull_session
from bokeh.models import Div
from bokeh.util.warnings import BokehUserWarning
from tornado.ioloop import IOLoop

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts" / "metrics"))
import perfdashboard
from perfdashboard import (check_port_available, create_server, prewarm_session,
                           start_server_process, stop_owned_process)


class BridgeTest(unittest.TestCase):
    def test_duplicate_reference_warning_filter(self):
        args = SimpleNamespace(input="metrics.csv", trace=None, eustall=True)
        with warnings.catch_warnings(record=True) as caught:
            warnings.simplefilter("always")
            with patch("perfdashboard.Server"), patch("perfdashboard.ScriptHandler"):
                create_server(args)
            for reference in ("p1358", "p1540", "p1722"):
                warnings.warn(f"reference already known '{reference}'", BokehUserWarning)
            warnings.warn("Other Bokeh warning", BokehUserWarning)
            warnings.warn("reference already known 'p1358'", UserWarning)
        self.assertEqual([str(item.message) for item in caught], [
            "Other Bokeh warning", "reference already known 'p1358'",
        ])

    def test_occupied_port_is_not_killed(self):
        with patch("perfdashboard.socket.create_connection") as connection, patch(
            "perfdashboard.subprocess.Popen"
        ) as spawn:
            with self.assertRaisesRegex(OSError, "Port 8000 is in use"):
                check_port_available(8000)
            connection.return_value.close.assert_called_once()
            spawn.assert_not_called()

    def test_owned_process_cleanup(self):
        proc = MagicMock()
        proc.poll.return_value = None
        stop_owned_process(proc)
        proc.terminate.assert_called_once()
        proc.wait.assert_called_once_with(timeout=5)
        proc.poll.return_value = 0
        stop_owned_process(proc)
        proc.terminate.assert_called_once()

    def test_prewarm_is_quiet(self):
        output = io.StringIO()
        with patch("urllib.request.urlopen", return_value=MagicMock()), contextlib.redirect_stdout(output):
            prewarm_session()
        self.assertEqual(output.getvalue(), "")

    def test_server_process_command(self):
        proc = MagicMock(**{"poll.return_value": None})
        output = io.StringIO()
        with patch("perfdashboard.check_port_available") as check, patch(
            "perfdashboard.subprocess.Popen", return_value=proc
        ) as spawn, patch("perfdashboard.socket.create_connection"), patch(
            "perfdashboard.time.sleep"
        ), patch("perfdashboard.prewarm_session") as prewarm, contextlib.redirect_stdout(output):
            started = start_server_process("metrics.csv", trace="trace.json", eustall=True)
        self.assertIs(started, proc)
        check.assert_called_once_with(8000)
        prewarm.assert_called_once_with(8000)
        command = spawn.call_args[0][0]
        self.assertEqual(command[1], str(Path(perfdashboard.__file__).resolve()))
        self.assertEqual(command[2:], [str(Path("metrics.csv").resolve()),
                                       "--trace", str(Path("trace.json").resolve()), "--eustall"])
        self.assertIn("http://localhost:8000/perfdashboard", output.getvalue())

    def test_idle_connection_does_not_block_kernel_redirect(self):
        with tempfile.TemporaryDirectory() as directory:
            metrics = Path(directory) / "metrics.csv"
            metrics.write_text("Kernel,GlobalInstanceId,QueryBeginTime[ns],Counter\nkernel,9,100,42\nother,4,200,12\n")
            args = SimpleNamespace(input=str(metrics), trace=None, eustall=False)
            loop = IOLoop()
            server = create_server(args, loop, port=0)
            server.start()
            worker = threading.Thread(target=loop.start)
            worker.start()
            idle = socket.create_connection(("localhost", server.port), timeout=2)
            try:
                for path, expected in (("/%22kernel%22/9", 302),
                                       ("/%22kernel%2Fwith%25slash%22/9", 302),
                                       ("/perfdashboard?gid=9", 200),
                                       ("/kernel/invalid", 400),
                                       ("/favicon.ico", 200)):
                    with self.subTest(path=path):
                        client = http.client.HTTPConnection("localhost", server.port, timeout=10)
                        try:
                            client.request("GET", path)
                            response = client.getresponse()
                            self.assertEqual(response.status, expected)
                            if expected == 302:
                                self.assertEqual(
                                    response.getheader("Location"),
                                    "/perfdashboard?gid=9&kernel="
                                    + ("kernel%2Fwith%25slash" if "with" in path else "kernel"),
                                )
                            response.read()
                        finally:
                            client.close()
                session = pull_session(url=f"http://localhost:{server.port}/perfdashboard",
                                       arguments={"gid": "9"})
                try:
                    messages = [model.text for model in session.document.select({"type": Div})]
                    self.assertFalse(any("samples selected" in text for text in messages), messages)
                    self.assertFalse(any("Kernel 9:" in text for text in messages), messages)
                finally:
                    session.close()
                session = pull_session(url=f"http://localhost:{server.port}/perfdashboard",
                                       arguments={"gid": "2"})
                try:
                    messages = [model.text for model in session.document.select({"type": Div})]
                    self.assertIn("No metric data collected or no metrics specified", messages)
                    root = session.document.roots[0]
                    self.assertEqual(len(root.children), 1)
                    self.assertIsInstance(root.children[0], Div)
                    self.assertEqual(root.children[0].text,
                                     "No metric data collected or no metrics specified")
                finally:
                    session.close()
            finally:
                idle.close()
                loop.add_callback(server.stop)
                loop.add_callback(loop.stop)
                worker.join(timeout=5)
                loop.close(all_fds=True)
            self.assertFalse(worker.is_alive())


if __name__ == "__main__":
    unittest.main()