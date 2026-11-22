"""Check paired sample validation and aggregation independently of chart rendering."""

import csv
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from plot_benchmarks import load_results


class BenchmarkAggregationTest(unittest.TestCase):
    def load(self, rows):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "suite.csv"
            with path.open("w", newline="", encoding="utf-8") as output:
                writer = csv.writer(output)
                writer.writerow(["workload", "seed", "scheduler", "job_count", "makespan",
                                 "total_wait_time", "average_wait_time", "p95_wait_time",
                                 "max_wait_time"])
                writer.writerows(rows)
            return load_results(path)

    def test_mean_and_sample_standard_deviation(self):
        profiles, summaries = self.load([
            ["example", 0, "fifo", 2, 10, 4, 2, 4, 4],
            ["example", 0, "sjf", 2, 8, 2, 1, 2, 2],
            ["example", 1, "fifo", 2, 14, 12, 6, 10, 10],
            ["example", 1, "sjf", 2, 10, 6, 3, 4, 4],
        ])
        self.assertEqual(profiles, ["example"])
        self.assertEqual(summaries[0]["seed_count"], 2)
        self.assertEqual(summaries[0]["average_wait_time_mean"], 4)
        self.assertAlmostEqual(summaries[0]["average_wait_time_stddev"], 8 ** 0.5)
        self.assertEqual(summaries[1]["makespan_mean"], 9)
        self.assertEqual(summaries[0]["p95_wait_time_mean"], 7)
        self.assertAlmostEqual(summaries[0]["max_wait_time_stddev"], 18 ** 0.5)

    def test_invalid_pairs_are_rejected(self):
        valid = [["example", 0, "fifo", 2, 10, 4, 2, 4, 4],
                 ["example", 0, "sjf", 2, 8, 2, 1, 2, 2]]
        for rows in ([], valid[:1], valid + valid[:1],
                     [valid[0], ["example", 0, "sjf", 3, 8, 3, 1, 2, 2]],
                     [valid[0], ["example", 0, "sjf", 2, 8, 2, "nan", 2, 2]],
                     [valid[0], ["example", 0, "sjf", 2, 8, 2, 1, 3, 2]],
                     [valid[0], ["example", 0, "sjf", 2, 8, 2, 1, 2, "inf"]]):
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                self.load(rows)

    def test_tail_metrics_aggregate_per_seed_rather_than_pooling_jobs(self):
        # Seed maxima of 4 and 10 have a mean of 7, not a pooled maximum of 10.
        _, summaries = self.load([
            ["example", 0, "fifo", 2, 10, 4, 2, 4, 4],
            ["example", 0, "sjf", 2, 8, 2, 1, 2, 2],
            ["example", 1, "fifo", 2, 14, 12, 6, 10, 10],
            ["example", 1, "sjf", 2, 10, 6, 3, 4, 4],
        ])
        self.assertEqual(summaries[0]["max_wait_time_mean"], 7)
        self.assertEqual(summaries[0]["p95_wait_time_mean"], 7)

    def test_p95_below_average_is_valid_for_outlier_heavy_results(self):
        _, summaries = self.load([
            ["example", 0, "fifo", 20, 1001, 1000, 50, 0, 1000],
            ["example", 0, "sjf", 20, 1001, 1000, 50, 0, 1000],
        ])
        self.assertEqual(summaries[0]["p95_wait_time_mean"], 0)
        self.assertEqual(summaries[0]["max_wait_time_mean"], 1000)
        self.assertEqual(summaries[0]["max_wait_time_stddev"], 0)

    def test_missing_tail_columns_require_regeneration(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "old.csv"
            path.write_text("workload,scheduler,job_count,makespan,total_wait_time,average_wait_time\n",
                            encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "regenerate benchmark CSV"):
                load_results(path)


if __name__ == "__main__":
    unittest.main()
