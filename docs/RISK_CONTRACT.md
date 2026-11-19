# Risk check contract

`tickpath::risk::RiskCheck` (include/tickpath/risk/risk_check.hpp). Integers only.

## Checks, in order

The first failing check is the verdict.

| # | Verdict | Rejects when |
|---|---|---|
| 1 | KillSwitch | kill switch engaged |
| 2 | ZeroQuantity | `qty == 0` |
| 3 | MaxOrderSize | `qty > max_order_qty` |
| 4 | NoReference | buy: no best ask; sell: no best bid |
| 5 | PriceCollar | buy: `price * 10000 > ask * (10000 + bps)`; sell: `price * 10000 < bid * (10000 - bps)`, lower bound 0 when `bps >= 10000` |
| 6 | PositionLimit | `abs(position + signed qty) > max_abs_position`, signed qty = +qty for buy, -qty for sell |
| - | Accept | none of the above |

## Notes

- Collar reference is the far side: a buy is compared with the best offer, a sell with the
  best bid, so the check bounds how far an order may cross the market. Exactly on the
  boundary is accepted.
- Collar arithmetic must not overflow for any `Price` (uint32) and any `collar_bps` up to
  `max_collar_bps`: `2^32 * (10000 + 10^6)` fits in 64 bits.
- Position counts fills only (`on_fill`), not open orders. Worst case is assumed: the
  order fills completely.
- `check()` does not change state. Releasing the kill switch restores normal checks.
