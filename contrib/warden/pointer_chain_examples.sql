-- ============================================================================
-- POINTER_CHAIN_CHECK (type 244 / 0xF4) example seeds for the `warden` table.
--
-- Wire format on the client side is identical to MEM_CHECK (243 / 0xF3); the
-- server walks a multi-hop pointer dereference chain across consecutive Warden
-- cycles and `memcmp`-validates the bytes at the final resolved address.
--
-- Schema reminder (see WardenCheckMgr::LoadWardenChecks):
--   id        uint16   -- check id
--   build     uint16   -- client build (vanilla 1.12.1 enUS = 5875)
--   type      uint8    -- 244 for POINTER_CHAIN_CHECK
--   data      string   -- unused for this type (leave empty)
--   result    string   -- hex of the EXPECTED final-hop bytes (length must
--                         match the `length` column)
--   address   uint32   -- chain base address (32-bit, x86)
--   length    uint8    -- bytes to read at the final hop (1..N)
--   str       string   -- comma-separated hex offsets, e.g. '0x10,0x24,0x8'.
--                         Empty string = zero hops (degenerate single-read).
--                         Each offset is added to the pointer dereferenced at
--                         the previous hop to produce the next hop's address.
--                         A leading '!' flips the terminal compare: instead of
--                         "fail on mismatch" (verify expected bytes), the row
--                         becomes "fail on match" (detect a forbidden cheat
--                         signature, e.g. PQR landing in a dynamically
--                         resolved memory region). The '!' is consumed before
--                         the offsets are parsed, so '!0x4,0x8' is a 2-hop
--                         chain in signature-detect mode.
--   comment   string   -- free text
--
-- Walk semantics for offsets `o1, o2, ..., oN` and base `B`:
--     hop 0 (intermediate): read 4 bytes at B          -> P0
--     hop 1 (intermediate): read 4 bytes at P0 + o1    -> P1
--     hop 2 (intermediate): read 4 bytes at P1 + o2    -> P2
--     ...
--     hop N (terminal):     read `length` bytes at P_{N-1} + oN
--
-- IMPORTANT — addresses below are templates calibrated for the canonical
-- vanilla 1.12.1 enUS WoW.exe (build 5875,
-- MD5 5fea0d4eed95002f436200a16a4f4795). Image base 0x00400000. They are
-- consistent with the function addresses already used in WardenWin.cpp's
-- module-init block (SFileOpenFile = 0x002485F0 + image base, etc.) and with
-- offsets widely documented in vanilla emulation/cheat communities (e.g.
-- ownedcore vanilla reverse-engineering threads, public vanilla bot/cheat
-- repos).
--
-- BEFORE GOING TO PRODUCTION:
--   1. Re-confirm every address against your actual binary disassembly. A
--      different localisation (frFR, deDE, etc.) shifts addresses.
--   2. Capture the real expected bytes at the resolved address from a known
--      clean client and paste them into the `result` column. Placeholder
--      values are noted with `-- TODO` below.
--   3. Pick check ids that don't collide with your existing rows.
-- ============================================================================


-- ----------------------------------------------------------------------------
-- Example 1 — Vtable hook detection on the Client Object Manager
--
-- Detects: cheats that hook a virtual function on the global Object Manager
--          singleton (a common technique for "object dumper" cheats that swap
--          a vtable slot for a thunk that filters returned objects, leaks
--          GUIDs, or injects fake updates).
--
-- Chain (3 hops, terminal reads 5 bytes of the first virtual function's
-- prologue):
--     base = 0x00B41414           ; s_curMgr — pointer to ClntObjMgr instance
--     hop 0: read [0x00B41414]    -> objMgrInstance
--     hop 1: read [objMgrInstance + 0x00] -> vtable
--     hop 2: read [vtable + 0x00] -> first virtual function pointer
--     hop 3 (terminal): read 5 bytes at that function -> expected prologue
--
-- A `jmp` detour is 5 bytes (E9 XX XX XX XX), which guarantees the prologue
-- bytes change if the function is hooked.
-- ----------------------------------------------------------------------------
INSERT INTO `warden`
    (`id`, `build`, `type`, `data`, `result`, `address`, `length`, `str`, `comment`)
VALUES
    (10001, 5875, 244, '',
     '0000000000', -- TODO: replace with the real first 5 prologue bytes from clean WoW.exe
     0x00B41414, 5, '0x0,0x0,0x0',
     'Pointer chain: ClntObjMgr -> vtable -> vtable[0] -> prologue (detect vtable hook)');


-- ----------------------------------------------------------------------------
-- Example 2 — Hook detection on import-resolved GetTickCount
--
-- Detects: cheats that patch the WoW import for kernel32!GetTickCount to a
--          thunk returning bogus timestamps, defeating the TIMING_CHECK
--          (87/0x57). The IAT slot lives in WoW's `.idata`; following it
--          lands inside kernel32.dll's loaded copy.
--
-- Chain (1 hop, terminal reads 5 bytes of the resolved function prologue):
--     base = 0x00C2D154           ; IAT slot for kernel32!GetTickCount
--                                 ;   (TODO: confirm RVA on your binary)
--     hop 0 (terminal): read 5 bytes at [0x00C2D154]
--
-- Caveat: kernel32.dll is part of the OS, so the prologue bytes vary across
-- Windows versions. In practice you'd seed `result` per-OS or use a small
-- whitelist via multiple check rows. Listed here as a textbook IAT-hook
-- pattern; consider it a template, not a drop-in.
-- ----------------------------------------------------------------------------
INSERT INTO `warden`
    (`id`, `build`, `type`, `data`, `result`, `address`, `length`, `str`, `comment`)
