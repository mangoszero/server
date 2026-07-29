# API Documentation Generation Workflow

This document describes the workflow for generating comprehensive API documentation for the MaNGOS Zero server using Doxygen.

## Prerequisites

- Doxygen 1.8.0 or higher
- CMake 3.12 or higher
- Graphviz (for call graphs and diagrams, optional)

## Quick Start

### 1. Configure Documentation

```bash
# Create build directory
mkdir build
cd build

# Configure with documentation enabled
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### 2. Generate Documentation

```bash
# Generate HTML documentation
make docs

# Or generate specific format
make html_docs    # HTML only
make pdf_docs     # PDF only
make xml_docs      # XML only
```

### 3. View Documentation

```bash
# Open in browser
open build/docs/html/index.html

# Or serve locally
cd build/docs/html
python -m http.server 8000
# Navigate to http://localhost:8000
```

## Detailed Workflow

### Step 1: Build Configuration

The Doxygen configuration is controlled by `Doxyfile.in` which is processed by CMake. Key settings:

- **EXTRACT_ALL = YES**: Documents all entities, even undocumented ones
- **EXTRACT_PRIVATE = YES**: Includes private members in documentation
- **SOURCE_BROWSER = YES**: Includes source code browsing
- **REFERENCES_RELATION = YES**: Shows function call relationships
- **ALPHABETICAL_INDEX = YES**: Provides alphabetical class index

### Step 2: Documentation Standards

#### Class Documentation
```cpp
/**
 * @brief Brief description of the class
 * 
 * Detailed description of the class purpose, usage, and important
 * implementation details. Include:
 * - Main functionality
 * - Usage examples
 * - Thread safety notes
 * - Performance considerations
 * 
 * @note Important implementation notes
 * @warning Critical warnings for users
 */
class MyClass
{
    // ...
};
```

#### Method Documentation
```cpp
/**
 * @brief Brief description of method purpose
 * 
 * Detailed description including algorithm explanation,
 * parameter constraints, and return value details.
 * 
 * @param param1 Description of first parameter
 * @param param2 Description of second parameter
 * @return Description of return value
 * @note Special behavior notes
 * @warning Important warnings
 */
int myMethod(int param1, std::string param2);
```

#### File Documentation
```cpp
/**
 * @file filename.h
 * @brief Brief description of file contents
 * 
 * Detailed description of the file's purpose and the main
 * classes/functions it contains.
 * 
 * @author Author Name
 * @date YYYY-MM-DD
 */
```

### Step 3: Automated Generation

#### CMake Integration
```cmake
# Add to CMakeLists.txt
find_package(Doxygen REQUIRED)

if(DOXYGEN_FOUND)
    set(DOXYFILE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in 
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    
    add_custom_target(docs
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
    
    add_custom_target(html_docs
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating HTML API documentation"
        VERBATIM
    )
endif()
```

#### Continuous Integration
Add to your CI pipeline:

```yaml
# GitHub Actions example
- name: Generate Documentation
  run: |
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make docs
    
- name: Deploy Documentation
  uses: peaceiris/actions-gh-pages@v3
  with:
    github_token: ${{ secrets.GITHUB_TOKEN }}
    publish_dir: ./build/docs/html
```

### Step 4: Quality Assurance

#### Documentation Review Checklist
- [ ] All public classes have class-level documentation
- [ ] All public methods have parameter and return documentation
- [ ] Complex algorithms have detailed explanations
- [ ] Thread safety is documented where relevant
- [ ] Performance characteristics are noted
- [ ] Usage examples are provided for key APIs
- [ ] Cross-references between related classes work
- [ ] All TODO/FIXME comments are addressed

#### Automated Checks
```bash
# Check for undocumented parameters
doxygen -w Doxyfile | grep "not documented"

# Check for missing class documentation
doxygen -w Doxyfile | grep "documented"

# Generate statistics
doxygen -s Doxyfile
```

### Step 5: Output Formats

#### HTML Documentation
- Primary format for browsing
- Interactive navigation
- Search functionality
- Source code cross-references
- Call graphs and diagrams

#### PDF Documentation
- Printable reference
- Complete API specification
- Suitable for offline reading

#### XML Documentation
- Machine-readable format
- Integration with other tools
- Custom processing pipelines

## Best Practices

### 1. Consistent Style
- Use `@brief` for all descriptions
- Follow parameter naming conventions
- Include return value descriptions
- Add usage examples for complex APIs

### 2. Comprehensive Coverage
- Document all public interfaces
- Include private members when relevant
- Explain design decisions
- Provide context for complex code

### 3. Maintainability
- Keep documentation close to code
- Update docs with code changes
- Review documentation in PRs
- Use automated checks

### 4. User Focus
- Write for developers using the API
- Provide clear examples
- Explain when to use different methods
- Include performance guidance

## Troubleshooting

### Common Issues

#### Missing Documentation
```bash
# Enable verbose output
doxygen -d Doxyfile

# Check for parsing errors
doxygen -w Doxyfile > warnings.txt
```

#### Large Build Times
- Reduce `EXTRACT_ALL` to NO for faster builds
- Exclude test directories with `EXCLUDE_PATTERNS`
- Use `CREATE_SUBDIRS = YES` for large projects

#### Memory Issues
- Increase `SYMBOL_CACHE_SIZE` for large codebases
- Use `OPTIMIZE_OUTPUT_FOR_C = YES` for C-heavy code
- Reduce `MAX_INITIALIZER_LINES` to limit output

### Performance Optimization

```bash
# Parallel generation (Doxygen 1.9+)
doxygen -j $(nproc) Doxyfile

# Incremental generation
doxygen -u Doxyfile
```

## Integration with Development Workflow

### Pre-commit Hooks
```bash
#!/bin/sh
# .git/hooks/pre-commit
doxygen -w Doxyfile 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Documentation warnings detected. Please fix before committing."
    exit 1
fi
```

### IDE Integration
- **VS Code**: Use Doxygen Documentation Generator extension
- **CLion**: Built-in Doxygen support
- **Vim**: DoxygenToolkit plugin
- **Emacs**: doxygen.el package

## Maintenance

### Regular Tasks
1. Update documentation with new features
2. Review and improve existing docs
3. Check for broken links
4. Update examples and tutorials
5. Monitor documentation coverage metrics

### Metrics to Track
- Documentation coverage percentage
- Number of undocumented public APIs
- Documentation build time
- User feedback and issues

## Resources

- [Doxygen Manual](https://www.doxygen.nl/manual/)
- [Doxygen Formatting Guide](https://www.doxygen.nl/manual/docblocks.html)
- [CMake Integration](https://cmake.org/cmake/help/latest/module/FindDoxygen.html)
- [Graphviz Documentation](https://graphviz.org/documentation/)
