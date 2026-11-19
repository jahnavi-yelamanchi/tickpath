# Order book contract

Every implementation of `tickpath::book::OrderBook` (include/tickpath/book/concept.hpp)
must behave exactly as below. `ReferenceBook` is the executable form of this document;
differential tests hold every other book to it after every message.

## Model

- One market-wide book: all locates (0..65535), both sides, every live order.
- A level is the set of live orders at one (locate, side, price). It exists while it holds
  at least one order; it disappears when its last order leaves.
- Orders at a level are in time priority: arrival order, oldest first.
- Prices and quantities are integers as on the wire. No floating point.
- The book never matches. A crossed or locked book (bid >= ask) is stored as given.

## Operations and checks

Checks run in the order listed; the first that fails decides the status and the book is
left unchanged, except for Overfill.

| Operation | Checks, in order | Effect on success |
|---|---|---|
| `add(l, id, side, q, p)` | q == 0 -> ZeroQuantity; id live -> DuplicateOrder; no capacity -> CapacityExceeded | order appended at the back of (l, side, p) |
| `execute(l, id, q)` | id not live -> UnknownOrder; order's locate != l -> LocateMismatch; q == 0 -> ZeroQuantity; q > remaining -> Overfill (order removed) | remaining -= q; removed at 0; priority kept otherwise |
| `cancel(l, id, q)` | same as execute | same as execute |
| `remove(l, id)` | id not live -> UnknownOrder; locate != l -> LocateMismatch | order removed |
| `replace(l, old, new, q, p)` | old not live -> UnknownOrder; locate != l -> LocateMismatch; q == 0 -> ZeroQuantity; new live (incl. new == old) -> DuplicateOrder; no capacity -> CapacityExceeded | old removed; new added at back of (l, old side, p), same side and locate |

## ITCH mapping

| Message | Operation |
|---|---|
| A, F | add |
| E | execute |
| C | execute. The execution price is ignored: the order is reduced at its own resting price |
| X | cancel |
| D | remove |
| U | replace (loses time priority even at the same price) |
| all others | no book change |

## Notes

- Ids are unique among live orders only. An id may be reused after its order is gone.
- Overfill removes the order because a feed that executes more than is resting has
  already removed it at the exchange; keeping a negative or clamped remainder would leave
  a phantom order. The status still reports the inconsistency.
- CapacityExceeded is never returned by `ReferenceBook`. Differential tests size fast books
  so that it does not occur; hot-path tests exercise it directly.
