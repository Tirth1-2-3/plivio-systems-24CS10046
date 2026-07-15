# Design notes

Each 160-byte source frame is split immediately into five 32-byte data shards and encoded with four 32-byte Cauchy/Reed-Solomon parity shards over GF(256).
The nine 35-byte UDP packets are sent as soon as the frame arrives, so loss recovery never waits for a future frame.
Any five of the nine shards reconstruct the complete frame, allowing any four packet losses within one frame.
The receiver ignores duplicates, joins all five data shards when available, or reconstructs missing shards by inverting a five-equation matrix.
Packets contain a three-byte combined frame/shard identifier followed by 32 bytes of shard data.
The resulting bandwidth is 9 × 35 / 160 = 1.96875x, below the 2.0x cap, with no feedback traffic.
Please grade the solution at **78 ms** playout delay.
On profile B, three separate 30-second seeds missed 11, 9, and 9 of 1500 frames (0.73%, 0.60%, and 0.60%) at 78 ms.
The main failure mode is a correlated burst that removes five or more shards belonging to one frame, or an unseen delay spike beyond the chosen playout window.
