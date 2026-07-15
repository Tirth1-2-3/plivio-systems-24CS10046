# Experiment log

All experiments used the Reed-Solomon-style 4-data/3-parity implementation in
`sender.cpp` and `receiver.cpp`. The wire packet is 165 bytes, so seven packets
per four 160-byte source frames use 1.80x the raw bandwidth.

| Profile | Seed | Duration | Delay | Misses | Miss rate | Overhead | Result | Observation |
|---|---:|---:|---:|---:|---:|---:|---|---|
| A (mild) | 1 | 10 s | 100 ms | 0/500 | 0.00% | 1.80x | VALID | Immediate systematic packets plus parity comfortably handle mild loss. |
| B (moderate) | 1 | 10 s | 100 ms | 4/500 | 0.80% | 1.80x | VALID | Valid, but too close to the 1% limit to recommend. |
| B (moderate) | 1 | 30 s | 120 ms | 8/1500 | 0.53% | 1.80x | VALID | Full-length timing result; safer margin than 100 ms. |
| B (moderate) | 2 | 10 s | 120 ms | 2/500 | 0.40% | 1.80x | VALID | A second loss pattern remained valid. |
| B (moderate) | 1 | 30 s | 120 ms | 10/1500 | 0.67% | 1.80x | VALID | Final validation after robust subset-based decoding. |

## Changes and reasoning

1. Replaced one-shot forwarding with systematic erasure coding, because a
   resend request and reply would each suffer the hostile network delay/loss.
2. Chose four data and three parity shards. The receiver can reconstruct a
   group after any four of its seven packets arrive, including up to three
   erasures, while staying below the 2.0x bandwidth cap.
3. Kept data packets systematic and sent them immediately. Frames that survive
   the network do not wait for the rest of their coding group.
4. Tested 100 ms first. It worked on both visible profiles, but profile B's
   0.80% result left little room for unseen random patterns.
5. Selected 120 ms for grading after a full profile-B run and another seed both
   stayed below the 1% miss limit.
6. The receiver now tries every available four-shard subset before decoding.
   This avoids treating a singular, reordered subset as an unrecoverable block.
