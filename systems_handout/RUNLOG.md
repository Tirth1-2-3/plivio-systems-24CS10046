# Experiment log

The final implementation uses immediate per-frame erasure coding: four 40-byte
data shards and three 40-byte parity shards. Its measured bandwidth is 1.97x.

| Design | Profile | Seed | Duration | Delay | Misses | Miss rate | Overhead | Result |
|---|---|---:|---:|---:|---:|---:|---:|---|
| Original 4-frame FEC | B | 1 | 30 s | 120 ms | 16/1500 | 1.07% | 1.80x | INVALID |
| 2-frame, 80-byte shards | B | 1 | 10 s | 80 ms | 34/500 | 6.80% | 1.86x | INVALID |
| 2-frame, 80-byte shards | B | 1 | 30 s | 90 ms | 11/1500 | 0.73% | 1.86x | VALID |
| 2-frame, 80-byte shards | B | 2 | 30 s | 90 ms | 12/1500 | 0.80% | 1.86x | VALID |
| Per-frame, 40-byte shards | B | 1 | 10 s | 80 ms | 3/500 | 0.60% | 1.97x | VALID |
| Per-frame, 40-byte shards | B | 2 | 10 s | 80 ms | 6/500 | 1.20% | 1.97x | INVALID |
| Per-frame, 40-byte shards | B | 1 | 10 s | 81 ms | 2/500 | 0.40% | 1.97x | VALID |
| Per-frame, 40-byte shards | B | 2 | 10 s | 81 ms | 1/500 | 0.20% | 1.97x | VALID |
| Per-frame, 40-byte shards | B | 1 | 10 s | 82 ms | 3/500 | 0.60% | 1.97x | VALID |
| Per-frame, 40-byte shards | B | 2 | 10 s | 82 ms | 2/500 | 0.40% | 1.97x | VALID |
| Per-frame, 40-byte shards | B | 1 | 30 s | 81 ms | 5/1500 | 0.33% | 1.97x | VALID |
| Per-frame, 40-byte shards | B | 2 | 30 s | 81 ms | 9/1500 | 0.60% | 1.97x | VALID |
| Per-frame, 40-byte shards | B | 3 | 30 s | 81 ms | 7/1500 | 0.47% | 1.97x | VALID |
| Per-frame, 40-byte shards | B | 1 | 30 s | 85 ms | 1/1500 | 0.07% | 1.97x | VALID |
| Per-frame, 40-byte shards | B | 2 | 30 s | 85 ms | 1/1500 | 0.07% | 1.97x | VALID |
| Per-frame, 40-byte shards | A | 1 | 10 s | 45 ms | 0/500 | 0.00% | 1.97x | VALID |

## Changes and reasoning

1. The first design encoded four complete frames together. It had good loss
   protection, but parity for the first frame was unavailable for 60 ms.
2. Splitting two frames into four 80-byte shards reduced that coding wait to
   20 ms and made Profile B valid at 90 ms.
3. The final design splits one frame into four 40-byte shards and creates its
   three parity shards immediately. This removes coding wait entirely while
   retaining correction of any three lost packets.
4. At 80 ms the result depended too closely on relay and process scheduling;
   one visible seed crossed the miss cap. At 81 ms, three full-length Profile B
   seeds remained between 0.33% and 0.60%, so **81 ms is the lowest validated
   grading recommendation**.
