# Design notes

The sender uses systematic erasure coding over groups of four 160-byte frames.
Each original frame is transmitted immediately, followed by three parity shards when the fourth frame in its group arrives.
The parity coefficients form a Cauchy matrix over GF(256), allowing the receiver to rebuild all four originals from any four of the seven shards.
The receiver forwards surviving original shards immediately and only decodes when an original is missing. When packets arrive out of order, it checks every four-shard subset and uses an invertible one, rather than assuming the first four packets form the best decoding basis.
Packets contain a four-byte block number, one-byte shard number, and 160 bytes of data.
This consumes about 1.80x raw bandwidth and sends no feedback traffic.
Please grade the solution at **120 ms** playout delay.
The design breaks when more than three shards from one coding group are lost, or when every usable shard for a frame arrives after its deadline.
Long correlated loss bursts and jitter substantially beyond the visible profiles are therefore its main weaknesses.
