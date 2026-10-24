# ADR-0006: Testable menu model in app, rendering in infra

- Status: Accepted
- Date: 2026-10-13

## Context

`ui.cpp` was 1268 lines mixing browsable menu logic, input handling, and
pixel drawing — untestable and the biggest regression surface in the refactor
(encoder accel, long-press, screensaver wake).

## Decision

- `src/app/menu_model.*` owns the UI's testable logic: parameter IDs (`P_*`),
  per-mode group tables, flat-index browsing (`app_param_at`,
  `app_param_group`), and get/set/label/format (`menu_get_val`,
  `menu_set_val`, `menu_label_of`, `menu_fmt_val`). It takes an explicit
  `MenuPage` instead of reading controller state, and performs no I/O.
- `src/infra/display/` only renders (`renderer`, `oled`, `splash`, `saver`,
  `ui_controller`, `ui_nav`); `src/infra/ui.h` is the facade
  (`ui_init` / `ui_service` / `ui_draw` / `ui_run_pending` / …).
- Move rule used in the split (and still applicable): move code verbatim
  first, clean up second — UI input semantics must not change silently.

## Consequences

- Menu structure, ranges, and labels are covered by native suites (`menu:`
  suites in `test/test_main.cpp`).
- Display work cannot mutate engine state: renderers read snapshots, and
  input flows through the controller into `app/` use cases.

## Agent notes

New parameters or modes: extend the `P_*` vocabulary and group tables in
`menu_model.*` with test coverage; keep pixel code in `infra/display/`
mutation-free. Do not add browsable-logic branches inside renderers.
