#!/usr/bin/env python3
"""
Model preparation script for Intent Recognition

This script helps prepare your ONNX model for use with the C++ intent recognizer.
It performs the following tasks:
1. Validates model structure (inputs/outputs)
2. Quantizes model to INT8 (optional)
3. Organizes files into expected directory structure
4. Creates android_config.json

Usage:
    # Basic usage
    python prepare_model.py --model_path joint_model.onnx \\
                           --output_dir ../../../data/file/models/intend \\
                           --intent_labels intent_label.txt \\
                           --slot_labels slot_label.txt \\
                           --vocab vocab.txt

    # With quantization
    python prepare_model.py --model_path joint_model.onnx \\
                           --output_dir ../../../data/file/models/intend \\
                           --intent_labels intent_label.txt \\
                           --slot_labels slot_label.txt \\
                           --vocab vocab.txt \\
                           --quantize

    # Specify max sequence length
    python prepare_model.py --model_path joint_model.onnx \\
                           --output_dir ../../../data/file/models/intend \\
                           --intent_labels intent_label.txt \\
                           --slot_labels slot_label.txt \\
                           --vocab vocab.txt \\
                           --max_seq_len 128
"""

import os
import json
import shutil
import argparse
import onnx
from pathlib import Path


def validate_model(model_path: str) -> dict:
    """
    Validate ONNX model structure.

    Args:
        model_path: Path to ONNX model

    Returns:
        Dictionary with model info

    Raises:
        ValueError: If model structure is invalid
    """
    print(f"\nValidating model: {model_path}")

    model = onnx.load(model_path)

    # Check inputs
    inputs = model.graph.input
    print(f"\nInputs ({len(inputs)}):")
    for inp in inputs:
        name = inp.name
        shape = [dim.dim_value if dim.dim_value > 0 else "dynamic"
                 for dim in inp.type.tensor_type.shape.dim]
        print(f"  - {name}: {shape}")

    # Check outputs
    outputs = model.graph.output
    print(f"\nOutputs ({len(outputs)}):")
    for out in outputs:
        name = out.name
        shape = [dim.dim_value if dim.dim_value > 0 else "dynamic"
                 for dim in out.type.tensor_type.shape.dim]
        print(f"  - {name}: {shape}")

    # Validate expected structure
    if len(inputs) < 2:
        raise ValueError("Model should have at least 2 inputs (input_ids, attention_mask)")

    if len(outputs) < 2:
        raise ValueError("Model should have at least 2 outputs (intent_logits, slot_logits)")

    print("\n✓ Model structure is valid")

    return {
        'inputs': [inp.name for inp in inputs],
        'outputs': [out.name for out in outputs],
        'input_shapes': [[dim.dim_value for dim in inp.type.tensor_type.shape.dim] for inp in inputs],
        'output_shapes': [[dim.dim_value for dim in out.type.tensor_type.shape.dim] for out in outputs],
    }


def quantize_model(model_path: str, output_path: str):
    """
    Quantize ONNX model to INT8.

    Args:
        model_path: Path to input ONNX model
        output_path: Path to save quantized model
    """
    print(f"\nQuantizing model to INT8...")

    try:
        from onnxruntime.quantization import quantize_dynamic, QuantType

        quantize_dynamic(
            model_input=model_path,
            model_output=output_path,
            weight_type=QuantType.QInt8
        )

        print(f"✓ Quantized model saved to: {output_path}")

        # Print size comparison
        original_size = os.path.getsize(model_path) / (1024 * 1024)
        quantized_size = os.path.getsize(output_path) / (1024 * 1024)
        compression = (1 - quantized_size / original_size) * 100

        print(f"  Original size: {original_size:.2f} MB")
        print(f"  Quantized size: {quantized_size:.2f} MB")
        print(f"  Compression: {compression:.1f}%")

    except ImportError:
        print("⚠ Warning: onnxruntime not installed, skipping quantization")
        print("  Install with: pip install onnxruntime")
        shutil.copy(model_path, output_path)


def copy_file(src: str, dst_dir: str, filename: str):
    """Copy a file to destination directory."""
    if not os.path.exists(src):
        raise FileNotFoundError(f"File not found: {src}")

    dst_path = os.path.join(dst_dir, filename)
    shutil.copy(src, dst_path)
    print(f"✓ Copied: {filename}")


