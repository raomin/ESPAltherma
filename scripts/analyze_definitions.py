#!/usr/bin/env python3
"""
Analyze Daikin heat pump definition files to find common mappings.
This script parses all .h definition files in include/def,
extracts label definitions, and identifies common mappings across models.
"""

import os
import re
from collections import defaultdict
from pathlib import Path
import json

# Resolve paths relative to the repo root.
# This script lives in <repo>/scripts, definitions live in <repo>/include/def,
# and generated reports are written to <repo>/doc.
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DEF_DIR = REPO_ROOT / 'include' / 'def'
DOC_DIR = REPO_ROOT / 'doc'

# Regex pattern to parse LabelDef entries
# Format: {registryID, offset, convid, dataSize, dataType, "label"}
LABEL_PATTERN = re.compile(
    r'^\s*(?://)?'  # Optional comment prefix
    r'\s*\{\s*'
    r'(0x[0-9A-Fa-f]+|[0-9]+)\s*,'  # registryID (hex or decimal)
    r'\s*([0-9]+)\s*,'              # offset
    r'\s*([0-9]+)\s*,'              # convid
    r'\s*([0-9]+)\s*,'              # dataSize
    r'\s*(-?[0-9]+)\s*,'            # dataType
    r'\s*"([^"]+)"'                 # label
    r'\s*\}'
)

def parse_definition_file(filepath):
    """Parse a single definition file and extract all label definitions."""
    definitions = []
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    for line in content.split('\n'):
        match = LABEL_PATTERN.search(line)
        if match:
            registry_id = match.group(1)
            # Convert hex to int
            if registry_id.startswith('0x'):
                registry_id = int(registry_id, 16)
            else:
                registry_id = int(registry_id)
            
            offset = int(match.group(2))
            convid = int(match.group(3))
            data_size = int(match.group(4))
            data_type = int(match.group(5))
            label = match.group(6)
            
            # Check if it's commented out
            is_commented = line.strip().startswith('//')
            
            definitions.append({
                'registry_id': registry_id,
                'offset': offset,
                'convid': convid,
                'data_size': data_size,
                'data_type': data_type,
                'label': label,
                'commented': is_commented,
                'key': (registry_id, offset, convid)  # Unique identifier for a mapping
            })
    
    return definitions

def get_definition_files():
    """Get all .h definition files in the def directory (excluding subdirs and special files)."""
    files = []
    for f in DEF_DIR.iterdir():
        if f.is_file() and f.suffix == '.h':
            # Skip PROTOCOL files as they're different protocol
            if 'PROTOCOL' not in f.name and f.name != 'DEFAULT.h':
                files.append(f)
    return sorted(files)

