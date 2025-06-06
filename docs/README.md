# Poker Platform Documentation

## Overview

This directory contains comprehensive documentation for the Poker Platform project.

## Documentation Structure

- **[API Reference](api/)** - Detailed API documentation for all modules
- **[Architecture](architecture/)** - System design and architecture documentation
- **[Tutorials](tutorials/)** - Step-by-step guides for common tasks
- **[Protocols](protocols/)** - Network protocol specifications

## Key Documents

### Getting Started
- [Installation Guide](installation.md)
- [Quick Start Tutorial](tutorials/quickstart.md)
- [Building from Source](building.md)

### Development
- [Contributing Guidelines](../CONTRIBUTING.md)
- [Code Style Guide](style-guide.md)
- [Testing Guide](testing.md)

### Features
- [AI System](features/ai-system.md)
- [Tournament Mode](features/tournaments.md)
- [Hand History](features/hand-history.md)
- [Persistence](features/persistence.md)

### API Modules
- [Core Game Engine](api/game-engine.md)
- [AI Players](api/ai-players.md)
- [Network Protocol](api/network.md)
- [Variant Interface](api/variants.md)

## Building Documentation

To generate HTML documentation from source:

```bash
cd docs
doxygen Doxyfile
```

The generated documentation will be in `docs/html/`.

## Contributing to Documentation

When adding new features or making significant changes:

1. Update relevant API documentation
2. Add examples to tutorials if applicable
3. Update architecture diagrams if system design changes
4. Keep the changelog current

See [Contributing Guidelines](../CONTRIBUTING.md) for more details.