def create_config(output_dir: str, max_seq_len: int):
    """Create android_config.json."""
    config = {
        "max_seq_len": max_seq_len
    }

    config_path = os.path.join(output_dir, "android_config.json")
    with open(config_path, 'w', encoding='utf-8') as f:
        json.dump(config, f, indent=2, ensure_ascii=False)

    print(f"✓ Created: android_config.json")


def validate_labels(label_file: str, label_type: str):
    """Validate label file format."""
    with open(label_file, 'r', encoding='utf-8') as f:
        labels = [line.strip() for line in f if line.strip()]

    print(f"\n{label_type} ({len(labels)} labels):")
    for i, label in enumerate(labels[:5]):  # Show first 5
        print(f"  {i}: {label}")
    if len(labels) > 5:
        print(f"  ... ({len(labels) - 5} more)")

    return labels


def main():
    parser = argparse.ArgumentParser(
        description='Prepare ONNX model for Intent Recognition',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    parser.add_argument('--model_path', type=str, required=True,
                        help='Path to ONNX model file')
    parser.add_argument('--output_dir', type=str, required=True,
                        help='Output directory for prepared model')
    parser.add_argument('--intent_labels', type=str, required=True,
                        help='Path to intent labels file')
    parser.add_argument('--slot_labels', type=str, required=True,
                        help='Path to slot labels file')
    parser.add_argument('--vocab', type=str, required=True,
                        help='Path to vocabulary file')
    parser.add_argument('--quantize', action='store_true',
                        help='Quantize model to INT8')
    parser.add_argument('--max_seq_len', type=int, default=64,
                        help='Maximum sequence length (default: 64)')

    args = parser.parse_args()

    print("=" * 60)
    print("Intent Recognition Model Preparation")
    print("=" * 60)

    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)
    print(f"\nOutput directory: {args.output_dir}")

    # Validate model
    try:
        model_info = validate_model(args.model_path)
    except Exception as e:
        print(f"\n✗ Error validating model: {e}")
        return 1

    # Validate label files
    try:
        print("\n" + "-" * 60)
        intent_labels = validate_labels(args.intent_labels, "Intent Labels")

        print("\n" + "-" * 60)
        slot_labels = validate_labels(args.slot_labels, "Slot Labels")

        print("\n" + "-" * 60)
        print(f"Vocabulary: {args.vocab}")
        with open(args.vocab, 'r', encoding='utf-8') as f:
            vocab_size = sum(1 for _ in f)
        print(f"  Vocabulary size: {vocab_size}")

    except Exception as e:
        print(f"\n✗ Error validating labels: {e}")
        return 1

    # Copy or quantize model
    print("\n" + "-" * 60)
    print("Preparing model files...")
    print("-" * 60)

    model_filename = "joint_model_quantized.onnx" if args.quantize else "joint_model.onnx"
    model_output_path = os.path.join(args.output_dir, model_filename)

    if args.quantize:
        quantize_model(args.model_path, model_output_path)
    else:
        copy_file(args.model_path, args.output_dir, model_filename)

    # Copy label files
    copy_file(args.intent_labels, args.output_dir, "intent_label.txt")
    copy_file(args.slot_labels, args.output_dir, "slot_label.txt")
    copy_file(args.vocab, args.output_dir, "vocab.txt")

    # Create config
    create_config(args.output_dir, args.max_seq_len)

    # Summary
    print("\n" + "=" * 60)
    print("✓ Model preparation complete!")
    print("=" * 60)
    print(f"\nModel directory: {args.output_dir}")
    print("\nFiles created:")
    print(f"  - {model_filename}")
    print("  - intent_label.txt")
    print("  - slot_label.txt")
    print("  - vocab.txt")
    print("  - android_config.json")

    print("\nNext steps:")
    print("  1. Build the C++ example:")
    print("     cd examples/intent-recognition/build")
    print("     cmake .. && make")
    print("\n  2. Run the example:")
    print(f"     ./intent-recognition --model_dir {args.output_dir}")
    print("\n  3. Or try interactive mode:")
    print(f"     ./intent-recognition --model_dir {args.output_dir} --interactive")

    return 0


if __name__ == "__main__":
    exit(main())