def analyze_commonality():
    """Analyze all definition files and find common mappings."""
    files = get_definition_files()
    
    print(f"Found {len(files)} definition files to analyze\n")
    
    # Store all definitions per file
    all_definitions = {}
    
    # Track mapping occurrences: key -> {label: [(file, definition), ...]}
    mapping_occurrences = defaultdict(lambda: defaultdict(list))
    
    # Track unique labels per key
    labels_per_key = defaultdict(set)
    
    for filepath in files:
        filename = filepath.name
        defs = parse_definition_file(filepath)
        all_definitions[filename] = defs
        
        for d in defs:
            key = d['key']
            label = d['label']
            mapping_occurrences[key][label].append((filename, d))
            labels_per_key[key].add(label)
    
    total_files = len(files)
    
    # Analyze mappings
    print("=" * 80)
    print("ANALYSIS RESULTS")
    print("=" * 80)
    
    # Group by occurrence count
    universal_mappings = []  # In all files with same label
    common_mappings = []     # In 80%+ files
    majority_mappings = []   # In 50%+ files
    conflicting_mappings = []  # Same key but different labels
    
    for key, label_dict in mapping_occurrences.items():
        total_occurrences = sum(len(files) for files in label_dict.values())
        
        if len(label_dict) > 1:
            # Multiple labels for same key - conflict
            conflicting_mappings.append({
                'key': key,
                'labels': {label: len(files) for label, files in label_dict.items()},
                'total': total_occurrences
            })
        else:
            label = list(label_dict.keys())[0]
            files_list = label_dict[label]
            occurrence_pct = (total_occurrences / total_files) * 100
            
            entry = {
                'key': key,
                'label': label,
                'count': total_occurrences,
                'percentage': occurrence_pct,
                'definition': files_list[0][1]  # Take first definition
            }
            
            if total_occurrences == total_files:
                universal_mappings.append(entry)
            elif occurrence_pct >= 80:
                common_mappings.append(entry)
            elif occurrence_pct >= 50:
                majority_mappings.append(entry)
    
    # Sort by occurrence count
    universal_mappings.sort(key=lambda x: (-x['count'], x['key']))
    common_mappings.sort(key=lambda x: (-x['count'], x['key']))
    majority_mappings.sort(key=lambda x: (-x['count'], x['key']))
    conflicting_mappings.sort(key=lambda x: (-x['total'], x['key']))
    
    # Print results
    print(f"\n## UNIVERSAL MAPPINGS (100% - in all {total_files} files with same label)")
    print(f"Found: {len(universal_mappings)} mappings\n")
    for m in universal_mappings[:30]:  # Show top 30
        d = m['definition']
        print(f"  {{0x{d['registry_id']:02X},{d['offset']},{d['convid']},{d['data_size']},{d['data_type']},\"{d['label']}\"}}")
    if len(universal_mappings) > 30:
        print(f"  ... and {len(universal_mappings) - 30} more")
    
    print(f"\n## COMMON MAPPINGS (80%+ files with same label)")
    print(f"Found: {len(common_mappings)} mappings\n")
    for m in common_mappings[:30]:
        d = m['definition']
        print(f"  {{0x{d['registry_id']:02X},{d['offset']},{d['convid']},{d['data_size']},{d['data_type']},\"{d['label']}\"}}  ({m['count']}/{total_files} = {m['percentage']:.0f}%)")
    if len(common_mappings) > 30:
        print(f"  ... and {len(common_mappings) - 30} more")
    
    print(f"\n## MAJORITY MAPPINGS (50-80% files with same label)")
    print(f"Found: {len(majority_mappings)} mappings\n")
    for m in majority_mappings[:20]:
        d = m['definition']
        print(f"  {{0x{d['registry_id']:02X},{d['offset']},{d['convid']},{d['data_size']},{d['data_type']},\"{d['label']}\"}}  ({m['count']}/{total_files} = {m['percentage']:.0f}%)")
    if len(majority_mappings) > 20:
        print(f"  ... and {len(majority_mappings) - 20} more")
    
    print(f"\n## CONFLICTING MAPPINGS (same registry/offset/convid but different labels)")
    print(f"Found: {len(conflicting_mappings)} conflicts\n")
    for c in conflicting_mappings[:15]:
        key = c['key']
        print(f"  Key: (0x{key[0]:02X}, {key[1]}, {key[2]})")
        for label, count in sorted(c['labels'].items(), key=lambda x: -x[1]):
            print(f"    - \"{label}\": {count} files")
    if len(conflicting_mappings) > 15:
        print(f"  ... and {len(conflicting_mappings) - 15} more conflicts")
    
    return {
        'total_files': total_files,
        'universal': universal_mappings,
        'common': common_mappings,
        'majority': majority_mappings,
        'conflicting': conflicting_mappings,
        'all_definitions': all_definitions
    }

def identify_families(analysis_result):
    """Identify model families based on mapping patterns."""
    all_defs = analysis_result['all_definitions']
    
    # Create a signature for each file based on its mappings
    file_signatures = {}
    for filename, defs in all_defs.items():
        # Create a set of keys for this file
        keys = frozenset(d['key'] for d in defs)
        file_signatures[filename] = keys
    
    # Group files with similar signatures
    # Calculate Jaccard similarity between files
    print("\n" + "=" * 80)
    print("MODEL FAMILY ANALYSIS")
    print("=" * 80)
    
    # Look for natural groupings based on key prefixes (0x60, 0x61, etc)
    # This corresponds to different data sections
    
    key_prefixes = defaultdict(lambda: defaultdict(set))
    for filename, defs in all_defs.items():
        for d in defs:
            prefix = d['registry_id'] >> 4  # Upper nibble
            key_prefixes[prefix][filename].add(d['key'])
    
    # Find files that have unique sections (like 0xA0, 0xA1 for Monobloc)
    monobloc_files = set()
    standard_files = set()
    hybrid_files = set()
    geo_files = set()
    mini_chiller_files = set()
    
    for filename in all_defs.keys():
        if 'Monobloc' in filename:
            monobloc_files.add(filename)
        elif 'Hybrid' in filename:
            hybrid_files.add(filename)
        elif 'GEO' in filename:
            geo_files.add(filename)
        elif 'chiller' in filename.lower():
            mini_chiller_files.add(filename)
        else:
            standard_files.add(filename)
    
    print(f"\n## Identified Families by Name Pattern:")
    print(f"  - Monobloc models: {len(monobloc_files)}")
    print(f"  - Standard (Split) models: {len(standard_files)}")
    print(f"  - Hybrid models: {len(hybrid_files)}")
    print(f"  - Geothermal (GEO) models: {len(geo_files)}")
    print(f"  - Mini Chiller models: {len(mini_chiller_files)}")
    
    # Analyze which registry IDs are unique to each family
    print(f"\n## Registry ID distribution by family:")
    
    families = {
        'Monobloc': monobloc_files,
        'Standard': standard_files,
        'Hybrid': hybrid_files,
        'Geothermal': geo_files,
        'Mini Chiller': mini_chiller_files
    }
    
    family_registries = {}
    for family_name, files in families.items():
        if not files:
            continue
        registries = set()
        for filename in files:
            for d in all_defs[filename]:
                registries.add(d['registry_id'])
        family_registries[family_name] = registries
        print(f"  {family_name}: {sorted([hex(r) for r in registries])}")
    
    return families

