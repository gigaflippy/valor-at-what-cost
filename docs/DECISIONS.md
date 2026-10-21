# Decision log

Who decided what, and why. "Proposed by" is honest about where an idea came from; the **Why** column is written by Georgi in his own words.

## Decisions

| Date | Decision | Options considered | Proposed by | Why (Georgi) |
|---|---|---|---|---|
| 2026-10-21 | Write the game in C++ | — | Georgi | _TODO_ |
| 2026-10-21 | Use raylib instead of SFML or terminal-only | raylib, SFML, terminal (ncurses) | Claude | _TODO_ |
| 2026-10-21 | Bullet-hell roguelike in the style of Enter the Gungeon | arcade, roguelike, platformer, puzzle | Georgi | _TODO_ |
| 2026-10-21 | Name: Valor at What Cost | Valor, But At What Cost; Lead Descent; Wardenfall; others | Georgi | _TODO_ |
| 2026-10-21 | All rights reserved licence, not open source | all rights reserved, MIT | Georgi | _TODO_ |
| 2026-10-21 | Match Gungeon's art techniques 1:1 but with original art and our own flavour | pixel art like Gungeon, stylised neon | Georgi | _TODO_ |
| 2026-10-21 | Five main characters at most, plus one bonus | 4, 5, 6 | Georgi | _TODO_ |
| 2026-10-21 | Bonus character unlocked by an in-game achievement | bonus unlock, replace the Gambler | Georgi | _TODO_ |
| 2026-10-21 | Phase 0 before new features: modules, data files | features first, foundation first | Claude | _TODO_ |
| 2026-10-21 | Every change lands through a PR that Georgi reviews and merges | direct pushes, PRs | Georgi | _TODO_ |

## Times Georgi overruled Claude

Kept for honesty and as interview material: where the human call changed the outcome.

| Date | Claude proposed | Georgi decided instead | Outcome |
|---|---|---|---|
| 2026-10-21 | Start with a stylised neon look, all in code | Go for Gungeon-quality pixel art | Researched Gungeon's techniques; built the 480×270 pixel renderer |
| 2026-10-21 | Bonus stand-user character named Valentin Strazh | Not an obviously Bulgarian name; make him a proper stand-user archetype | Lucien Marrow and his spirit Curtain Call, with a Rewind power |
| 2026-10-21 | Bonus character as a sixth roster slot or a Gambler replacement | Keep the roster at five; unlock him through an achievement | "Showstopper" achievement unlock |
| 2026-10-21 | Profile README "tested by: GitHub Actions builds…" | That reads as "never tested": lead with local testing | "play-tested locally", CI moved to its own line |
| 2026-10-21 | Pushing profile changes straight to `main` | Every change goes through review | This log and the PR template |
