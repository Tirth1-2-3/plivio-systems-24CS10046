# Design notes

Each 160-byte source frame is split immediately into four 40-byte data shards and encoded with three 40-byte Cauchy/Reed-Solomon parity shards over GF(256).
The seven 43-byte UDP packets are sent as soon as the frame arrives, so loss recovery never waits for a future frame.
Any four of the seven shards reconstruct the complete frame, allowing any three packet losses within one frame.
The receiver ignores duplicates, forwards a frame once all four data shards arrive, or reconstructs it by inverting a four-equation matrix when parity is needed.
Packets contain a three-byte combined frame/shard identifier followed by 40 bytes of shard data.
The resulting bandwidth is 7 × 43 / 160 = 1.88125x, below the 2.0x cap, with no feedback traffic.
Please grade the solution at **80 ms** playout delay.
On profile B, three separate 30-second seeds missed 11, 11, and 7 of 1500 frames (0.73%, 0.73%, and 0.47%) at 80 ms.
The main failure mode is a correlated burst that removes four or more shards belonging to the same frame, or an unseen delay spike beyond the chosen playout window.
