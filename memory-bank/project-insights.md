# CRITICAL: Blood Bowl Field Dimensions

## Standard Blood Bowl Field: 26 × 15 SQUARES
- **FIELD_WIDTH = 26** (X: 0 to 25)
- **FIELD_HEIGHT = 15** (Y: 0 to 14)

This is confirmed by:
- Server FieldCoordinate.java: `FIELD_WIDTH = 26, FIELD_HEIGHT = 15`
- Official Blood Bowl 2025 rules: https://bloodbowlbase.ru/bb2025/core_rules/

## Box Coordinates (Off-field)
Players NOT on the field use "box coordinates":
- **X = -1**: Reserve
- **X = -2**: Knocked Out
- **X = -3**: Badly Hurt
- **X = -4**: Serious Injury
- **X = -5**: RIP (dead)
- **X = -6**: Banned
- **X = -7**: Missing
- **X >= 30**: Same categories for away team (30=reserve, 31=ko, etc.)

The `isBoxCoordinate()` function from FieldCoordinate.java:
```java
return x == -1 || x == -2 || x == -3 || x == -4 || x == -5 ||
       x == -6 || x == -7 || x >= 30;
```

## Server Coordinate System
- Server sends raw coordinates in the 26×15 space
- Valid field coordinates: X in [0, 25], Y in [0, 14]
- The `transform()` function mirrors X: `FIELD_WIDTH - 1 - getX()` = `25 - getX()`
- This is used when switching sides (home ↔ away perspective)

## UI Rendering
- GameField renders a 26×15 grid matching the server's coordinate system
- Players are filtered using `isBoxCoordinate()` - only on-field players render
- Ball is shown when its coordinate is NOT a box coordinate
- Team rosters show ALL roster players regardless of field position