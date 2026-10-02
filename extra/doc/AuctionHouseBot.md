Auction house bot
-----------------
For testing purposes and low population home servers, *mangos* provides an
auction house bot, which will provide a set of items on the auction houses based
on various configuration settings.

Configuration
-------------
The bot is a module: it is built with `-DBUILD_AHBOT=1` (the default) and loaded
when the first key of `ahbot.conf`, `Enable`, is 1. With `Enable = 0` the server
runs without it, `.ahbot` commands included.

Edit `ahbot.conf` to match your requirements. You can fine tune the amount of
items, the pricing, the range of item quality, and many other settings.

Please note that the default auction house bot settings will disable auction
generation. This is done by intention to prevent putting up a wrong range of
auctions for your server.

Notes
-----
The following rules apply when using the auction house bot:

* If an auction expires, auctions are deleted quietly.
* the bot will not buy its own items, and will not receive mail from the AH or
  receive returned mails.

The bot's auctions belong to the character named by
AuctionHouseBot.CharacterName in ahbot.conf; left empty, they have no owner.

The out-of-process auction house service is a separate module; see
`src/modules/AhService/README.md`. The playerbots' own auction bot
(`src/modules/Bots/ahbot/`) is not built.