def generate_generic_definitions(analysis_result, families):
    """Generate generic definition files based on analysis."""
    
    universal = analysis_result['universal']
    common = analysis_result['common']
    majority = analysis_result['majority']
    conflicting = analysis_result['conflicting']
    all_defs = analysis_result['all_definitions']
    
    print("\n" + "=" * 80)
    print("GENERATING GENERIC DEFINITION FILES")
    print("=" * 80)
    
    # Strategy:
    # 1. Create a GENERIC.h that includes all universal + common mappings
    # 2. For conflicting mappings, pick the most common label
    # 3. Optionally create family-specific files for unique mappings
    
    # Labels that should be ENABLED by default
    # All universal mappings (100% occurrence with same label) are enabled
    ENABLED_BY_DEFAULT = set(m['label'] for m in universal)
    
    # Collect all mappings for the generic file
    generic_mappings = {}
    
    # Add universal mappings
    for m in universal:
        key = m['key']
        d = m['definition']
        generic_mappings[key] = d
    
    # Add common mappings (80%+)
    for m in common:
        key = m['key']
        if key not in generic_mappings:
            d = m['definition']
            generic_mappings[key] = d
    
    # Add majority mappings (50%+)
    for m in majority:
        key = m['key']
        if key not in generic_mappings:
            d = m['definition']
            generic_mappings[key] = d
    
    # For conflicting mappings, pick the most common label
    for c in conflicting:
        key = tuple(c['key'])
        labels = c['labels']
        # Get the most common label
        best_label = max(labels.items(), key=lambda x: x[1])[0]
        
        # Find a definition with this label from any file
        for filename, defs in all_defs.items():
            for d in defs:
                if d['key'] == key and d['label'] == best_label:
                    generic_mappings[key] = d
                    break
            if key in generic_mappings:
                break
    
    # Sort mappings by registry_id, then offset
    sorted_mappings = sorted(generic_mappings.values(), 
                            key=lambda x: (x['registry_id'], x['offset'], x['convid']))
    
    # Registry descriptions
    REGISTRY_DESCRIPTIONS = {
        0x00: "System Configuration",
        0x10: "Operation Status & Control",
        0x11: "Outdoor Unit EEPROM",
        0x20: "Outdoor Unit Temperatures & Pressures (1)",
        0x21: "Outdoor Unit Temperatures & Pressures (2)",
        0x30: "Actuator Data (Compressor, Fan, Valves)",
        0x60: "Indoor Unit Status (1)",
        0x61: "Indoor Unit Temperatures",
        0x62: "Indoor Unit Status (2)",
        0x63: "Indoor Unit EEPROM & CT Sensors",
        0x64: "Hybrid/Boiler Data",
        0x65: "Extended Indoor Data",
        0xA0: "Monobloc Outdoor Data",
        0xA1: "Monobloc Heat Exchanger Data",
    }
    
    # Generate the file content
    output_lines = []
    output_lines.append('#include "labeldef.h"')
    output_lines.append('// ============================================================================')
    output_lines.append('// GENERIC definition file for Daikin Altherma heat pumps')
    output_lines.append('// ============================================================================')
    output_lines.append('//')
    output_lines.append('// Auto-generated from analysis of all model-specific definition files.')
    output_lines.append('// This file contains mappings common to most models (50%+ occurrence).')
    output_lines.append('//')
    output_lines.append('// USAGE:')
    output_lines.append('// - Common values are already ENABLED (no // prefix)')
    output_lines.append('// - Additional values are commented out - uncomment as needed')
    output_lines.append('// - Some values may not work with all models (will simply return no data)')
    output_lines.append('//')
    output_lines.append('// If you need model-specific values not listed here, check the')
    output_lines.append('// model-specific definition file for your heat pump.')
    output_lines.append('// ============================================================================')
    output_lines.append('')
    output_lines.append('LabelDef labelDefs[] = {')
    output_lines.append('')
    
    current_registry = None
    for d in sorted_mappings:
        # Add section comment when registry changes
        if current_registry != d['registry_id']:
            if current_registry is not None:
                output_lines.append('')
            current_registry = d['registry_id']
            reg_desc = REGISTRY_DESCRIPTIONS.get(current_registry, "Data")
            output_lines.append(f'// --- Registry 0x{current_registry:02X}: {reg_desc} ---')
        
        # Check if this should be enabled by default
        is_enabled = d['label'] in ENABLED_BY_DEFAULT
        
        # Format the line
        prefix = "" if is_enabled else "//"
        line = f"{prefix}{{0x{d['registry_id']:02X},{d['offset']},{d['convid']},{d['data_size']},{d['data_type']},\"{d['label']}\"}},"
        output_lines.append(line)
    
    output_lines.append('')
    output_lines.append('};')
    output_lines.append('')
    
    # Write the generic file
    DOC_DIR.mkdir(parents=True, exist_ok=True)
    output_path = DOC_DIR / 'GENERIC.h'
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(output_lines))
    
    print(f"\nGenerated: {output_path}")
    print(f"Total mappings: {len(sorted_mappings)}")
    
    # Count enabled vs commented
    enabled_count = sum(1 for d in sorted_mappings if d['label'] in ENABLED_BY_DEFAULT)
    
    # Count by type
    universal_count = len(analysis_result['universal'])
    common_count = len(analysis_result['common'])
    majority_count = len(analysis_result['majority'])
    conflict_resolved = len(conflicting)
    
    print(f"  - Universal (100%): {universal_count}")
    print(f"  - Common (80%+): {common_count}")
    print(f"  - Majority (50%+): {majority_count}")
    print(f"  - Conflicts resolved: {conflict_resolved}")
    print(f"  - Enabled by default: {enabled_count}")
    
    return output_path, sorted_mappings

