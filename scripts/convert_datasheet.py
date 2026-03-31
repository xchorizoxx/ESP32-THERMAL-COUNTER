#!/usr/bin/env python3
"""
PDF to Markdown Converter for MLX90640 Datasheet

Converts the Melexis MLX90640 datasheet from PDF to structured Markdown.
Preserves tables, figures, and technical specifications.

Requirements:
    pip install pymupdf4llm

Usage:
    python scripts/convert_datasheet.py docs/hardware/MLX90640_Datasheet.pdf docs/hardware/MLX90640_Datasheet.md
"""

import argparse
import sys
from pathlib import Path

try:
    import pymupdf4llm
except ImportError:
    print("Error: pymupdf4llm not installed.")
    print("Install with: pip install pymupdf4llm")
    sys.exit(1)


def convert_pdf_to_markdown(pdf_path: str, output_path: str = None) -> str:
    """
    Convert PDF file to Markdown.
    
    Args:
        pdf_path: Path to input PDF file
        output_path: Optional path for output markdown file
        
    Returns:
        Converted markdown content as string
    """
    pdf_file = Path(pdf_path)
    
    if not pdf_file.exists():
        raise FileNotFoundError(f"PDF file not found: {pdf_path}")
    
    if not pdf_file.suffix.lower() == '.pdf':
        raise ValueError(f"Input file must be PDF, got: {pdf_file.suffix}")
    
    # Convert using pymupdf4llm (best for technical docs with tables)
    print(f"Converting {pdf_file.name}...")
    
    markdown_content = pymupdf4llm.to_markdown(
        str(pdf_file),
        page_chunks=False,  # Single output document
        show_progress=True
    )
    
    # Add header with metadata
    header = f"""# {pdf_file.stem}

> Converted from PDF using `pymupdf4llm`
> Original: [{pdf_file.name}]({pdf_file.name})

---

"""
    
    full_content = header + markdown_content
    
    # Write to file if output path specified
    if output_path:
        output_file = Path(output_path)
        output_file.write_text(full_content, encoding='utf-8')
        print(f"Written: {output_file}")
    
    return full_content


def main():
    parser = argparse.ArgumentParser(
        description="Convert PDF datasheet to Markdown"
    )
    parser.add_argument(
        "input",
        help="Input PDF file path"
    )
    parser.add_argument(
        "output",
        nargs="?",
        help="Output Markdown file path (optional)"
    )
    
    args = parser.parse_args()
    
    try:
        # Auto-generate output path if not provided
        if not args.output:
            input_path = Path(args.input)
            args.output = str(input_path.with_suffix('.md'))
        
        content = convert_pdf_to_markdown(args.input, args.output)
        print(f"\nConversion complete. Output: {args.output}")
        print(f"Content length: {len(content)} characters")
        
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error during conversion: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
