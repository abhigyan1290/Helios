"""Exercise seed parsing, CSV identity, and repeatability at the CLI boundary."""

import csv
import io
import os
from pathlib import Path
import subprocess
import unittest


SIMULATOR = Path(os.environ.get("HELIOS_SIM", "build/helios_sim")).resolve()


@unittest.skipUnless(SIMULATOR.is_file(), "Build helios_sim before CLI tests")
class BenchmarkCliTest(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run([str(SIMULATOR), *args], capture_output=True, text=True,
                              timeout=60)

    def test_paired_csv_is_repeatable_and_has_seed_labels(self):
        first = self.run_cli("--compare-seeds", "2")
        self.assertEqual(first.returncode, 0, first.stderr)
        second = self.run_cli("--compare-seeds", "2")
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(first.stdout, second.stdout)
        rows = list(csv.DictReader(io.StringIO(first.stdout)))
        self.assertEqual(len(rows), 20)
        for i in range(0, len(rows), 2):
            fifo, sjf = rows[i:i + 2]
            self.assertEqual((fifo["scheduler"], sjf["scheduler"]), ("fifo", "sjf"))
            for field in ("seed", "workload", "job_count"):
                self.assertEqual(fifo[field], sjf[field])
            self.assertIn(int(fifo["seed"]), (0, 1))

    def test_invalid_seed_requests_fail_cleanly(self):
        for args in (("--compare-seeds", "0"), ("--compare-seeds", "-1"),
                     ("--compare-scale", "abc"), ("--compare-scale", "1", "extra"),
                     ("--compare-seeded", "generated-small"),
                     ("--compare-seeded", "missing", "0"),
                     ("--compare-seeded", "generated-small", "4294967296")):
            with self.subTest(args=args):
                result = self.run_cli(*args)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")
                self.assertIn("helios_sim:", result.stderr)

    def test_individual_seed_zero_is_supported(self):
        result = self.run_cli("--compare-seeded", "generated-small", "0")
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        self.assertEqual(len(rows), 2)
        self.assertTrue(all(row["seed"] == "0" and row["job_count"] == "25" for row in rows))

    def test_tail_summary_matches_exported_per_job_waits(self):
        comparison = self.run_cli("--compare", "generated-small")
        self.assertEqual(comparison.returncode, 0, comparison.stderr)
        summaries = list(csv.DictReader(io.StringIO(comparison.stdout)))
        for summary in summaries:
            policy = summary["scheduler"]
            exported = self.run_cli(f"--demo-{policy}", "generated-small")
            self.assertEqual(exported.returncode, 0, exported.stderr)
            waits = sorted(int(row["wait_time"]) for row in
                           csv.DictReader(io.StringIO(exported.stdout)))
            rank = len(waits) - len(waits) // 20
            self.assertEqual(int(summary["p95_wait_time"]), waits[rank - 1])
            self.assertEqual(int(summary["max_wait_time"]), max(waits))
        suite = self.run_cli("--compare-suite")
        self.assertEqual(suite.returncode, 0, suite.stderr)
        self.assertTrue(all("p95_wait_time" in row and "max_wait_time" in row
                            for row in csv.DictReader(io.StringIO(suite.stdout))))

    def test_additional_baselines_and_per_job_exports_agree(self):
        result = self.run_cli("--compare-baselines", "generated-medium", "100", "42")
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        self.assertEqual([row["scheduler"] for row in rows],
                         ["fifo", "oracle_sjf", "estimated_sjf", "random", "easy_backfill"])
        for row in rows:
            self.assertEqual(row["job_count"], "100")
            self.assertEqual(row["estimate_percent"], "100")
            self.assertEqual(row["random_seed"], "42")
            self.assertLessEqual(int(row["p95_wait_time"]), int(row["max_wait_time"]))
        for field in ("makespan", "total_wait_time", "average_wait_time", "p95_wait_time", "max_wait_time"):
            self.assertEqual(rows[1][field], rows[2][field])
        for command, parameter, row_index in (("--demo-random", "42", 3),
                                               ("--demo-estimated-sjf", "100", 2),
                                               ("--demo-backfill", "100", 4)):
            exported = self.run_cli(command, "generated-medium", parameter)
            self.assertEqual(exported.returncode, 0, exported.stderr)
            jobs = list(csv.DictReader(io.StringIO(exported.stdout)))
            self.assertEqual(len(jobs), 100)
            self.assertEqual(sum(int(job["wait_time"]) for job in jobs),
                             int(rows[row_index]["total_wait_time"]))

    def test_step_trace_shows_explicit_starts_waits_and_termination(self):
        result = self.run_cli("--demo-step", "fifo-blocking")
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        self.assertEqual([row["job_id"] for row in rows if row["action"] == "start"], ["1", "2", "3"])
        self.assertEqual(sum(int(row["completed_count"]) for row in rows), 3)
        self.assertEqual(rows[-1]["to_time"], "13")
        self.assertEqual(rows[-1]["terminated"], "1")
        for row in rows:
            if row["action"] == "start":
                self.assertEqual(row["from_time"], row["to_time"])
            else:
                self.assertGreater(int(row["to_time"]), int(row["from_time"]))

    def test_new_commands_validate_arguments(self):
        for args in (("--demo-step",), ("--demo-random", "light-overlap", "-1"),
                     ("--demo-backfill", "light-overlap", "0"),
                     ("--demo-estimated-sjf", "light-overlap", "2147483648"),
                     ("--compare-baselines", "light-overlap", "100")):
            with self.subTest(args=args):
                result = self.run_cli(*args)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")
                self.assertIn("helios_sim:", result.stderr)


if __name__ == "__main__":
    unittest.main()
