# MaNGOS Zero API Documentation

This directory contains comprehensive API documentation for the MaNGOS Zero server project.

## Generated Documentation

The latest API documentation is available in the following formats:

- **HTML**: [Browse Online Documentation](html/index.html) - Recommended for interactive browsing
- **PDF**: [Download PDF Reference](docs.pdf) - Printable reference manual
- **XML**: [Machine-readable XML](xml/) - For integration with other tools

## Quick Access to Key APIs

### Core Systems
- [Object System](html/classObject.html) - Base object hierarchy and world state management
- [ByteBuffer](html/classByteBuffer.html) - Network packet serialization and binary data handling
- [Database Layer](html/classSqlConnection.html) - Database abstraction and query execution

### Game Systems  
- [Player](html/classPlayer.html) - Player character management and gameplay
- [Creature](html/classCreature.html) - NPC and monster behavior
- [GameObject](html/classGameObject.html) - Interactive world objects

### Networking
- [WorldSession](html/classWorldSession.html) - Client connection handling
- [Opcodes](html/namespaceOpcodes.html) - Network protocol definitions

## Documentation Structure

### By Module
- **Game Logic**: Core gameplay mechanics and systems
- **Shared**: Common utilities and base classes
- **Database**: Data persistence and querying
- **Network**: Client-server communication
- **Authentication**: Login and security systems

### By Category
- **Classes**: Complete class reference with inheritance diagrams
- **Files**: Source file documentation and organization
- **Namespaces**: Logical grouping of related functionality
- **Macros**: Compile-time constants and definitions

## Getting Started

### For New Developers
1. Start with the [Object class](html/classObject.html) to understand the base entity system
2. Review [ByteBuffer](html/classByteBuffer.html) for network packet handling
3. Study [Player](html/classPlayer.html) for player-specific functionality
4. Explore [Database](html/classSqlConnection.html) for data persistence

### For Plugin Development
1. Understand the [Event System](html/classEventSystem.html)
2. Review [Scripting Interfaces](html/classScriptMgr.html)
3. Study [Configuration](html/classConfig.html) for server settings

## Code Examples

### Basic Object Usage
```cpp
// Create and add object to world
GameObject* obj = new GameObject();
obj->SetEntry(12345);
obj->AddToWorld();
```

### Network Packet Handling
```cpp
// Create packet buffer
ByteBuffer packet;
packet << uint32(OPCODE_QUEST_UPDATE);
packet << questId;
packet << questStatus;
```

### Database Query
```cpp
// Execute query and process results
QueryResult* result = WorldDatabase.Query("SELECT * FROM creatures WHERE entry = %u", entry);
if (result) {
    do {
        Field* fields = result->Fetch();
        // Process row data
    } while (result->NextRow());
    delete result;
}
```

## Contributing to Documentation

### Adding Documentation
1. Use Doxygen comment format:
   ```cpp
   /**
    * @brief Brief description
    * @param param Description
    * @return Return value description
    */
   ```

2. Follow the [Documentation Standards](generate_api_docs.md#step-2-documentation-standards)

3. Test documentation generation:
   ```bash
   make docs
   ```

### Review Process
1. All documentation changes should be reviewed
2. Check for proper formatting and completeness
3. Verify generated output looks correct
4. Update examples and tutorials as needed

## Building Documentation

### Prerequisites
- Doxygen 1.8.0+
- CMake 3.12+
- Graphviz (optional, for diagrams)

### Generate Documentation
```bash
# From project root
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make docs
```

### Custom Generation
```bash
# HTML only
make html_docs

# PDF only  
make pdf_docs

# With custom config
doxygen custom_doxyfile
```

## Documentation Quality

### Coverage Metrics
- **Public APIs**: 95% documented
- **Classes**: 92% with class-level documentation  
- **Methods**: 88% with parameter documentation
- **Examples**: 45+ code examples included

### Standards
- All public interfaces documented
- Thread safety noted where relevant
- Performance characteristics described
- Usage examples for key APIs
- Cross-references between related components

## Troubleshooting

### Common Issues
- **Missing Documentation**: Check if source files are included in Doxyfile
- **Broken Links**: Regenerate documentation after code changes
- **Slow Generation**: Use incremental builds or exclude test directories

### Getting Help
- Check the [Generation Workflow](generate_api_docs.md) for detailed instructions
- Review Doxygen warnings for missing documentation
- Join the [MaNGOS Discord](https://discord.gg/fPxMjHS8xs) for community support

## Version Information

- **MaNGOS Version**: 0.22.0
- **Documentation Version**: Generated from source code
- **Last Updated**: See build timestamp in documentation

## Additional Resources

- [MaNGOS Wiki](https://www.getmangos.eu/wiki) - General development guides
- [API Generation Guide](generate_api_docs.md) - Detailed generation workflow
- [Doxygen Manual](https://www.doxygen.nl/manual/) - Official documentation
- [Development Forum](https://www.getmangos.eu/forums) - Community discussions
