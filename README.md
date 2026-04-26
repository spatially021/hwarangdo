# Hwarangdo (working name)

A game-oriented programming language with a handle-view memory model.  
Entities are managed by the world, not by references.

한국어 README: README.ko.md

---

## 1. Overview

Hwarangdo is an experimental programming language designed specifically for game development.

Instead of treating objects as general-purpose references, the language introduces a handle-view model where:

- Entities are created and managed by the runtime (world)
- Access is performed through temporary views
- Lifetime is explicit and controlled

This approach aims to provide predictable behavior, clear ownership semantics, and runtime safety.

---

## 2. Core Concepts

### Entity vs Value

- class → Entity (managed by the world)
- struct → Value (copied and passed normally)

---

### Handle / View

```
Handle<Player> h = world.spawn Player();

Player p = world.view(h);
p.move();

world.destroy(h);
```

- Handle<T> is a stable identifier
- view produces a temporary observer
- Direct access through handles is not allowed

---

### Explicit Lifetime

- Entities are created via world.spawn
- Must be explicitly destroyed via world.destroy
- Invalid access is treated as an error

---

## 3. Example
```
struct Vector {
    int x;
    int y;
}

impl Vector {
    void add(int dx = 0, int dy = 0) {
        x += dx;
        y += dy;
    }
}

class Player {
    Vector pos;

    void move() {
        pos.add(1, _);
    }
}

class Main {
    frame void update() {
        Handle<Player> h = world.spawn Player();

        Player p = world.view(h);
        p.move();

        world.destroy(h);
    }
}
```
---

## 4. Current Status

- Lexer / Parser / Semantic analysis: implemented
- HIR: in progress
- Verifier: in progress
- Runtime: not implemented

The design is not yet stable.

---

## 5. Why this exists

Modern languages are not always aligned with how games actually run.

Games typically require:

- Frame-based execution
- Explicit lifetime control
- Predictable runtime behavior
- Strong separation between data and world state

Hwarangdo explores a model where these are part of the language itself.

---

## 6. Roadmap

- Complete HIR
- Implement verifier
- Design MIR
- Build runtime

---

## 7. Notes

This is an experimental personal project.

The goal is not to replace existing languages, but to explore a new design direction.