VALUES
    (10002, 5875, 244, '',
     '0000000000', -- TODO: per-OS captured bytes of kernel32!GetTickCount prologue
     0x00C2D154, 5, '',
     'Pointer chain: WoW IAT[GetTickCount] -> kernel32 prologue (detect IAT detour)');


-- ----------------------------------------------------------------------------
-- Example 3 — Sanity check on Local Player object type field
--
-- Detects: object-replace cheats that swap the local player's object type at
--          its UnitFields descriptor block to confuse server-side validation.
--          Reads the object type byte, expected to be 4 (TYPEID_PLAYER) for
--          the local player object.
--
-- Chain (3 hops, terminal reads 1 byte):
--     base = 0x00B41414                       ; s_curMgr
--     hop 0: read [0x00B41414]                -> objMgrInstance
--     hop 1: read [objMgrInstance + 0xAC]     -> first object in linked list
--                                                (publicly documented offset)
--     hop 2: read [object + 0x14]             -> object type id field
--                                                (TYPEID layout is documented
--                                                 in Object.h)
--
-- Note: the linked list head at +0xAC is not always the local player; it
-- iterates by +0x3C until matching local GUID. For a simple template we use
-- the head — adjust if you want strict local-player semantics. This is
-- primarily useful as a presence/structural-integrity check.
-- ----------------------------------------------------------------------------
INSERT INTO `warden`
    (`id`, `build`, `type`, `data`, `result`, `address`, `length`, `str`, `comment`)
VALUES
    (10003, 5875, 244, '',
     '04', -- TYPEID_PLAYER
     0x00B41414, 1, '0x0,0xAC,0x14',
     'Pointer chain: ObjMgr -> first object -> typeId byte (detect object spoof)');


-- ----------------------------------------------------------------------------
-- Example 4 — Zero-hop sanity (degenerate chain)
--
-- Equivalent in semantics to a plain MEM_CHECK, but routed through the
-- POINTER_CHAIN_CHECK code path. Useful as a smoke test when bringing the
-- feature up: pick a known stable byte run in WoW.exe's `.text` (any
-- function whose prologue you already trust) and seed the expected bytes.
--
-- Reads 5 bytes of the SFileOpenFile prologue at 0x006485F0
-- (image base 0x00400000 + RVA 0x002485F0, taken straight from the
-- WardenInitModuleRequest in src/game/Warden/WardenWin.cpp).
-- ----------------------------------------------------------------------------
INSERT INTO `warden`
    (`id`, `build`, `type`, `data`, `result`, `address`, `length`, `str`, `comment`)
VALUES
    (10004, 5875, 244, '',
     '0000000000', -- TODO: replace with real first 5 bytes of SFileOpenFile prologue
     0x006485F0, 5, '',
     'Zero-hop pointer-chain smoke test (SFileOpenFile prologue)');


-- ----------------------------------------------------------------------------
-- Example 5 — Signature-detect mode (third-party allocation scan)
--
-- Inspired by Krilliac/AdvancedWarden's MEM2_CHECK / GAGARIN pair pattern,
-- which targets cheats that allocate executable memory in well-known
-- dynamic regions and place their payload at a fixed offset within that
-- region. Approach there: one MEM_CHECK reads the dynamic base address and
-- caches it on the session; a paired MEM_CHECK then scans `base + small
-- offset` and fails when bytes != 0 (i.e. the region is not empty as it
-- should be on a clean client).
--
-- Our generalised equivalent: a single POINTER_CHAIN_CHECK row that walks
-- to the suspect address and uses signature-detect mode (leading `!`) to
-- fail when a known cheat-signature pattern appears.
--
-- Chain (1 hop, terminal reads 4 bytes; fails if pattern found):
--     base = 0x009F348            ; static slot that holds the dynamic
--                                 ;   allocation pointer for this cheat
--                                 ;   family. (TODO: confirm against your
--                                 ;   binary; the AdvancedWarden seed used
--                                 ;   address=652040=0x9F348, length=4.)
--     hop 0 (terminal): read 4 bytes at *base + 2 (or other small offset)
--                       expected = the cheat's 4-byte signature
--                       fail when read == expected (signature present)
--
-- The leading '!' on the offset string flips the terminal compare into
-- signature-detect mode. Pick the offset value (here 0x2) to match the
-- byte-window where the cheat is known to land. Length must equal the
-- length of the signature in `result`.
-- ----------------------------------------------------------------------------
INSERT INTO `warden`
    (`id`, `build`, `type`, `data`, `result`, `address`, `length`, `str`, `comment`)
VALUES
    (10005, 5875, 244, '',
     '00003000', -- TODO: replace with the real cheat signature bytes
     0x0009F348, 4, '!0x2',
     'Signature detect: dynamic 3rd-party allocation scan (PQR-class)');


-- ----------------------------------------------------------------------------
-- Optional: action override per check id (only if you want non-default
-- penalty for these). Default action comes from
-- CONFIG_UINT32_WARDEN_CLIENT_FAIL_ACTION (0=LOG, 1=KICK, 2=BAN). The
-- override lives in the characters DB.
-- ----------------------------------------------------------------------------
-- INSERT INTO `warden_action` (`wardenId`, `action`) VALUES
--     (10001, 1),  -- kick on vtable-hook detection
--     (10002, 0),  -- log only on IAT check (more false-positive prone)
--     (10003, 1),  -- kick on object-type spoof
--     (10004, 0),  -- log only on smoke test
--     (10005, 2);  -- ban on confirmed signature match