def generate_discrepancies_report(analysis_result):
    """Generate a report of discrepancies - same label name but different mappings."""
    
    all_defs = analysis_result['all_definitions']
    
    print("\n" + "=" * 80)
    print("GENERATING DISCREPANCIES REPORT")
    print("=" * 80)
    
    # Find labels that appear with different keys across files
    label_to_keys = defaultdict(lambda: defaultdict(list))
    
    for filename, defs in all_defs.items():
        for d in defs:
            label = d['label']
            key = (d['registry_id'], d['offset'], d['convid'], d['data_size'], d['data_type'])
            label_to_keys[label][key].append(filename)
    
    # Find labels with multiple different keys
    discrepancies = []
    for label, keys_dict in label_to_keys.items():
        if len(keys_dict) > 1:
            discrepancies.append({
                'label': label,
                'variants': [
                    {
                        'key': key,
                        'files': files,
                        'count': len(files)
                    }
                    for key, files in keys_dict.items()
                ]
            })
    
    # Sort by label name
    discrepancies.sort(key=lambda x: x['label'])
    
    # Generate the report
    output_lines = []
    output_lines.append("# Daikin Definition Discrepancies Report")
    output_lines.append("=" * 80)
    output_lines.append("")
    output_lines.append("This report lists labels that have DIFFERENT mappings across model files.")
    output_lines.append("Same label name, but different registry/offset/convid/size/type values.")
    output_lines.append("")
    output_lines.append(f"Total discrepancies found: {len(discrepancies)}")
    output_lines.append("")
    output_lines.append("=" * 80)
    output_lines.append("")
    
    for disc in discrepancies:
        label = disc['label']
        output_lines.append(f"## \"{label}\"")
        output_lines.append("-" * 60)
        
        # Sort variants by count (most common first)
        variants = sorted(disc['variants'], key=lambda x: -x['count'])
        
        for i, var in enumerate(variants):
            key = var['key']
            registry_id, offset, convid, data_size, data_type = key
            count = var['count']
            files = var['files']
            
            marker = "[MOST COMMON]" if i == 0 else ""
            output_lines.append(f"  Variant {i+1} {marker}")
            output_lines.append(f"    Mapping: {{0x{registry_id:02X}, {offset}, {convid}, {data_size}, {data_type}}}")
            output_lines.append(f"    Used in {count} file(s):")
            for f in sorted(files)[:5]:  # Show first 5 files
                output_lines.append(f"      - {f}")
            if len(files) > 5:
                output_lines.append(f"      ... and {len(files) - 5} more")
        output_lines.append("")
    
    # Write the report
    DOC_DIR.mkdir(parents=True, exist_ok=True)
    output_path = DOC_DIR / 'DISCREPANCIES.md'
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(output_lines))
    
    print(f"Generated: {output_path}")
    print(f"Total labels with different mappings: {len(discrepancies)}")
    
    return output_path, discrepancies

