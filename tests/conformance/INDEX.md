# Conformance Index

Every numbered rule in `docs/LANGUAGE_REFERENCE.md` maps to at least one test.

| Rule | Test(s) |
|------|---------|
| R001 | 15_int32 |
| R002 | 15_int32 |
| R003 | 15_int32 |
| R004 | errors/div_zero, errors/mod_zero |
| R005 | 02_strings |
| R006 | 03_arrays |
| R007 | errors/oob |
| R008 | 16_variables |
| R009 | 16_variables |
| R010 | 17_truthiness |
| R011 | 04_control |
| R012 | 05_functions |
| R013 | 14_scoping |
| R014 | 07_logic, 17_truthiness |
| R015 | 11_precedence |
| R016 | 02_strings, 19_escapes |
| R017 | 06_equality |
| R018 | 10_use |
| R019 | 13_comments |
| R020 | 18_semicolons |
| R021 | 19_escapes |
| R022 | 20_builtins |
| R023 | errors/argv_call |
| R024 | 20_builtins |
| R025 | errors/div_zero |

## Additional conformance tests

| Test | Covers |
|------|--------|
| 01_arith | arithmetic and precedence |
| 08_chrord | `chr` and `ord` |
| 09_sub | `sub` on strings and arrays |
| 12_unary | unary `-`, `+`, `not` |
