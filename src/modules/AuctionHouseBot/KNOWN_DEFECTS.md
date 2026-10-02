# Auction house bot module: known defects

Open defects in `src/modules/AuctionHouseBot/`, from the static review of the
auction module split (2026-10-03). Numbers are the review's. Line numbers are
as of the review and may drift. They predate the module split; the split moved
the code without changing it.

Fixed since the review: #18, the seller's out-of-range price conversion
(`SetPricesOfItem` now works in 64 bits and saturates).

The review's #8 and the first half of #7 exist only alongside the AH service
module (they are about standing down for its worker) and are listed in
`src/modules/AhService/KNOWN_DEFECTS.md`.

## 7. `.ahbot reload` cannot turn the seller or the buyer off

- Severity: bug
- Where: `AuctionHouseBot.cpp:2599` and `:2610` (`InitializeAgents`, called by
  `ReloadAllConfig`), `AuctionBotSeller::Update`, `AuctionBotBuyer::Update`

`InitializeAgents` rebuilds an agent only when its flag is 1; when
`AuctionHouseBot.Seller.Enabled` or `AuctionHouseBot.Buyer.Enabled` has become
0, the existing agent is kept. Neither agent looks at that flag again:
`AuctionBotSeller::Update` gates on the house's item ratio, and
`AuctionBotBuyer::Update` on the per-house `Buyer.<House>.Enabled` flags. After
setting `Seller.Enabled = 0` (or `Buyer.Enabled = 0`) and `.ahbot reload`, the
bot keeps selling (or buying) until the server restarts.

Fix direction: in `InitializeAgents`, delete and null an agent whose flag is 0.
