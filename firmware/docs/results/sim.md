| Test | Check | Result |
| --- | --- | --- |
| IT-01 boot + free-running flash at night | 2 boot flashes within 0.25 s (got 26 flashes, gap 0.165 s) | pass |
| IT-01 boot + free-running flash at night | period 2.496..2.496 s, expected 2.496 s (78 ticks x 32 ms) | pass |
| IT-01 boot + free-running flash at night | flash width 15.0..15.0 ms, expected 15 ms | pass |
| IT-01 boot + free-running flash at night | CPU awake 1.053 % of the time at night incl. 15 ms flashes (budget 2 %) | pass |
| IT-01 boot + free-running flash at night | stack low-water 0x97: 8 bytes used, 51 bytes spare | pass |
| IT-02 daylight: no flashing | 0 flashes in 59 s of daylight (got 0) | pass |
| IT-02 daylight: no flashing | CPU awake 0.0150 % by day, after boot (target < 0.05 %) | pass |
| IT-03 dusk: starts flashing | quiet before dusk | pass |
| IT-03 dusk: starts flashing | first flash 13.90 s after dusk (sim limit: 2 x 8.2 s + one cycle) | pass |
| IT-04 dawn: stops flashing | flashing before dawn (6) | pass |
| IT-04 dawn: stops flashing | no flashes from 1.5 s after dawn (got 0) | pass |
| IT-05 low supercap: silent until recovered | no flashes (not even boot) at 2.1 V (got 0) | pass |
| IT-05 low supercap: silent until recovered | flashing resumes 12.31 s after cap reaches 3.0 V (sim limit: 2 x 8.2 s + one cycle) | pass |
| IT-06 supercap sags at night: stops | no flashes from 1.5 s after the cap falls to 2.2 V (got 0) | pass |
| IT-07 hysteresis: 2.5 V boot stays silent | 2.5 V is between 2.3 V and 2.7 V: no flashes after a cold start (got 0) | pass |
| IT-08 locks to a neighbour 8 % faster | neighbour every 2.30 s: last 13 flashes all within one tick after it (13 matched, worst 28 ms) | pass |
| IT-09 locks to a neighbour 20 % faster | neighbour every 2.00 s: last 15 flashes all within one tick after it (15 matched, worst 27 ms) | pass |
| IT-10 light right after own flash is ignored | flash times identical to free-running (10 vs 10 flashes) | pass |
| IT-11 steady light (streetlight) does not cause runaway | still flashes at the nominal period under constant light (min 2.496 s) | pass |
| IT-12 tap switch: immediate flash, rattles held off | flash 0 ms after tap (limit one tick) | pass |
| IT-12 tap switch: immediate flash, rattles held off | second tap within 1 s ignored (0 flashes) | pass |
| IT-13 tap by day: one 'alive' flash | exactly one flash right after the tap, then quiet | pass |
| IT-14 two real boards see each other and lock | from 40 s on, every flash of A has one from B within one tick (8/8, worst 14 ms) | pass |
