# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Fuzzes scripts/summary/summary.py read_summary_tar(): tarfile.open on an
# untrusted archive, and the timing-summary parser run over every member. Half
# of the inputs are used as the archive itself; the other half become the
# contents of the members of a well-formed archive, so the summary parser is
# reached without the fuzzer having to forge a valid tar and gzip stream.

import io
import sys
import tarfile
import zlib
import gzip
import lzma
import atheris
import fuzz_common

fuzz_common.add_script_dir("summary")
with fuzz_common.instrument():
    import summary

EXPECTED = (tarfile.TarError, EOFError, zlib.error, gzip.BadGzipFile, OSError, lzma.LZMAError,
            UnicodeDecodeError)


def BuildArchive(fdp):
    """A well-formed gzip tar archive of 1 to 3 members whose contents come from fdp.

    Lets the summary parser be reached with fuzzed member contents without the
    fuzzer having to forge a valid tar and gzip stream.
    """
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        for i in range(fdp.ConsumeIntInRange(1, 3)):
            content = fdp.ConsumeBytes(fdp.ConsumeIntInRange(0, 4096))
            info = tarfile.TarInfo(name=f"output.{i}")
            info.size = len(content)
            tar.addfile(info, io.BytesIO(content))
    return buf.getvalue()


def TestOneInput(data):
    """Atheris entry point: data is a selector byte, then either a whole archive (even
    selector) or the contents of the members of one (odd selector, BuildArchive).

    Reads it with summary.read_summary_tar(). The archive errors in EXPECTED are a
    clean rejection; any other exception is a finding.
    """
    if not data:
        return
    # The selector is read explicitly, not with ConsumeBool(): which end of the data a
    # FuzzedDataProvider call takes from is not part of its contract.
    rest = data[1:]
    archive = BuildArchive(atheris.FuzzedDataProvider(rest)) if data[0] & 1 else rest
    with fuzz_common.TempFile(archive, ".tgz") as path:
        try:
            summary.read_summary_tar(path)
        except EXPECTED:
            pass


if __name__ == "__main__":
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()
