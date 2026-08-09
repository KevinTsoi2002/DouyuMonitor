# Room Actions Menu Design

**Date:** 2026-08-09

## Goal

Make the existing room-tile overflow button functional without adding another persistent panel. The menu exposes the room-level actions already required by the product specification and gives the user a concise diagnostic summary.

## Design

- `RoomTile` owns only the menu open/closed state.
- The menu renders below the tile top bar with a fixed width and does not affect the tile grid size.
- Actions reuse existing store methods: refresh stream availability, set primary room, and remove room.
- The diagnostic block reads the current playback presentation and danmaku connection state. It never displays a playback URL, query string, token, cookie, or raw adapter error.
- Closing the menu happens after every action. The existing sidebar controls remain available for keyboard and compact layouts.

## Acceptance

- Opening the overflow button exposes the four expected menu actions/statuses.
- Retrying calls the existing per-room availability refresh and leaves other rooms untouched.
- Removing from the menu removes only the selected room and repairs focus using existing store behavior.
- The menu renders without overlap at desktop and mobile widths, and browser console logs remain clean.
