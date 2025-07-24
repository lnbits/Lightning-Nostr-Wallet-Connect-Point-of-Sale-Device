#!/usr/bin/env python3
"""
PlatformIO Extra Script for Documentation Generation
Provides Doxygen integration for the NWC Point of Sale project
"""

import os
import subprocess
import sys
from pathlib import Path

Import("env")

def generate_docs(source, target, env):
    """Generate documentation using Doxygen"""
    print("=" * 60)
    print("Generating documentation with Doxygen...")
    print("=" * 60)
    
    project_dir = Path(env.get("PROJECT_DIR"))
    doxyfile_path = project_dir / "Doxyfile"
    
    if not doxyfile_path.exists():
        print("Error: Doxyfile not found!")
        return 1
    
    try:
        # Run doxygen
        result = subprocess.run(
            ["doxygen", str(doxyfile_path)],
            cwd=str(project_dir),
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            print(f"Doxygen failed with return code {result.returncode}")
            print("STDOUT:", result.stdout)
            print("STDERR:", result.stderr)
            return result.returncode
        else:
            print("Documentation generated successfully!")
            docs_dir = project_dir / "docs" / "html"
            if docs_dir.exists():
                index_file = docs_dir / "index.html"
                if index_file.exists():
                    print(f"Documentation available at: {index_file}")
                    print(f"Open in browser: file://{index_file.absolute()}")
            
        return 0
        
    except FileNotFoundError:
        print("Error: Doxygen not found in PATH!")
        print("Please install Doxygen: brew install doxygen")
        return 1
    except Exception as e:
        print(f"Error running Doxygen: {e}")
        return 1

def clean_docs(source, target, env):
    """Clean generated documentation"""
    print("Cleaning documentation...")
    
    project_dir = Path(env.get("PROJECT_DIR"))
    docs_dir = project_dir / "docs"
    
    if docs_dir.exists():
        import shutil
        shutil.rmtree(docs_dir)
        print("Documentation cleaned.")
    else:
        print("No documentation to clean.")

# Add custom targets
env.Alias("docs", None, generate_docs)
env.Alias("cleandocs", None, clean_docs)

# Help message
env.AddCustomTarget(
    name="docs",
    dependencies=None,
    actions=generate_docs,
    title="Generate Documentation",
    description="Generate project documentation using Doxygen"
)

env.AddCustomTarget(
    name="cleandocs", 
    dependencies=None,
    actions=clean_docs,
    title="Clean Documentation",
    description="Remove generated documentation files"
)

print("Documentation commands available:")
print("  pio run -t docs      - Generate documentation")
print("  pio run -t cleandocs - Clean documentation")