def analyze_conflicts_for_families(analysis_result, families):
    """Analyze if conflicts align with model families."""
    
    conflicting = analysis_result['conflicting']
    all_defs = analysis_result['all_definitions']
    
    print("\n" + "=" * 80)
    print("CONFLICT ANALYSIS BY FAMILY")
    print("=" * 80)
    
    # For each conflict, see which families use which label
    family_conflicts = []
    
    for c in conflicting:
        key = tuple(c['key'])
        
        family_label_map = defaultdict(lambda: defaultdict(int))
        
        for filename, defs in all_defs.items():
            # Find which family this file belongs to
            file_family = None
            for family_name, files in families.items():
                if filename in files:
                    file_family = family_name
                    break
            
            if not file_family:
                continue
            
            # Find the label for this key in this file
            for d in defs:
                if d['key'] == key:
                    family_label_map[file_family][d['label']] += 1
        
        if family_label_map:
            family_conflicts.append({
                'key': key,
                'labels_by_family': dict(family_label_map)
            })
    
    # Print family-aligned conflicts
    print("\nConflicts that align with families (different families use different labels):")
    family_aligned = 0
    for fc in family_conflicts[:10]:
        key = fc['key']
        families_data = fc['labels_by_family']
        
        # Check if different families consistently use different labels
        unique_labels = set()
        for family, labels in families_data.items():
            unique_labels.update(labels.keys())
        
        if len(unique_labels) > 1:
            family_aligned += 1
            print(f"\n  Key: (0x{key[0]:02X}, {key[1]}, {key[2]})")
            for family, labels in sorted(families_data.items()):
                for label, count in labels.items():
                    print(f"    {family}: \"{label}\" ({count} files)")
    
    print(f"\nTotal family-aligned conflicts: {family_aligned}/{len(family_conflicts)}")
    
    return family_conflicts

def main():
    """Main analysis function."""
    print("Daikin Definition File Analyzer")
    print("=" * 80)
    
    # Run analysis
    analysis = analyze_commonality()
    
    # Identify families
    families = identify_families(analysis)
    
    # Analyze conflicts
    analyze_conflicts_for_families(analysis, families)
    
    # Generate generic definition file
    output_path, mappings = generate_generic_definitions(analysis, families)
    
    # Generate discrepancies report
    generate_discrepancies_report(analysis)
    
    # Summary
    print("\n" + "=" * 80)
    print("SUMMARY & RECOMMENDATIONS")
    print("=" * 80)
    
    total_unique_mappings = len(set(
        d['key'] 
        for defs in analysis['all_definitions'].values() 
        for d in defs
    ))
    
    print(f"""
Analysis complete!

Files analyzed: {analysis['total_files']}
Total unique mapping keys: {total_unique_mappings}
Mappings in GENERIC.h: {len(mappings)}

The generated GENERIC.h file covers:
- All universal mappings (present in 100% of files)
- Common mappings (present in 80%+ of files)
- Majority mappings (present in 50%+ of files)
- Resolved conflicts (using most common label)

Recommendation:
Users can start with GENERIC.h for most Altherma models.
If specific values are not working, they can fall back to 
their model-specific definition file.

Family-specific notes:
- Monobloc models have unique 0xA0-0xA1 registries
- Standard (Split) models have the most common pattern
- Hybrid models have additional boiler-related mappings
- Geothermal models have ground source specific values
""")

if __name__ == '__main__':
    main()
