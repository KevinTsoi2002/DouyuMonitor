# DouyuMonitor Figma UI Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a componentized Figma proposal for DouyuMonitor's professional monitoring-console UI at 1440 x 900 and 1920 x 1080.

**Architecture:** Build a dedicated Figma design file with local design tokens, reusable control/room/tile components, and five composed desktop frames. Use auto layout for structural relationships, keep the live canvas dominant, and validate every major frame through Figma screenshots and metadata reads.

**Tech Stack:** Figma Design, Figma Plugin API through `use_figma`, Figma variables, local components, Auto Layout

---

### Task 1: Create and inspect the Figma file

**Files:**
- Reference: `docs/superpowers/specs/2026-08-10-ui-optimization-design.md`
- Reference: `src/renderer/styles.css`

- [ ] **Step 1:** Resolve the authenticated Figma plan with `whoami`.
- [ ] **Step 2:** Create `DouyuMonitor - Professional Monitoring UI` as a Figma design file.
- [ ] **Step 3:** Inspect pages, existing nodes, variables, styles, and available fonts before writing canvas content.
- [ ] **Step 4:** Confirm Segoe UI availability; use Microsoft YaHei for Chinese only when available, otherwise use Figma's available CJK-compatible fallback and record it.

### Task 2: Build local design foundations

**Files:**
- Reference: `src/renderer/styles.css`
- Reference: `docs/superpowers/specs/2026-08-10-ui-optimization-design.md`

- [ ] **Step 1:** Create scoped color variables for page, panel, hover, border, text, orange, live, offline, and warning colors.
- [ ] **Step 2:** Create scoped spacing and radius variables for 4, 6, 8, 12, 16, 28, 32, and 40px values.
- [ ] **Step 3:** Create text styles for brand, control, room name, helper text, and danmaku.
- [ ] **Step 4:** Validate variable scopes, values, style font families, and naming through a read-only `use_figma` call.

### Task 3: Build reusable components

**Files:**
- Reference: `src/renderer/components/AppHeader.tsx`
- Reference: `src/renderer/components/RoomSidebar.tsx`
- Reference: `src/renderer/components/RoomTile.tsx`

- [ ] **Step 1:** Create Icon Button variants for default, hover, active, disabled, and danger states.
- [ ] **Step 2:** Create Primary Button variants for default, hover, and disabled states.
- [ ] **Step 3:** Create Room Row variants for live, offline, primary, audio-focus, and hover states.
- [ ] **Step 4:** Create Status Indicator variants for live, offline, connecting, and warning states.
- [ ] **Step 5:** Create reusable live-tile overlay controls and offline tile states.
- [ ] **Step 6:** Screenshot the component sheet and correct any clipped text, inconsistent sizing, or overlapping nodes.

### Task 4: Compose the 1440 x 900 proposal

**Files:**
- Reference: `docs/superpowers/specs/2026-08-10-ui-optimization-design.md`

- [ ] **Step 1:** Create `Desktop / 1440x900 / Monitor` with a 40px header and 2 x 2 monitoring grid.
- [ ] **Step 2:** Create `Desktop / 1440x900 / Room Drawer` with a 292px overlay drawer that does not resize the grid.
- [ ] **Step 3:** Create `Desktop / 1440x900 / Danmaku Settings` with a 320px settings popover.
- [ ] **Step 4:** Use component instances for repeated rooms, rows, controls, status indicators, and menu items.
- [ ] **Step 5:** Screenshot each frame and verify hidden-idle controls, text truncation, drawer overlap, and popover placement.

### Task 5: Compose the 1920 x 1080 proposal

**Files:**
- Reference: `docs/superpowers/specs/2026-08-10-ui-optimization-design.md`

- [ ] **Step 1:** Create `Desktop / 1920x1080 / Monitor` with a 40px header and 3 x 2 monitoring grid.
- [ ] **Step 2:** Preserve the 6px grid gap, control density, tile radius, and status hierarchy from the 1440px version.
- [ ] **Step 3:** Screenshot the full frame and individual tile/header sections to detect reduced-scale errors.

### Task 6: Final Figma verification

**Files:**
- Verify: `docs/superpowers/specs/2026-08-10-ui-optimization-design.md`

- [ ] **Step 1:** Inspect metadata for all five deliverable frames and the component sheet.
- [ ] **Step 2:** Assert that all text uses the selected product font family and that no placeholder shimmer remains.
- [ ] **Step 3:** Verify no text clipping, incoherent overlap, blank image placeholders, or inconsistent status colors.
- [ ] **Step 4:** Return the Figma file URL and summarize the represented states and any font substitution.
