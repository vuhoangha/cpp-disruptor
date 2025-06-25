# Sequence cache line 128 - Yield wait strategy
NUM_EVENTS_PER_PRODUCER = 50'000'000
- 1P1C: 10M
- 2P1C: 13M
- 3P1C: 13.5M
- 4P1C: 18.5M
- 5P1C: 21M


# Sequence cache line 128 - Adaptive wait strategy
NUM_EVENTS_PER_PRODUCER = 50'000'000
- 1P1C: 37M
- 2P1C: 15M
- 3P1C: 19M
- 4P1C: 23M
- 5P1C: 27M


# Sequence cache line 64 - Adaptive wait strategy
NUM_EVENTS_PER_PRODUCER = 50'000'000
- 1P1C: 65M
- 2P1C: 15M
- 3P1C: 19M
- 4P1C: 21M
- 5P1C: 27M


# Lmax Disruptor
NUM_EVENTS_PER_PRODUCER = 50'000'000
- 1P1C: 7M
- 2P1C: 10M
- 3P1C: 11.5M
- 4P1C: 16M
- 5P1C: 19M


# Sequence cache line 128 - Yield wait strategy
NUM_EVENTS_PER_PRODUCER = 500'000'000
- 1P1C: 600M
- 1P2C: 350M
- 1P3C: 150M
- 1P4C: 30M
- 1P5C: 25M


# Sequence cache line 128 - Adaptive wait strategy
NUM_EVENTS_PER_PRODUCER = 500'000'000
- 1P1C: 480M
- 1P2C: 121M
- 1P3C: 80M
- 1P4C: 16M
- 1P5C: 15M


# Lmax Disruptor
NUM_EVENTS_PER_PRODUCER = 500'000'000
- 1P1C: 350M
- 1P2C: 18M
- 1P3C: 9M
- 1P4C: 9.5M
- 1P5C: 10M